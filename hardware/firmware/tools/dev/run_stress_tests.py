#!/usr/bin/env python3
"""Run Story V3 stress loop over serial for a fixed duration."""

from __future__ import annotations

import argparse
import json
import os
import re
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

STANDARD_SCENARIOS = ["DEFAULT", "EXPRESS", "EXPRESS_DONE", "SPECTRE"]
COMBINED_LA_SCENARIOS = ["DEFAULT"]
DEFAULT_SCENARIOS = list(STANDARD_SCENARIOS)
FATAL_MARKERS = ("PANIC", "Guru Meditation", "ASSERT", "ABORT", "REBOOT", "rst:", "watchdog")
DONE_MARKERS = (
    "STORY_ENGINE_DONE",
    "step=STEP_DONE",
    "step=STEP_WIN",
    "step: done",
    "-> STEP_DONE",
    "-> STEP_WIN",
    '"step":"STEP_DONE"',
    '"step":"STEP_WIN"',
)
FORCE_ETAPE2_CMD = "STORY_FORCE_ETAPE2"
WS_STATUS_TYPE = "status"
PROTOCOL_JSON_V3 = "json_v3"
PROTOCOL_LEGACY_SC = "legacy_sc"
RE_SCENARIO_ITEM = re.compile(r"SC_LIST_ITEM\s+idx=\d+\s+id=([A-Za-z0-9_\-]+)")

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


def append_rx(log_fp, ring: Deque[str], line: str) -> None:
    ring.append(line)
    if len(ring) > 10000:
        ring.popleft()
    log_line(log_fp, f"rx {line}")


def send_cmd(ser: serial.Serial, cmd: str) -> None:
    ser.write((cmd + "\n").encode("utf-8"))
    ser.flush()
    time.sleep(0.2)


def send_json_cmd(ser: serial.Serial, cmd: str, data: Optional[dict[str, object]] = None) -> None:
    payload = {"cmd": cmd}
    if data:
        payload["data"] = data
    send_cmd(ser, json.dumps(payload, separators=(",", ":")))


def line_has_fatal(line: str) -> bool:
    upper = line.upper()
    return any(marker.upper() in upper for marker in FATAL_MARKERS)


def line_has_done(line: str) -> bool:
    upper = line.upper()
    return any(marker.upper() in upper for marker in DONE_MARKERS)


def line_has_critical(line: str, require_ui_link: bool = True) -> bool:
    if line_has_fatal(line):
        return True
    if require_ui_link and "UI_LINK_STATUS" in line and "connected=0" in line:
        return True
    return False


def scan_for_patterns(
    ser: serial.Serial,
    duration_s: float,
    log_fp,
    ring: Deque[str],
    patterns: list[re.Pattern[str]],
    require_ui_link: bool,
) -> tuple[bool, str, bool]:
    for line in read_lines(ser, duration_s):
        append_rx(log_fp, ring, line)
        if line_has_critical(line, require_ui_link):
            return False, line, True
        for pattern in patterns:
            if pattern.search(line):
                return True, line, False
    return False, "timeout", False


def detect_story_protocol(ser: serial.Serial, log_fp, ring: Deque[str], require_ui_link: bool) -> str:
    log_line(log_fp, "Probing story protocol")
    send_json_cmd(ser, "story.status")
    saw_unknown_json = False
    saw_json_status = False
    for line in read_lines(ser, 1.5):
        append_rx(log_fp, ring, line)
        if line_has_critical(line, require_ui_link):
            return PROTOCOL_LEGACY_SC
        if 'UNKNOWN {"cmd":"story.status"}' in line:
            saw_unknown_json = True
        if '"running"' in line or '"step"' in line or '"ok"' in line:
            saw_json_status = True
    if saw_json_status and not saw_unknown_json:
        return PROTOCOL_JSON_V3

    send_cmd(ser, "HELP")
    saw_sc_commands = False
    for line in read_lines(ser, 1.5):
        append_rx(log_fp, ring, line)
        if line_has_critical(line, require_ui_link):
            return PROTOCOL_LEGACY_SC
        if "SC_LOAD" in line or "SC_LIST" in line:
            saw_sc_commands = True
    if saw_unknown_json or saw_sc_commands:
        return PROTOCOL_LEGACY_SC
    return PROTOCOL_JSON_V3


def list_legacy_scenarios(ser: serial.Serial, log_fp, ring: Deque[str], require_ui_link: bool) -> list[str]:
    send_cmd(ser, "SC_LIST")
    scenarios: list[str] = []
    for line in read_lines(ser, 2.0):
        append_rx(log_fp, ring, line)
        if line_has_critical(line, require_ui_link):
            break
        match = RE_SCENARIO_ITEM.search(line)
        if match:
            scenarios.append(match.group(1))
    if scenarios:
        return scenarios
    return ["DEFAULT"]


def prepare_story_session_for_protocol(ser: serial.Serial, log_fp, protocol: str) -> None:
    preamble = ["BOOT_NEXT"]
    if protocol == PROTOCOL_JSON_V3:
        preamble.extend(["STORY_TEST_ON", "STORY_TEST_DELAY 1000"])
    for cmd in preamble:
        send_cmd(ser, cmd)
        log_line(log_fp, f"tx {cmd}")


def run_scenario_json_v3(
    ser: serial.Serial,
    scenario: str,
    log_fp,
    ring: Deque[str],
    duration_s: float,
    require_ui_link: bool,
) -> bool:
    log_line(log_fp, f"Scenario {scenario}: load + arm")
    send_json_cmd(ser, "story.load", {"scenario": scenario})
    send_cmd(ser, "STORY_ARM")
    send_json_cmd(ser, "story.status")

    done = False
    for line in read_lines(ser, duration_s):
        append_rx(log_fp, ring, line)
        if line_has_critical(line, require_ui_link):
            log_line(log_fp, f"CRITICAL {line}")
            return False
        if line_has_done(line):
            done = True
    if done:
        return True

    # Force ETAPE2 once before failing to reduce false negatives on slow boots.
    log_line(log_fp, f"WARN scenario {scenario} timeout, forcing ETAPE2 once")
    send_cmd(ser, FORCE_ETAPE2_CMD)
    send_json_cmd(ser, "story.status")
    for line in read_lines(ser, 6.0):
        append_rx(log_fp, ring, line)
        if line_has_critical(line, require_ui_link):
            log_line(log_fp, f"CRITICAL {line}")
            return False
        if line_has_done(line):
            return True
    log_line(log_fp, f"WARN scenario {scenario} did not complete")
    return False


def run_scenario_legacy_sc(
    ser: serial.Serial,
    scenario: str,
    log_fp,
    ring: Deque[str],
    duration_s: float,
    require_ui_link: bool,
) -> bool:
    del duration_s  # Legacy path actively drives transitions, timeout is command-based.
    log_line(log_fp, f"Scenario {scenario}: legacy SC_LOAD + transitions")

    send_cmd(ser, f"SC_LOAD {scenario}")
    ok, info, critical = scan_for_patterns(
        ser,
        3.0,
        log_fp,
        ring,
        [
            re.compile(rf"ACK SC_LOAD id={re.escape(scenario)} ok=1", re.IGNORECASE),
            re.compile(r"ACK SC_LOAD id=[A-Za-z0-9_\-]+ ok=1", re.IGNORECASE),
        ],
        require_ui_link,
    )
    if not ok:
        if critical:
            log_line(log_fp, f"CRITICAL {info}")
        else:
            log_line(log_fp, f"WARN scenario {scenario} load failed: {info}")
        return False

    # Legacy Freenove story flow expects SC_EVENT serial BTN_NEXT transitions.
    # Keep NEXT as fallback for backward compatibility.
    drive_steps = [
        ("UNLOCK", "UNLOCK"),
        ("SC_EVENT serial BTN_NEXT", "NEXT"),
        ("SC_EVENT serial BTN_NEXT", "NEXT"),
        ("SC_EVENT serial BTN_NEXT", "NEXT"),
    ]
    for primary_cmd, fallback_cmd in drive_steps:
        progressed = False
        for cmd in (primary_cmd, fallback_cmd):
            send_cmd(ser, cmd)
            ok, info, critical = scan_for_patterns(
                ser,
                2.4,
                log_fp,
                ring,
                [
                    re.compile(r"ACK\s+SC_EVENT\s+ok=1", re.IGNORECASE),
                    re.compile(r"ACK\s+UNLOCK(\s+ok=1)?", re.IGNORECASE),
                    re.compile(r"ACK\s+NEXT(\s+ok=1)?", re.IGNORECASE),
                    re.compile(r"\[SCENARIO\]\s+transition", re.IGNORECASE),
                    re.compile(r"step=STEP_(DONE|WIN)", re.IGNORECASE),
                    re.compile(r"STEP_(DONE|WIN)", re.IGNORECASE),
                ],
                require_ui_link,
            )
            if not ok and critical:
                log_line(log_fp, f"CRITICAL {info}")
                return False
            if line_has_done(info):
                return True
            if ok:
                progressed = True
                break
        if not progressed:
            log_line(log_fp, f"WARN scenario {scenario} no progression after `{primary_cmd}`")
            return False

    send_cmd(ser, "STATUS")
    ok, info, critical = scan_for_patterns(
        ser,
        2.0,
        log_fp,
        ring,
        [re.compile(r"step=STEP_(DONE|WIN)", re.IGNORECASE), re.compile(r"STEP_(DONE|WIN)", re.IGNORECASE)],
        require_ui_link,
    )
    if ok:
        return True
    if critical:
        log_line(log_fp, f"CRITICAL {info}")
    else:
        log_line(log_fp, f"WARN scenario {scenario} did not reach STEP_DONE")
    return False


def run_scenario(
    ser: serial.Serial,
    scenario: str,
    log_fp,
    ring: Deque[str],
    duration_s: float,
    protocol: str,
    require_ui_link: bool,
) -> bool:
    if protocol == PROTOCOL_LEGACY_SC:
        return run_scenario_legacy_sc(ser, scenario, log_fp, ring, duration_s, require_ui_link)
    return run_scenario_json_v3(ser, scenario, log_fp, ring, duration_s, require_ui_link)


def main() -> int:
    parser = argparse.ArgumentParser(description="Story V3 4h stress test")
    parser.add_argument("--port", help="ESP32 serial port")
    parser.add_argument("--baud", type=int, default=115200, help="Serial baud")
    parser.add_argument("--hours", type=float, default=4.0, help="Duration in hours")
    parser.add_argument("--scenario-duration", type=float, default=20.0, help="Seconds per scenario")
    parser.add_argument(
        "--scenarios",
        default="",
        help="Optional comma-separated scenario IDs. Defaults depend on protocol.",
    )
    parser.add_argument(
        "--scenario-profile",
        choices=["auto", "standard", "combined_la"],
        default="auto",
        help="Scenario profile. auto selects combined_la when ZACUS_ENV includes freenove_esp32s3.",
    )
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

    requested_scenarios = [item.strip() for item in args.scenarios.split(",") if item.strip()]
    scenario_profile = args.scenario_profile
    if scenario_profile == "auto":
        if "freenove_esp32s3" in os.environ.get("ZACUS_ENV", ""):
            scenario_profile = "combined_la"
        else:
            scenario_profile = "standard"
    require_ui_link = scenario_profile != "combined_la"

    with log_path.open("w", encoding="utf-8") as log_fp:
        log_line(log_fp, f"Starting stress test: {args.hours:.2f}h on {port} (baud={args.baud})")
        log_line(log_fp, f"Scenario profile: {scenario_profile}")
        if not require_ui_link:
            log_line(log_fp, "UI link gate: skipped (combined board profile)")

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
                protocol = detect_story_protocol(ser, log_fp, ring, require_ui_link)
                log_line(log_fp, f"Story protocol: {protocol}")
                if protocol == PROTOCOL_LEGACY_SC:
                    available_scenarios = list_legacy_scenarios(ser, log_fp, ring, require_ui_link)
                    if requested_scenarios:
                        scenarios = [s for s in requested_scenarios if s in available_scenarios]
                        if not scenarios:
                            scenarios = [available_scenarios[0]]
                    elif scenario_profile == "combined_la" and "DEFAULT" in available_scenarios:
                        scenarios = ["DEFAULT"]
                    elif "DEFAULT" in available_scenarios:
                        # Keep legacy runs deterministic: DEFAULT has stable transitions in single-board mode.
                        scenarios = ["DEFAULT"]
                    else:
                        scenarios = [available_scenarios[0]]
                else:
                    if requested_scenarios:
                        scenarios = requested_scenarios
                    elif scenario_profile == "combined_la":
                        scenarios = list(COMBINED_LA_SCENARIOS)
                    else:
                        scenarios = list(STANDARD_SCENARIOS)
                log_line(log_fp, f"Scenarios: {', '.join(scenarios)}")
                prepare_story_session_for_protocol(ser, log_fp, protocol)

                while time.time() < end_time:
                    for scenario in scenarios:
                        if time.time() >= end_time:
                            break
                        iterations += 1
                        log_line(log_fp, f"[{iterations}] Running {scenario}")
                        ok = run_scenario(
                            ser,
                            scenario,
                            log_fp,
                            ring,
                            args.scenario_duration,
                            protocol,
                            require_ui_link,
                        )
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
