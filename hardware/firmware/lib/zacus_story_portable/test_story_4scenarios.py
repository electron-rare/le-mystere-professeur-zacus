#!/usr/bin/env python3
"""Run a 4-scenario Story serial loop using current Freenove serial commands."""

import argparse
import re
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

STATUS_RE = re.compile(
    r"\bSTATUS\b.*\bscenario=([A-Z0-9_]+)\b.*\bstep=([A-Z0-9_]+)\b.*\bscreen=([A-Z0-9_]+)\b"
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


def wait_for_sc_load_ack(ser: serial.Serial, scenario_id: str, timeout_s: float = 2.5) -> bool:
    deadline = time.time() + timeout_s
    while time.time() < deadline:
        for line in collect_lines(ser, 0.2):
            if f"ACK SC_LOAD id={scenario_id} ok=1" in line:
                return True
            if f"ACK SC_LOAD id={scenario_id} ok=0" in line or "ERR SC_LOAD" in line:
                return False
    return False


def fetch_status(ser: serial.Serial, timeout_s: float = 2.5) -> tuple[str, str, str] | None:
    deadline = time.time() + timeout_s
    while time.time() < deadline:
        send_cmd(ser, "STATUS")
        for line in collect_lines(ser, 0.6):
            match = STATUS_RE.search(line)
            if match:
                scenario_id, step_id, screen_id = match.groups()
                return scenario_id, step_id, screen_id
    return None


def run_scenario(ser: serial.Serial, name: str, scenario_id: str, log) -> bool:
    log.write(ts(f"Scenario {name} ({scenario_id})\n"))
    send_cmd(ser, f"SC_LOAD {scenario_id}")
    if not wait_for_sc_load_ack(ser, scenario_id, 3.0):
        log.write(ts(f"FAIL load {scenario_id}\n"))
        return False

    status = fetch_status(ser, timeout_s=3.0)
    if status is None:
        log.write(ts(f"FAIL status {scenario_id}\n"))
        return False
    status_scenario, status_step, status_screen = status
    if status_scenario != scenario_id:
        log.write(ts(f"FAIL status scenario mismatch expected={scenario_id} got={status_scenario}\n"))
        return False
    if status_screen in {"", "n/a"}:
        log.write(ts(f"FAIL status screen invalid step={status_step} screen={status_screen}\n"))
        return False

    log.write(
        ts(
            "STATUS "
            f"scenario={status_scenario} step={status_step} screen={status_screen}"
            "\n"
        )
    )
    # Low-cost behavior check: BTN_NEXT should always be accepted as a serial event command.
    send_cmd(ser, "SC_EVENT serial BTN_NEXT")
    if not wait_for_token(ser, "ACK SC_EVENT", 2.0):
        log.write(ts("FAIL SC_EVENT serial BTN_NEXT\n"))
        return False

    log.write(ts("OK\n"))
    return True


def main() -> int:
    parser = argparse.ArgumentParser(description="Story 4-scenario serial loop")
    parser.add_argument("--port", required=True, help="Serial port (ESP32)")
    parser.add_argument("--baud", type=int, default=115200, help="Serial baud")
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
        send_cmd(ser, "RESET")
        collect_lines(ser, 1.0)
        for name, scenario_id in SCENARIOS.items():
            if not run_scenario(ser, name, scenario_id, log):
                ok = False
                break

    ser.close()
    print(ts(f"Log: {log_path}"))
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
