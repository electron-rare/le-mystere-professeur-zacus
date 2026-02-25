#!/usr/bin/env python3
"""Capture serial logs during a real user playtest."""

from __future__ import annotations

import argparse
import time

import serial


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True, help="Serial port (ex: /dev/cu.usbmodem5AB90753301)")
    parser.add_argument("--baud", type=int, default=115200, help="Serial baud rate")
    parser.add_argument("--duration", type=int, default=1200, help="Capture duration in seconds")
    parser.add_argument("--log", required=True, help="Output log file path")
    args = parser.parse_args()

    end_at = time.time() + max(1, args.duration)
    with open(args.log, "w", encoding="utf-8") as out:
        out.write(
            f"# user live test log port={args.port} baud={args.baud} "
            f"start={time.strftime('%Y-%m-%d %H:%M:%S')}\n"
        )
        out.flush()
        with serial.Serial(args.port, args.baud, timeout=0.25) as ser:
            while time.time() < end_at:
                line = ser.readline().decode("utf-8", errors="ignore").rstrip("\n")
                if not line:
                    continue
                out.write(f"[{time.strftime('%H:%M:%S')}] {line}\n")
                out.flush()

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
