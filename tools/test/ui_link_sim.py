#!/usr/bin/env python3
"""Minimal UI Link v2 simulator for UART testing."""

from __future__ import annotations

import argparse
import importlib.util
import platform
import string
import sys
import time
from pathlib import Path
from typing import Any


VALID_BTNS = {"NEXT", "PREV", "OK", "BACK", "VOL_UP", "VOL_DOWN", "MODE"}
VALID_ACTIONS = {"click", "long", "down", "up"}


def ensure_pyserial():
    try:
        import serial
        from serial.tools import list_ports
    except ImportError:
        print("[fail] missing dependency: pip install pyserial", file=sys.stderr)
        raise SystemExit(3)
    return serial, list_ports


def load_serial_smoke_module(repo_root: Path):
    module_path = repo_root / "hardware" / "firmware" / "tools" / "dev" / "serial_smoke.py"
    if not module_path.exists():
        print(f"[fail] missing serial smoke module: {module_path}", file=sys.stderr)
        raise SystemExit(2)

    spec = importlib.util.spec_from_file_location("zacus_serial_smoke", module_path)
    if spec is None or spec.loader is None:
        print("[fail] unable to load serial smoke module", file=sys.stderr)
        raise SystemExit(2)

    module = importlib.util.module_from_spec(spec)
    try:
        spec.loader.exec_module(module)
    except SystemExit as exc:
        raise SystemExit(int(exc.code) if isinstance(exc.code, int) else 2)
    return module


def crc8(data: bytes) -> int:
    crc = 0x00
    for byte in data:
        crc ^= byte
        for _ in range(8):
            if crc & 0x80:
                crc = ((crc << 1) ^ 0x07) & 0xFF
            else:
                crc = (crc << 1) & 0xFF
    return crc


def build_frame(msg_type: str, fields: dict[str, str]) -> str:
    payload = msg_type
    for key, value in fields.items():
        payload += f",{key}={value}"
    frame_crc = crc8(payload.encode("ascii", errors="ignore"))
    return f"{payload}*{frame_crc:02X}\n"


def parse_frame(line: str) -> tuple[dict[str, Any] | None, str | None]:
    text = line.strip()
    if not text:
        return None, "empty line"
    if "*" not in text:
        return None, "missing CRC separator (*)"

    payload, crc_text = text.rsplit("*", 1)
    if len(crc_text) != 2 or any(ch not in string.hexdigits for ch in crc_text):
        return None, "invalid CRC token"

    expected_crc = int(crc_text, 16)
    computed_crc = crc8(payload.encode("ascii", errors="ignore"))
    if expected_crc != computed_crc:
        return None, f"CRC mismatch expected={expected_crc:02X} computed={computed_crc:02X}"

    parts = payload.split(",")
    if not parts or not parts[0]:
        return None, "missing frame type"

    frame_type = parts[0]
    fields: dict[str, str] = {}
    for token in parts[1:]:
        if "=" not in token:
            continue
        key, value = token.split("=", 1)
        if key:
            fields[key] = value

    return {"type": frame_type, "fields": fields, "payload": payload}, None


def parse_script(raw_script: str) -> list[tuple[str, str]]:
    if not raw_script.strip():
        return []

    events: list[tuple[str, str]] = []
    for chunk in raw_script.split(","):
        token = chunk.strip()
        if not token:
            continue
        if ":" in token:
            btn, action = token.split(":", 1)
        else:
            btn, action = token, "click"
        btn = btn.strip().upper()
        action = action.strip().lower()
        if btn not in VALID_BTNS:
            raise ValueError(f"invalid button '{btn}'")
        if action not in VALID_ACTIONS:
            raise ValueError(f"invalid action '{action}'")
        events.append((btn, action))
    return events


def send_frame(ser, msg_type: str, fields: dict[str, str]) -> None:
    frame = build_frame(msg_type, fields)
    print(f"[tx] {frame.strip()}")
    ser.write(frame.encode("ascii", errors="ignore"))


def poll_frames(ser, window_s: float, auto_pong: bool) -> list[dict[str, Any]]:
    deadline = time.monotonic() + max(window_s, 0.05)
    frames: list[dict[str, Any]] = []
    while time.monotonic() < deadline:
        raw = ser.readline()
        if not raw:
            continue
        line = raw.decode("utf-8", errors="ignore").strip()
        if not line:
            continue
        print(f"[rx] {line}")

        frame, error = parse_frame(line)
        if frame is None:
            print(f"[warn] ignored frame: {error}")
            continue

        frames.append(frame)
        if auto_pong and frame["type"] == "PING":
            send_frame(ser, "PONG", {"ms": str(int(time.monotonic() * 1000))})
    return frames


def select_port(args, smoke_module, list_ports_module) -> tuple[str | None, str | None]:
    if args.port:
        found = smoke_module.find_port_by_name(args.port, args.wait_port)
        if found is None:
            return None, f"explicit port not found after {args.wait_port}s: {args.port}"
        return str(found.device), None

    prefer_cu = platform.system() == "Darwin"
    ports_map = smoke_module.load_ports_map()

    baseline = list(list_ports_module.comports())
    if baseline:
        print("Using existing ports (already connected).")
        detection = smoke_module.detect_roles(smoke_module.filter_detectable_ports(baseline), prefer_cu, ports_map)
        if "esp32" in detection:
            return str(detection["esp32"].get("device", "")), None
        if detection:
            first_role = sorted(detection)[0]
            return str(detection[first_role].get("device", "")), None
        return None, "failed to classify baseline ports"

    new_ports = smoke_module.wait_for_new_ports(set(), args.wait_port)
    if not new_ports:
        return None, f"no serial port detected after waiting {args.wait_port}s"

    detection = smoke_module.detect_roles(smoke_module.filter_detectable_ports(new_ports), prefer_cu, ports_map)
    if "esp32" in detection:
        return str(detection["esp32"].get("device", "")), None
    if detection:
        first_role = sorted(detection)[0]
        return str(detection[first_role].get("device", "")), None

    return None, "failed to classify detected ports"


def handshake(ser, args) -> bool:
    hello_fields = {
        "proto": "2",
        "ui_type": args.ui_type,
        "ui_id": args.ui_id,
        "fw": args.fw,
        "caps": args.caps,
    }
    send_frame(ser, "HELLO", hello_fields)

    ack_seen = False
    keyframe_seen = False
    deadline = time.monotonic() + 5.0
    while time.monotonic() < deadline:
        frames = poll_frames(ser, 0.4, auto_pong=True)
        for frame in frames:
            frame_type = frame.get("type")
            if frame_type == "ACK":
                ack_seen = True
            elif frame_type == "KEYFRAME":
                keyframe_seen = True
        if ack_seen and keyframe_seen:
            print("[pass] handshake complete (ACK + KEYFRAME)")
            return True

    if not ack_seen:
        print("[fail] handshake timeout: missing ACK", file=sys.stderr)
    if not keyframe_seen:
        print("[fail] handshake timeout: missing KEYFRAME", file=sys.stderr)
    return False


def run_script(ser, script_events: list[tuple[str, str]], script_delay_ms: int) -> None:
    for index, (btn, action) in enumerate(script_events, start=1):
        send_frame(
            ser,
            "BTN",
            {
                "id": btn,
                "action": action,
                "ts": str(int(time.monotonic() * 1000)),
            },
        )
        poll_frames(ser, max(script_delay_ms / 1000.0, 0.05), auto_pong=True)
        print(f"[pass] script event {index}/{len(script_events)} ({btn}:{action})")


def main() -> int:
    parser = argparse.ArgumentParser(description="UI Link v2 UART simulator")
    parser.add_argument("--port", help="Serial port to use (optional)")
    parser.add_argument("--baud", type=int, default=19200, help="UART baud rate (default 19200)")
    parser.add_argument("--wait-port", type=int, default=3, help="Seconds to wait for port detection")
    parser.add_argument("--allow-no-hardware", action="store_true", help="Return SKIP when no serial adapter is detected")
    parser.add_argument("--ui-type", default="TFT", help="HELLO ui_type field")
    parser.add_argument("--ui-id", default="zacus-cli", help="HELLO ui_id field")
    parser.add_argument("--fw", default="dev", help="HELLO fw field")
    parser.add_argument("--caps", default="btn:1;touch:0;display:cli", help="HELLO caps field")
    parser.add_argument("--script", default="", help="Comma-separated BTN script, e.g. NEXT:click,OK:long")
    parser.add_argument("--script-delay-ms", type=int, default=300, help="Delay between scripted BTN events")
    args = parser.parse_args()

    serial_module, list_ports_module = ensure_pyserial()

    repo_root = Path(__file__).resolve().parents[2]
    smoke_module = load_serial_smoke_module(repo_root)
    device, reason = select_port(args, smoke_module, list_ports_module)
    if not device:
        if args.allow_no_hardware:
            print(f"SKIP: {reason}")
            return 0
        print(f"[fail] {reason}", file=sys.stderr)
        return 2

    try:
        script_events = parse_script(args.script)
    except ValueError as exc:
        print(f"[fail] invalid --script: {exc}", file=sys.stderr)
        return 2

    print(f"[info] using device={device} baud={args.baud}")

    try:
        with serial_module.Serial(device, args.baud, timeout=0.2) as ser:
            time.sleep(0.2)
            ser.reset_input_buffer()

            if not handshake(ser, args):
                return 1

            if script_events:
                run_script(ser, script_events, args.script_delay_ms)
                print("[pass] scripted UI Link session complete")
                return 0

            print("[info] no script provided; live mode active (Ctrl+C to stop)")
            while True:
                poll_frames(ser, 0.5, auto_pong=True)
    except KeyboardInterrupt:
        print("\n[ok] stopped by user")
        return 0
    except Exception as exc:
        print(f"[fail] serial session error on {device}: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
