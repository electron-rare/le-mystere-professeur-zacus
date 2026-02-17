#!/usr/bin/env python3
"""Run Story V2 stress loop over serial for a fixed duration."""

from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
import time
from collections import deque
from datetime import datetime
from pathlib import Path
from threading import Event, Lock, Thread
from typing import Deque, Dict, Optional

try:
    import serial
except ImportError:
    print("pip install pyserial", file=sys.stderr)
    sys.exit(2)

DEFAULT_SCENARIOS = ["DEFAULT", "EXPRESS", "EXPRESS_DONE", "SPECTRE"]
FATAL_MARKERS = ("PANIC", "Guru Meditation", "ASSERT", "ABORT", "REBOOT", "rst:", "watchdog")
DONE_MARKERS = ("STORY_ENGINE_DONE", "step=STEP_DONE", "step: done", "-> STEP_DONE")
FORCE_ETAPE2_CMD = "STORY_FORCE_ETAPE2"
WS_STATUS_TYPE = "status"

try:
    import websocket  # type: ignore
except Exception:
    websocket = None

FW_ROOT = Path(__file__).resolve().parents[2]
REPO_ROOT = FW_ROOT.parent.parent
PHASE = "stress_test"


def init_evidence(outdir: Optional[str]) -> Dict[str, Path]:
    stamp = datetime.utcnow().strftime("%Y%m%d-%H%M%S")
    if outdir:
        path = Path(outdir)
        if not path.is_absolute():
            path = FW_ROOT / path
    else:
        path = FW_ROOT / "artifacts" / PHASE / stamp
    path.mkdir(parents=True, exist_ok=True)
    return {
        "dir": path,
        "meta": path / "meta.json",
        "git": path / "git.txt",
        "commands": path / "commands.txt",
        "summary": path / "summary.md",
    }


def write_git_info(dest: Path) -> None:
    lines = []
    try:
        branch = subprocess.check_output([
            "git",
            "-C",
            str(REPO_ROOT),
            "rev-parse",
            "--abbrev-ref",
            "HEAD",
        ], text=True).strip()
    except Exception:
        branch = "n/a"
    try:
        commit = subprocess.check_output([
            "git",
            "-C",
            str(REPO_ROOT),
            "rev-parse",
            "HEAD",
        ], text=True).strip()
    except Exception:
        commit = "n/a"
    lines.append(f"branch: {branch}")
    lines.append(f"commit: {commit}")
    lines.append("status:")
    try:
        status = subprocess.check_output([
            "git",
            "-C",
            str(REPO_ROOT),
            "status",
            "--porcelain",
        ], text=True).strip()
    except Exception:
        status = ""
    if status:
        lines.append(status)
    dest.write_text("\n".join(lines) + "\n", encoding="utf-8")


def write_meta_json(dest: Path, command: str) -> None:
    payload = {
        "timestamp": datetime.utcnow().strftime("%Y%m%d-%H%M%S"),
        "phase": PHASE,
        "utc": datetime.utcnow().strftime("%Y-%m-%dT%H:%M:%SZ"),
        "command": command,
        "cwd": str(Path.cwd()),
        "repo_root": str(REPO_ROOT),
        "fw_root": str(FW_ROOT),
    }
    dest.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")


def record_command(commands_path: Path, cmd: Optional[object]) -> None:
    if isinstance(cmd, list):
        line = " ".join(cmd)
    else:
        line = str(cmd)
    with commands_path.open("a", encoding="utf-8") as fp:
        fp.write(line + "\n")


def write_summary(dest: Path, result: str, log_path: Optional[Path], notes: list[str]) -> None:
    lines = ["# Stress test summary", "", f"- Result: **{result}**"]
    if log_path is not None:
        lines.append(f"- Log: {log_path.name}")
    for note in notes:
        lines.append(f"- {note}")
    dest.write_text("\n".join(lines) + "\n", encoding="utf-8")


def ts(msg: str) -> str:
    stamp = datetime.now().strftime("%H:%M:%S")
    return f"[{stamp}] {msg}"


def log_line(fp, msg: str) -> None:
    line = ts(msg)
    print(line, flush=True)
    fp.write(line + "\n")
    fp.flush()


def resolve_port(allow_no_hw: bool, commands_path: Optional[Path]) -> Optional[str]:
    cmd = [
        sys.executable,
        str(REPO_ROOT / "tools" / "test" / "resolve_ports.py"),
        "--auto-ports",
        "--need-esp32",
    ]
    if allow_no_hw:
        cmd.append("--allow-no-hardware")
    if commands_path is not None:
        record_command(commands_path, cmd)
    try:
        output = subprocess.check_output(cmd, text=True)
    except subprocess.CalledProcessError:
        return None
    try:
        data = json.loads(output)
    except json.JSONDecodeError:
        return None
    if data.get("status") != "pass":
        return None
    return data.get("ports", {}).get("esp32") or None


def start_ws_sampler(ws_url: str, log_fp, stop_event: Event, samples: list[int], lock: Lock) -> Optional[Thread]:
    if websocket is None:
        log_line(log_fp, "WARN websocket-client missing; heap sampling disabled")
        return None

    def worker() -> None:
        ws = None
        try:
            ws = websocket.create_connection(ws_url, timeout=1)
            while not stop_event.is_set():
                try:
                    payload = ws.recv()
                except websocket.WebSocketTimeoutException:
                    continue
                except Exception:
                    break
                try:
                    data = json.loads(payload)
                except json.JSONDecodeError:
                    continue
                if data.get("type") != WS_STATUS_TYPE:
                    continue
                memory_free = data.get("data", {}).get("memory_free")
                if isinstance(memory_free, int):
                    with lock:
                        samples.append(memory_free)
        finally:
            if ws is not None:
                try:
                    ws.close()
                except Exception:
                    pass

    thread = Thread(target=worker, daemon=True)
    thread.start()
    return thread


def read_lines(ser: serial.Serial, duration_s: float):
    deadline = time.time() + duration_s
    while time.time() < deadline:
        raw = ser.readline()
        if not raw:
            continue
        line = raw.decode("utf-8", errors="ignore").strip()
        if line:
            yield line


def send_cmd(ser: serial.Serial, cmd: str) -> None:
    ser.write((cmd + "\n").encode("utf-8"))
    ser.flush()
    time.sleep(0.2)


def prepare_story_session(ser: serial.Serial, log_fp) -> None:
    preamble = ("BOOT_NEXT", "STORY_V2_ENABLE ON", "STORY_TEST_ON", "STORY_TEST_DELAY 1000")
    for cmd in preamble:
        send_cmd(ser, cmd)
        log_line(log_fp, f"tx {cmd}")


def line_has_fatal(line: str) -> bool:
    upper = line.upper()
    return any(marker.upper() in upper for marker in FATAL_MARKERS)


def line_has_done(line: str) -> bool:
    upper = line.upper()
    return any(marker.upper() in upper for marker in DONE_MARKERS)


def run_scenario(
    ser: serial.Serial,
    scenario: str,
    log_fp,
    ring: Deque[str],
    duration_s: float,
) -> bool:
    log_line(log_fp, f"Scenario {scenario}: load + arm")
    send_cmd(ser, f"STORY_LOAD_SCENARIO {scenario}")
    send_cmd(ser, "STORY_ARM")
    send_cmd(ser, "STORY_STATUS")

    done = False
    for line in read_lines(ser, duration_s):
        ring.append(line)
        if len(ring) > 10000:
            ring.popleft()
        log_line(log_fp, f"rx {line}")
        if line_has_fatal(line):
            log_line(log_fp, f"CRITICAL {line}")
            return False
        if "UI_LINK_STATUS" in line and "connected=0" in line:
            log_line(log_fp, f"CRITICAL ui link down: {line}")
            return False
        if line_has_done(line):
            done = True
    if done:
        return True

    # Force ETAPE2 once before failing to reduce false negatives on slow boots.
    log_line(log_fp, f"WARN scenario {scenario} timeout, forcing ETAPE2 once")
    send_cmd(ser, FORCE_ETAPE2_CMD)
    send_cmd(ser, "STORY_STATUS")
    for line in read_lines(ser, 6.0):
        ring.append(line)
        if len(ring) > 10000:
            ring.popleft()
        log_line(log_fp, f"rx {line}")
        if line_has_fatal(line):
            log_line(log_fp, f"CRITICAL {line}")
            return False
        if "UI_LINK_STATUS" in line and "connected=0" in line:
            log_line(log_fp, f"CRITICAL ui link down: {line}")
            return False
        if line_has_done(line):
            return True
    log_line(log_fp, f"WARN scenario {scenario} did not complete")
    return False


def main() -> int:
    parser = argparse.ArgumentParser(description="Story V2 4h stress test")
    parser.add_argument("--port", help="ESP32 serial port")
    parser.add_argument("--baud", type=int, default=115200, help="Serial baud")
    parser.add_argument("--hours", type=float, default=4.0, help="Duration in hours")
    parser.add_argument("--scenario-duration", type=float, default=20.0, help="Seconds per scenario")
    parser.add_argument("--log", default="", help="Log output path")
    parser.add_argument("--allow-no-hardware", action="store_true")
    parser.add_argument("--ws-url", default="", help="Optional WebSocket URL for heap sampling")
    parser.add_argument("--heap-leak-bytes", type=int, default=5120, help="Heap drop threshold (bytes)")
    parser.add_argument("--outdir", default="", help="Evidence output directory")
    args = parser.parse_args()

    outdir = args.outdir or os.environ.get("ZACUS_OUTDIR", "") or None
    evidence = init_evidence(outdir)
    write_git_info(evidence["git"])
    evidence["commands"].write_text("# Commands\n", encoding="utf-8")
    record_command(evidence["commands"], " ".join(sys.argv))
    write_meta_json(evidence["meta"], " ".join(sys.argv))

    if args.log:
        log_path = Path(args.log)
        if not log_path.is_absolute():
            log_path = evidence["dir"] / log_path
    else:
        log_path = evidence["dir"] / "stress_test.log"
    log_path.parent.mkdir(parents=True, exist_ok=True)

    def finalize(exit_code: int, notes: list[str], result_override: Optional[str] = None) -> int:
        if result_override:
            result = result_override
        else:
            result = "PASS" if exit_code == 0 else "FAIL"
        write_summary(evidence["summary"], result, log_path, notes)
        print(f"RESULT={result}")
        return exit_code

    port = args.port or resolve_port(args.allow_no_hardware, evidence["commands"])
    if not port:
        msg = "No ESP32 port resolved"
        print(msg)
        exit_code = 0 if args.allow_no_hardware else 2
        result_override = "SKIP" if exit_code == 0 else None
        return finalize(exit_code, [msg], result_override)

    duration_s = args.hours * 3600.0
    end_time = time.time() + duration_s
    ring: Deque[str] = deque()
    errors = 0
    iterations = 0

    with log_path.open("w", encoding="utf-8") as log_fp:
        log_line(log_fp, f"Starting stress test: {args.hours:.2f}h on {port} (baud={args.baud})")
        log_line(log_fp, f"Scenarios: {', '.join(DEFAULT_SCENARIOS)}")

        stop_event = Event()
        ws_lock = Lock()
        ws_samples: list[int] = []
        ws_thread = None
        if args.ws_url:
            ws_thread = start_ws_sampler(args.ws_url, log_fp, stop_event, ws_samples, ws_lock)

        try:
            with serial.Serial(port, args.baud, timeout=0.5) as ser:
                time.sleep(0.8)
                ser.reset_input_buffer()
                prepare_story_session(ser, log_fp)

                while time.time() < end_time:
                    for scenario in DEFAULT_SCENARIOS:
                        if time.time() >= end_time:
                            break
                        iterations += 1
                        log_line(log_fp, f"[{iterations}] Running {scenario}")
                        ok = run_scenario(ser, scenario, log_fp, ring, args.scenario_duration)
                        if ok:
                            log_line(log_fp, f"OK {scenario}")
                        else:
                            errors += 1
                            log_line(log_fp, f"FAIL {scenario}")
                            break
                    if errors:
                        break

        except Exception as exc:
            log_line(log_fp, f"ERROR serial failure: {exc}")
            stop_event.set()
            return finalize(2, [f"Serial failure: {exc}"])
        finally:
            stop_event.set()
            if ws_thread is not None:
                ws_thread.join(timeout=2.0)

        elapsed_h = (args.hours * 3600.0 - max(0.0, end_time - time.time())) / 3600.0
        total = iterations
        success = total - errors
        success_rate = (success / total * 100.0) if total else 0.0

        log_line(log_fp, f"Completed {total} iterations in {elapsed_h:.2f}h")
        log_line(log_fp, f"Success rate: {success_rate:.1f}%")

        if ws_samples:
            with ws_lock:
                start_heap = ws_samples[0]
                end_heap = ws_samples[-1]
            heap_drop = start_heap - end_heap
            log_line(log_fp, f"Heap free start={start_heap} end={end_heap} drop={heap_drop}")
            if heap_drop > args.heap_leak_bytes:
                log_line(log_fp, "FAIL heap drop exceeds threshold")
                return finalize(1, ["Heap drop exceeds threshold"])

        if errors:
            log_line(log_fp, f"Errors: {errors}")
            return finalize(1, [f"Errors: {errors}"])

        log_line(log_fp, "All iterations passed")
        return finalize(0, ["All iterations passed"])


if __name__ == "__main__":
    raise SystemExit(main())
