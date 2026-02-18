#!/usr/bin/env python3
"""Run a 4-scenario Story V3 serial loop using StoryPortableRuntime commands."""

import argparse
import json
import sys
import time
from collections import OrderedDict
from datetime import datetime
from pathlib import Path

try:
    import serial
except ImportError:
    print("pip install pyserial", file=sys.stderr)
    sys.exit(2)

SCENARIOS = OrderedDict(
    [
        ("DEFAULT", "DEFAULT"),
        ("EXPRESS", "EXAMPLE_UNLOCK_EXPRESS"),
        ("EXPRESS_DONE", "EXEMPLE_UNLOCK_EXPRESS_DONE"),
        ("SPECTRE", "SPECTRE_RADIO_LAB"),
    ]
)


def ts(msg: str) -> str:
    now = datetime.now().strftime("%H:%M:%S")
    return f"[{now}] {msg}"


def open_serial(port: str, baud: int) -> serial.Serial:
    ser = serial.Serial(port, baud, timeout=1.0)
    time.sleep(0.5)
    ser.flushInput()
    ser.flushOutput()
    return ser


def send_cmd(ser: serial.Serial, cmd: str) -> None:
    ser.write((cmd + "\n").encode("utf-8"))
    ser.flush()
    time.sleep(0.2)


def send_json_cmd(ser: serial.Serial, cmd: str, data: dict | None = None) -> None:
    payload = {"cmd": cmd}
    if data:
        payload["data"] = data
    send_cmd(ser, json.dumps(payload, separators=(",", ":")))


def collect_lines(ser: serial.Serial, duration_s: float = 2.0) -> list[str]:
    lines = []
    deadline = time.time() + duration_s
    while time.time() < deadline:
        raw = ser.readline()
        if not raw:
            continue
        line = raw.decode("utf-8", errors="ignore").strip()
        if line:
            lines.append(line)
    return lines


def wait_for_token(ser: serial.Serial, token: str, timeout_s: float = 2.0) -> bool:
    deadline = time.time() + timeout_s
    while time.time() < deadline:
        for line in collect_lines(ser, 0.2):
            if token in line:
                return True
    return False


def wait_for_json_ok(ser: serial.Serial, timeout_s: float = 2.0) -> bool:
    deadline = time.time() + timeout_s
    while time.time() < deadline:
        for line in collect_lines(ser, 0.2):
            if '"ok":true' in line and '"code":"ok"' in line:
                return True
    return False


def run_scenario(ser: serial.Serial, name: str, scenario_id: str, force_step: str, log) -> bool:
    log.write(ts(f"Scenario {name} ({scenario_id})\n"))
    send_json_cmd(ser, "story.load", {"scenario": scenario_id})
    if not wait_for_json_ok(ser, 2.0):
        log.write(ts(f"FAIL load {scenario_id}\n"))
        return False

    send_cmd(ser, "STORY_ARM")
    send_cmd(ser, f"STORY_FORCE_STEP {force_step}")
    ok = wait_for_token(ser, "STORY_FORCE_STEP_OK", 2.0)
    if not ok:
        log.write(ts(f"FAIL force step {force_step}\n"))
        return False

    send_json_cmd(ser, "story.status")
    lines = collect_lines(ser, 1.0)
    for line in lines:
        if line.startswith("{"):
            log.write(ts(line + "\n"))
    log.write(ts("OK\n"))
    return True


def main() -> int:
    parser = argparse.ArgumentParser(description="Story V3 4-scenario serial loop")
    parser.add_argument("--port", required=True, help="Serial port (ESP32)")
    parser.add_argument("--baud", type=int, default=115200, help="Serial baud")
    parser.add_argument("--force-step", default="STEP_WAIT_UNLOCK", help="Step id for STORY_FORCE_STEP")
    parser.add_argument("--log", default="", help="Optional log path")
    args = parser.parse_args()

    log_path = args.log
    if not log_path:
        stamp = datetime.now().strftime("%Y%m%d-%H%M%S")
        log_path = f"artifacts/rc_live/test_4scenarios_{stamp}.log"
    Path(log_path).parent.mkdir(parents=True, exist_ok=True)

    try:
        ser = open_serial(args.port, args.baud)
    except Exception as exc:
        print(ts(f"ERROR cannot open serial: {exc}"))
        return 2

    ok = True
    with open(log_path, "w", encoding="utf-8") as log:
        for name, scenario_id in SCENARIOS.items():
            if not run_scenario(ser, name, scenario_id, args.force_step, log):
                ok = False
                break

    ser.close()
    print(ts(f"Log: {log_path}"))
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
