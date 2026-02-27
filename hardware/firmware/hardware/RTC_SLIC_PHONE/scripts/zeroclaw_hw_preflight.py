#!/usr/bin/env python3
"""Run ZeroClaw USB discovery/introspection before hardware actions."""

from __future__ import annotations

import argparse
import glob
import os
import shutil
import subprocess
import sys
from pathlib import Path


def resolve_bin(requested: str) -> str:
    if os.path.sep in requested:
        if not Path(requested).exists():
            raise FileNotFoundError(f"ZeroClaw binary not found: {requested}")
        return requested
    resolved = shutil.which(requested)
    if not resolved:
        raise FileNotFoundError(
            "ZeroClaw binary not found in PATH. Set --zeroclaw-bin explicitly."
        )
    return resolved


def default_ports() -> list[str]:
    patterns = [
        "/dev/cu.usbserial*",
        "/dev/tty.usbserial-*",
        "/dev/tty.SLAB_USBtoUART",
        "/dev/tty.usbmodem*",
    ]
    ports: list[str] = []
    for pattern in patterns:
        ports.extend(glob.glob(pattern))
    return sorted(set(ports))


def run_command(cmd: list[str]) -> int:
    print(f"$ {' '.join(cmd)}")
    proc = subprocess.run(cmd, check=False)
    return proc.returncode


def main() -> int:
    parser = argparse.ArgumentParser(
        description="ZeroClaw preflight for RTC hardware sessions."
    )
    parser.add_argument(
        "--zeroclaw-bin",
        default=os.environ.get("ZEROCLAW_BIN", "zeroclaw"),
        help="Path/name of zeroclaw binary (default: zeroclaw in PATH).",
    )
    parser.add_argument(
        "--port",
        action="append",
        default=[],
        help="Explicit serial port to introspect (repeatable).",
    )
    parser.add_argument(
        "--require-port",
        action="store_true",
        help="Fail if no serial port candidate is detected.",
    )
    args = parser.parse_args()

    try:
        zc_bin = resolve_bin(args.zeroclaw_bin)
    except FileNotFoundError as exc:
        print(f"[fail] {exc}", file=sys.stderr)
        return 2

    if run_command([zc_bin, "hardware", "discover"]) != 0:
        print("[fail] zeroclaw hardware discover failed", file=sys.stderr)
        return 3

    ports = args.port if args.port else default_ports()
    if not ports:
        message = "[warn] no candidate serial ports detected."
        if args.require_port:
            print(message, file=sys.stderr)
            return 4
        print(message)
        return 0

    failures = 0
    for port in ports:
        rc = run_command([zc_bin, "hardware", "introspect", port])
        if rc != 0:
            failures += 1

    if failures:
        print(f"[fail] introspection failed on {failures} port(s).", file=sys.stderr)
        return 5

    print("[ok] zeroclaw hardware preflight passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
