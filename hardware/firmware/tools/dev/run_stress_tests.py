#!/usr/bin/env python3
"""Run Story V2 stress loop over serial for a fixed duration."""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
import time
from collections import deque
from datetime import datetime
from pathlib import Path
from threading import Event, Lock, Thread
from typing import Deque, Optional

try:
    import serial
except ImportError:
    print("pip install pyserial", file=sys.stderr)
    sys.exit(2)

DEFAULT_SCENARIOS = ["DEFAULT", "EXPRESS", "EXPRESS_DONE", "SPECTRE"]
FATAL_MARKERS = ("PANIC", "Guru Meditation", "ASSERT", "ABORT", "REBOOT", "rst:", "watchdog")
DONE_MARKERS = ("STORY_ENGINE_DONE", "step=STEP_DONE", "step: done")
WS_STATUS_TYPE = "status"

try:
    import websocket  # type: ignore
except Exception:
    websocket = None


def ts(msg: str) -> str:
    stamp = datetime.now().strftime("%H:%M:%S")
    return f"[{stamp}] {msg}"


def log_line(fp, msg: str) -> None:
    line = ts(msg)
    print(line, flush=True)
    fp.write(line + "\n")
    fp.flush()


def resolve_port(allow_no_hw: bool) -> Optional[str]:
    cmd = [
        sys.executable,
        "tools/test/resolve_ports.py",
        "--auto-ports",
        "--need-esp32",
    ]
    if allow_no_hw:
        cmd.append("--allow-no-hardware")
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
    if not done:
        log_line(log_fp, f"WARN scenario {scenario} did not complete")
        return False
    return True


def main() -> int:
    parser = argparse.ArgumentParser(description="Story V2 4h stress test")
    parser.add_argument("--port", help="ESP32 serial port")
    parser.add_argument("--baud", type=int, default=115200, help="Serial baud")
    parser.add_argument("--hours", type=float, default=4.0, help="Duration in hours")
    parser.add_argument("--scenario-duration", type=float, default=15.0, help="Seconds per scenario")
    parser.add_argument("--log", default="", help="Log output path")
    parser.add_argument("--allow-no-hardware", action="store_true")
    parser.add_argument("--ws-url", default="", help="Optional WebSocket URL for heap sampling")
    parser.add_argument("--heap-leak-bytes", type=int, default=5120, help="Heap drop threshold (bytes)")
    args = parser.parse_args()

    port = args.port or resolve_port(args.allow_no_hardware)
    if not port:
        msg = "No ESP32 port resolved"
        print(msg)
        return 0 if args.allow_no_hardware else 2

    stamp = datetime.utcnow().strftime("%Y%m%d-%H%M%S")
    if args.log:
        log_path = Path(args.log)
    else:
        log_path = Path("artifacts") / f"stress_test_4h_{stamp}.log"
    log_path.parent.mkdir(parents=True, exist_ok=True)

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
            return 2
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
                return 1

        if errors:
            log_line(log_fp, f"Errors: {errors}")
            return 1

        log_line(log_fp, "All iterations passed")
        return 0


if __name__ == "__main__":
    raise SystemExit(main())
