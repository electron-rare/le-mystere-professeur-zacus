#!/usr/bin/env python3
"""Live monitor for pulse-dial hotline routes on A252."""

from __future__ import annotations

import argparse
import glob
import json
import sys
import time
from dataclasses import dataclass
from typing import Any, Dict, Optional

try:
    import serial  # type: ignore
except ImportError:  # pragma: no cover
    serial = None


@dataclass
class Snapshot:
    hook: str = ""
    dial_buffer: str = ""
    hotline_active: bool = False
    hotline_current_key: str = ""
    hotline_pending_restart: bool = False
    playing: bool = False


class SerialEndpoint:
    def __init__(self, port: str, baud: int, timeout_s: float = 0.5) -> None:
        if serial is None:
            raise RuntimeError("pyserial is required (pip install pyserial)")
        self.port = port
        self.baud = baud
        self.timeout_s = timeout_s
        self._ser: Optional[serial.Serial] = None

    def __enter__(self) -> "SerialEndpoint":
        self._ser = serial.Serial(self.port, self.baud, timeout=self.timeout_s)
        time.sleep(0.8)
        self._ser.reset_input_buffer()
        self._ser.reset_output_buffer()
        return self

    def __exit__(self, exc_type, exc, tb) -> None:
        if self._ser and self._ser.is_open:
            self._ser.close()

    def command(self, cmd: str, timeout_s: float = 3.0, expect: str = "any") -> Dict[str, Any]:
        if not self._ser or not self._ser.is_open:
            raise RuntimeError("serial port not open")
        self._ser.reset_input_buffer()
        self._ser.write((cmd + "\r\n").encode())
        self._ser.flush()

        deadline = time.time() + timeout_s
        last_line = ""
        while time.time() < deadline:
            raw = self._ser.readline()
            if not raw:
                continue
            line = raw.decode("utf-8", errors="ignore").strip()
            if not line:
                continue
            last_line = line
            if line and line[0] in ("{", "[") and line[-1] in ("}", "]"):
                if expect in {"any", "json"}:
                    try:
                        return json.loads(line)
                    except json.JSONDecodeError:
                        continue
                continue
            if line.startswith("OK ") or line.startswith("ERR "):
                if expect in {"any", "ack"}:
                    return {"ok": line.startswith("OK "), "line": line}
                continue
            if line == "PONG":
                if expect in {"any", "pong", "ack"}:
                    return {"ok": True, "result": "PONG"}
                continue
        raise RuntimeError(f"timeout on command '{cmd}' last='{last_line}'")

    def sync(self, retries: int = 6) -> None:
        last_error = ""
        for _ in range(retries):
            try:
                self.command("PING", timeout_s=2.0, expect="pong")
                return
            except Exception as exc:  # pragma: no cover - hardware timing
                last_error = str(exc)
                time.sleep(0.4)
        raise RuntimeError(f"serial sync failed: {last_error}")


def resolve_port(explicit_port: str | None) -> str:
    if explicit_port:
        return explicit_port
    for pattern in ("/dev/cu.usbserial*", "/dev/tty.usbserial*"):
        candidates = sorted(glob.glob(pattern))
        if candidates:
            return candidates[0]
    raise RuntimeError("No serial port found. Provide --port explicitly.")


def build_snapshot(status: Dict[str, Any], hotline: Dict[str, Any]) -> Snapshot:
    telephony = status.get("telephony", {}) if isinstance(status, dict) else {}
    audio = status.get("audio", {}) if isinstance(status, dict) else {}
    hotline_obj = hotline if isinstance(hotline, dict) else {}

    return Snapshot(
        hook=str(telephony.get("hook", "")),
        dial_buffer=str(telephony.get("dial_buffer", "")),
        hotline_active=bool(hotline_obj.get("active", telephony.get("hotline_active", False))),
        hotline_current_key=str(hotline_obj.get("current_key", telephony.get("hotline_current_key", ""))),
        hotline_pending_restart=bool(hotline_obj.get("pending_restart", False)),
        playing=bool(audio.get("playing", False)),
    )


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Live hotline monitor for pulse dialing 1/2/3 on A252")
    parser.add_argument("--port", default="", help="serial port (default: first /dev/*usbserial*)")
    parser.add_argument("--baud", type=int, default=115200, help="serial baudrate")
    parser.add_argument("--duration", type=float, default=180.0, help="monitoring window in seconds")
    parser.add_argument("--interval-ms", type=int, default=100, help="poll interval in milliseconds")
    parser.add_argument("--expect", default="1,2,3", help="expected hotline keys, comma-separated")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    expected_keys = [item.strip() for item in str(args.expect).split(",") if item.strip()]
    seen = {key: False for key in expected_keys}
    port = resolve_port(args.port.strip() or None)
    interval_s = max(0.05, float(args.interval_ms) / 1000.0)

    print(f"[monitor] port={port} baud={args.baud} duration={args.duration:.1f}s interval={interval_s:.3f}s")
    print(f"[monitor] expected hotline keys: {', '.join(expected_keys)}")

    last = Snapshot()
    start = time.monotonic()
    interrupted = False

    try:
        with SerialEndpoint(port, args.baud) as dev:
            dev.sync()
            print("[monitor] sync OK; start dialing (pulse) 1, then 2, then 3")

            while (time.monotonic() - start) < float(args.duration):
                status = dev.command("STATUS", expect="json", timeout_s=3.0)
                hotline = dev.command("HOTLINE_STATUS", expect="json", timeout_s=3.0)
                snap = build_snapshot(status, hotline)

                if snap.hotline_active and snap.hotline_current_key in seen:
                    seen[snap.hotline_current_key] = True

                if snap != last:
                    elapsed = time.monotonic() - start
                    print(
                        f"[{elapsed:7.3f}s] hook={snap.hook:<8} dial={snap.dial_buffer:<8} "
                        f"hotline_active={str(snap.hotline_active).lower():<5} "
                        f"key={snap.hotline_current_key:<3} pending_restart={str(snap.hotline_pending_restart).lower():<5} "
                        f"playing={str(snap.playing).lower():<5}"
                    )
                    last = snap

                if expected_keys and all(seen.values()):
                    print("[monitor] all expected hotline keys observed; stopping early")
                    break

                time.sleep(interval_s)
    except KeyboardInterrupt:
        interrupted = True
    except Exception as exc:
        print(f"[monitor] FAIL: {exc}")
        return 1

    missing = [key for key, ok in seen.items() if not ok]
    if missing:
        print(f"[monitor] FAIL missing hotline keys: {', '.join(missing)}")
        if interrupted:
            print("[monitor] interrupted by user before all keys were observed")
        return 1

    print("[monitor] PASS hotline keys observed:", ", ".join(expected_keys))
    return 0


if __name__ == "__main__":
    sys.exit(main())
