#!/usr/bin/env python3
"""Provision FFat media files over serial for A252."""

from __future__ import annotations

import argparse
import base64
import time
from pathlib import Path

import serial  # type: ignore


def send_cmd(ser: serial.Serial, cmd: str, timeout_s: float = 8.0) -> str:
    ser.reset_input_buffer()
    ser.write((cmd + "\r\n").encode("utf-8"))
    ser.flush()
    deadline = time.time() + timeout_s
    last = ""
    while time.time() < deadline:
        line = ser.readline().decode("utf-8", errors="ignore").strip()
        if not line:
            continue
        last = line
        if line.startswith("OK ") or line.startswith("ERR ") or line == "PONG":
            return line
    raise RuntimeError(f"timeout on '{cmd}' last='{last}'")


def normalize_remote_path(local_path: Path, root: Path) -> str:
    rel = local_path.relative_to(root)
    return "/" + str(rel).replace("\\", "/")


def provision_file(ser: serial.Serial, local_path: Path, remote_path: str, chunk_size: int) -> None:
    response = send_cmd(ser, f"FFAT_RESET {remote_path}")
    if not response.startswith("OK "):
        raise RuntimeError(f"FFAT_RESET failed for {remote_path}: {response}")

    raw = local_path.read_bytes()
    for offset in range(0, len(raw), chunk_size):
        chunk = raw[offset : offset + chunk_size]
        payload = base64.b64encode(chunk).decode("ascii")
        last_error = ""
        for _ in range(3):
            try:
                response = send_cmd(ser, f"FFAT_APPEND_B64 {remote_path} {payload}", timeout_s=20.0)
            except Exception as exc:
                last_error = str(exc)
                time.sleep(0.2)
                continue
            if response.startswith("OK "):
                last_error = ""
                break
            last_error = response
            time.sleep(0.2)
        if last_error:
            raise RuntimeError(f"FFAT_APPEND_B64 failed for {remote_path}: {last_error}")

    response = send_cmd(ser, f"FFAT_EXISTS {remote_path}")
    if not response.startswith("OK "):
        raise RuntimeError(f"FFAT_EXISTS failed for {remote_path}: {response}")


def main() -> int:
    parser = argparse.ArgumentParser(description="Provision FFat media over serial")
    parser.add_argument("--port", default="/dev/cu.usbserial-0001")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--root", default="data/audio", help="local media root directory")
    parser.add_argument("--chunk-bytes", type=int, default=48, help="raw bytes per base64 serial frame")
    parser.add_argument("--ext", default=".wav", help="comma-separated file extensions to provision (default: .wav)")
    args = parser.parse_args()

    root = Path(args.root).resolve()
    if not root.exists():
        raise RuntimeError(f"media root not found: {root}")

    allowed_ext = {item.strip().lower() for item in args.ext.split(",") if item.strip()}
    files = sorted(
        p
        for p in root.rglob("*")
        if p.is_file() and (not allowed_ext or p.suffix.lower() in allowed_ext)
    )
    if not files:
        raise RuntimeError(f"no files found in {root}")

    with serial.Serial(args.port, args.baud, timeout=0.5) as ser:
        time.sleep(0.9)
        if send_cmd(ser, "PING", timeout_s=3.0) != "PONG":
            raise RuntimeError("device ping failed")

        for index, local_path in enumerate(files, start=1):
            remote_path = normalize_remote_path(local_path, root)
            print(f"[{index}/{len(files)}] {remote_path}")
            provision_file(ser, local_path, remote_path, args.chunk_bytes)

    print("FFat media provision complete.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
