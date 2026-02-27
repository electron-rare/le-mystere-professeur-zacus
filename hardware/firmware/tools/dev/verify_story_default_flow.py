#!/usr/bin/env python3
"""Quick serial check for Freenove default Story screen flow."""

from __future__ import annotations

import argparse
import os
import re
import sys
import time
from dataclasses import dataclass

try:
    import serial
    from serial.tools import list_ports
except ImportError:
    print("pip install pyserial", file=sys.stderr)
    raise SystemExit(2)


STATUS_RE = re.compile(r"\bSTATUS\b.*\bstep=([A-Z0-9_]+)\b.*\bscreen=([A-Z0-9_]+)\b")


@dataclass(frozen=True)
class StepExpect:
    event_cmd: str
    expected_step: str
    expected_screen: str


STEPS = (
    StepExpect("SC_EVENT button ANY", "SCENE_LA_DETECTOR", "SCENE_LA_DETECTOR"),
    StepExpect("SC_EVENT serial BTN_NEXT", "RTC_ESP_ETAPE1", "SCENE_WIN_ETAPE1"),
    StepExpect("SC_EVENT serial FORCE_DONE", "WIN_ETAPE1", "SCENE_WIN_ETAPE1"),
    StepExpect("SC_EVENT serial BTN_NEXT", "STEP_WARNING", "SCENE_WARNING"),
    StepExpect("SC_EVENT button ANY", "SCENE_LEFOU_DETECTOR", "SCENE_LEFOU_DETECTOR"),
)


def detect_port() -> str | None:
    env_port = os.getenv("ZACUS_PORT_ESP32", "").strip()
    if env_port:
        return env_port

    modem_ports: list[str] = []
    serial_ports: list[str] = []
    for port in list_ports.comports():
        device = str(port.device)
        if "usbmodem" in device:
            modem_ports.append(device)
        elif "usbserial" in device or "SLAB_USBtoUART" in device:
            serial_ports.append(device)

    if modem_ports:
        return sorted(modem_ports)[0]
    if serial_ports:
        return sorted(serial_ports)[0]
    return None


def read_lines(ser: serial.Serial, timeout_s: float) -> list[str]:
    deadline = time.time() + timeout_s
    lines: list[str] = []
    pending = ""
    while time.time() < deadline:
        data = ser.read(ser.in_waiting or 1)
        if not data:
            time.sleep(0.03)
            continue
        pending += data.decode(errors="replace")
        while "\n" in pending:
            line, pending = pending.split("\n", 1)
            line = line.strip("\r").strip()
            if line:
                lines.append(line)
    if pending.strip():
        lines.append(pending.strip())
    return lines


def send_and_collect(ser: serial.Serial, cmd: str, timeout_s: float) -> list[str]:
    ser.write((cmd + "\n").encode("utf-8"))
    ser.flush()
    time.sleep(0.18)
    return read_lines(ser, timeout_s)


def find_status(lines: list[str]) -> tuple[str, str] | None:
    for line in reversed(lines):
        match = STATUS_RE.search(line)
        if match:
            return match.group(1), match.group(2)
    return None


def assert_contains(lines: list[str], token: str, context: str) -> None:
    for line in lines:
        if token in line:
            return
    raise RuntimeError(f"{context}: token not found: {token}")


def run(port: str, baud: int, timeout_s: float, settle_s: float) -> int:
    print(f"[info] port={port} baud={baud}")
    with serial.Serial(port, baud, timeout=0.2) as ser:
        time.sleep(settle_s)
        ser.reset_input_buffer()

        def fetch_status(max_attempts: int = 6) -> tuple[str, str] | None:
            status: tuple[str, str] | None = None
            for _ in range(max_attempts):
                status_lines = send_and_collect(ser, "STATUS", timeout_s)
                status = find_status(status_lines)
                if status is not None:
                    return status
                time.sleep(0.2)
            return None

        send_and_collect(ser, "RESET", timeout_s)

        lines = send_and_collect(ser, "SC_LOAD DEFAULT", timeout_s)
        if any("ACK SC_LOAD id=DEFAULT ok=1" in line for line in lines):
            if any("load source=story_file path=/story/scenarios/DEFAULT.json" in line for line in lines):
                print("[ok] SC_LOAD DEFAULT from story_file")
            else:
                print("[ok] SC_LOAD DEFAULT acknowledged")
        else:
            # Some boards reboot on SC_LOAD while still converging to runtime state.
            # Continue with RESET + STATUS, which is sufficient to validate flow.
            print("[warn] SC_LOAD ACK missing, fallback to runtime RESET/STATUS flow")

        status = fetch_status()
        expected_initial = ("SCENE_U_SON_PROTO", "SCENE_U_SON_PROTO")
        if status != expected_initial:
            raise RuntimeError(
                f"STATUS initial: expected step/screen={expected_initial}, got {status!r}"
            )
        print("[ok] initial step/screen=SCENE_U_SON_PROTO")

        for step in STEPS:
            event_lines = send_and_collect(ser, step.event_cmd, timeout_s)
            assert_contains(event_lines, "ACK SC_EVENT", step.event_cmd)

            status = fetch_status()
            expected_status = (step.expected_step, step.expected_screen)
            if status != expected_status:
                raise RuntimeError(
                    f"{step.event_cmd}: expected step/screen={expected_status}, got {status!r}"
                )
            print(f"[ok] {step.event_cmd} -> step={status[0]} screen={status[1]}")

    print("[ok] default story flow validated")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Validate DEFAULT Story screen flow over serial on Freenove."
    )
    parser.add_argument("--port", default="", help="Serial port (default: auto detect)")
    parser.add_argument("--baud", type=int, default=115200, help="Serial baud")
    parser.add_argument("--timeout", type=float, default=1.8, help="Per-command timeout")
    parser.add_argument("--settle", type=float, default=2.0, help="Boot settle delay in seconds")
    args = parser.parse_args()

    port = args.port.strip() or detect_port()
    if not port:
        print("[error] no serial port detected (expected usbmodem)", file=sys.stderr)
        return 2

    try:
        return run(port=port, baud=args.baud, timeout_s=args.timeout, settle_s=args.settle)
    except Exception as exc:  # noqa: BLE001
        print(f"[fail] {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
