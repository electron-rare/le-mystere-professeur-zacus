#!/usr/bin/env python3
"""Validate Freenove microphone runtime metrics over serial."""

from __future__ import annotations

import argparse
import json
import os
import re
import sys
import time
from dataclasses import dataclass

try:
    import serial
    from serial.tools import list_ports
except ImportError:
    print("Missing dependency: pip install pyserial", file=sys.stderr)
    raise SystemExit(2)


STATUS_RE = re.compile(r"\bSTATUS\b.*\bscreen=([A-Z0-9_]+)\b")

SCENE_LA_OPTIONS = ("SCENE_LA_DETECTOR",)
TUNER_RE = re.compile(
    r"\bMIC_TUNER_STATUS\b.*\bfreq=(\d+)\b.*\bcents=(-?\d+)\b.*\bconf=(\d+)\b.*\blevel=(\d+)\b.*\bpeak=(\d+)\b"
)


@dataclass(frozen=True)
class MicStats:
    sample_count: int
    mic_ready_all: bool
    level_min: int
    level_max: int
    level_span: int
    peak_max: int
    freq_non_zero: int
    conf_non_zero: int


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
            time.sleep(0.02)
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


def send(ser: serial.Serial, command: str, timeout_s: float) -> list[str]:
    ser.write((command + "\n").encode("utf-8"))
    ser.flush()
    time.sleep(0.08)
    return read_lines(ser, timeout_s)


def find_status_screen(lines: list[str]) -> str | None:
    for line in reversed(lines):
        match = STATUS_RE.search(line)
        if match:
            return match.group(1)
    return None


def extract_hw_json(lines: list[str]) -> dict | None:
    for line in reversed(lines):
        if not line.startswith("{") or not line.endswith("}"):
            continue
        try:
            parsed = json.loads(line)
        except json.JSONDecodeError:
            continue
        if isinstance(parsed, dict):
            return parsed
    return None


def extract_tuner_status(lines: list[str]) -> dict | None:
    for line in reversed(lines):
        match = TUNER_RE.search(line)
        if not match:
            continue
        freq, cents, conf, level, peak = match.groups()
        return {
            "mic_ready": True,
            "mic_freq_hz": int(freq),
            "mic_pitch_cents": int(cents),
            "mic_pitch_confidence": int(conf),
            "mic_level_pct": int(level),
            "mic_peak": int(peak),
        }
    return None


def collect_stats(samples: list[dict]) -> MicStats:
    levels = [int(sample.get("mic_level_pct", 0)) for sample in samples]
    peaks = [int(sample.get("mic_peak", 0)) for sample in samples]
    freqs = [int(sample.get("mic_freq_hz", 0)) for sample in samples]
    confs = [int(sample.get("mic_pitch_confidence", 0)) for sample in samples]
    return MicStats(
        sample_count=len(samples),
        mic_ready_all=all(bool(sample.get("mic_ready", False)) for sample in samples),
        level_min=min(levels),
        level_max=max(levels),
        level_span=max(levels) - min(levels),
        peak_max=max(peaks),
        freq_non_zero=sum(1 for value in freqs if value > 0),
        conf_non_zero=sum(1 for value in confs if value > 0),
    )


def run(args: argparse.Namespace) -> int:
    port = args.port.strip() or detect_port()
    if not port:
        print("[fail] no serial port detected (expected usbmodem)", file=sys.stderr)
        return 2

    print(f"[info] port={port} baud={args.baud}")
    with serial.Serial(port, args.baud, timeout=0.2) as ser:
        time.sleep(args.settle)
        ser.reset_input_buffer()
        ser.reset_output_buffer()

        send(ser, "RESET", 1.4)
        send(ser, "SC_LOAD DEFAULT", 1.8)
        send(ser, "SC_EVENT unlock", 1.0)
        send(ser, "SC_EVENT serial BTN_NEXT", 1.0)

        status_lines = send(ser, "STATUS", 0.8)
        current_screen = find_status_screen(status_lines)
        print(f"[info] screen={current_screen or 'unknown'}")
        if current_screen not in SCENE_LA_OPTIONS:
            print("[fail] expected LA scene before mic sampling, got %s" % (current_screen or "none"), file=sys.stderr)
            return 1

        samples: list[dict] = []
        deadline = time.time() + args.duration
        while time.time() < deadline:
            payload = extract_hw_json(send(ser, "HW_STATUS_JSON", 0.35))
            if payload is None:
                payload = extract_tuner_status(send(ser, "MIC_TUNER_STATUS", 0.35))
            if payload is not None:
                samples.append(payload)
            if len(samples) >= args.samples:
                break
            time.sleep(0.05)

    if not samples:
        print("[fail] no mic payload decoded (HW_STATUS_JSON or MIC_TUNER_STATUS)", file=sys.stderr)
        return 1

    stats = collect_stats(samples)
    print(f"[mic] samples={stats.sample_count}")
    print(f"[mic] mic_ready_all={int(stats.mic_ready_all)}")
    print(f"[mic] level min/max/span={stats.level_min}/{stats.level_max}/{stats.level_span}")
    print(f"[mic] peak_max={stats.peak_max}")
    print(f"[mic] freq_non_zero={stats.freq_non_zero}")
    print(f"[mic] conf_non_zero={stats.conf_non_zero}")

    ok = (
        stats.mic_ready_all
        and stats.peak_max >= args.min_peak
        and stats.level_span >= args.min_level_span
        and stats.freq_non_zero > 0
    )
    print(f"[mic] verdict={'PASS' if ok else 'FAIL'}")
    return 0 if ok else 1


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Validate live mic metrics on Freenove over serial.")
    parser.add_argument("--port", default="", help="Serial port (default: auto detect)")
    parser.add_argument("--baud", type=int, default=115200, help="Serial baud")
    parser.add_argument("--settle", type=float, default=1.8, help="Boot settle delay in seconds")
    parser.add_argument("--duration", type=float, default=8.0, help="Max sampling duration in seconds")
    parser.add_argument("--samples", type=int, default=18, help="Target number of mic samples")
    parser.add_argument("--min-peak", type=int, default=700, help="Minimum accepted mic_peak max")
    parser.add_argument("--min-level-span", type=int, default=8, help="Minimum accepted mic_level_pct span")
    return parser.parse_args()


def main() -> int:
    return run(parse_args())


if __name__ == "__main__":
    raise SystemExit(main())
