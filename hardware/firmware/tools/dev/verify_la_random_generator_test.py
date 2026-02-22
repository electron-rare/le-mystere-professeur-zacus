#!/usr/bin/env python3
"""Generate and play random tones with intermittent LA and validate LA detector over serial."""

from __future__ import annotations

import argparse
import math
import os
import random
import re
import shutil
import subprocess
import sys
import tempfile
import time
import wave
from pathlib import Path

try:
    import serial
    from serial.tools import list_ports
except ImportError as exc:
    print("Missing dependency: pip install pyserial", file=sys.stderr)
    raise SystemExit(2) from exc


TUNER_RE = re.compile(
    r"\bMIC_TUNER_STATUS\b.*?\bfreq=(\d+)\b.*?\bcents=(-?\d+)\b.*?\bconf=(\d+)\b.*?\blevel=(\d+)\b"
    r".*?\bgain=(\d+)\b.*?\bla_gate=(\d+)\b.*?\bla_match=(\d+)\b.*?\bla_lock=(\d+)\b"
    r".*?\bla_pending=(\d+)\b.*?\bla_stable_ms=(\d+)\b.*?\bla_pct=(\d+)\b",
    re.IGNORECASE,
)
STATUS_RE = re.compile(r"\bSTATUS\b.*\bscreen=([A-Z0-9_]+)\b")


def detect_port() -> str | None:
    env_port = os.getenv("ZACUS_PORT_ESP32", "").strip()
    if env_port:
        return env_port

    usbmodem = []
    fallback = []
    for port in list_ports.comports():
        device = str(port.device)
        if "usbmodem" in device:
            usbmodem.append(device)
        elif "usbserial" in device or "SLAB_USBtoUART" in device:
            fallback.append(device)

    if usbmodem:
        return sorted(usbmodem)[0]
    if fallback:
        return sorted(fallback)[0]
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
    time.sleep(0.1)
    return read_lines(ser, timeout_s)


def wait_screen(ser: serial.Serial, expected: str, timeout_s: float) -> bool:
    deadline = time.time() + timeout_s
    while time.time() < deadline:
        lines = send(ser, "STATUS", 0.8)
        for line in lines:
            match = STATUS_RE.search(line)
            if match and match.group(1) == expected:
                return True
        time.sleep(0.15)
    return False


def parse_tuner_status(line: str) -> dict | None:
    match = TUNER_RE.search(line)
    if not match:
        return None
    freq, cents, conf, level, gain, gate, match_f, lock_f, pending, stable_ms, pct = match.groups()
    return {
        "freq_hz": int(freq),
        "cents": int(cents),
        "conf": int(conf),
        "level": int(level),
        "gain": int(gain),
        "gate": int(gate),
        "match": int(match_f),
        "lock": int(lock_f),
        "pending": int(pending),
        "stable_ms": int(stable_ms),
        "pct": int(pct),
    }


def generate_wav(path: Path, duration_s: float, sample_rate: int, seed: int | None) -> None:
    if seed is not None:
        random.seed(seed)

    total_ms = int(duration_s * 1000)
    cursor = 0
    segments: list[tuple[float, int, float]] = []

    # At least one long LA anchor to validate lock accumulation logic.
    anchor_start_ms = random.randint(max(300, int(total_ms * 0.15)), max(300, int(total_ms * 0.45)))
    anchor_length_ms = random.randint(3200, 4800)
    anchor_end_ms = min(total_ms - 300, anchor_start_ms + anchor_length_ms)
    if anchor_end_ms <= anchor_start_ms:
        anchor_end_ms = total_ms

    la_freq = 440
    la_strength = 0.42
    bg_strength = 0.22

    while cursor < total_ms:
        remaining = total_ms - cursor
        if cursor < anchor_start_ms:
            if remaining <= 0:
                break
            seg_ms = min(random.randint(150, 600), remaining)
            # avoid entering LA anchor zone too early
            if cursor + seg_ms > anchor_start_ms:
                seg_ms = anchor_start_ms - cursor
            if seg_ms <= 0:
                cursor = anchor_start_ms
                continue
            if random.random() < 0.35:
                freq = random.randint(300, 1200)
                amp = bg_strength + random.uniform(-0.04, 0.06)
            else:
                freq = 0
                amp = bg_strength * 0.6
            segments.append((seg_ms / 1000.0, freq, amp))
            cursor += seg_ms
            continue

        if cursor < anchor_end_ms:
            seg_ms = min(random.randint(250, 700), anchor_end_ms - cursor)
            if seg_ms <= 0:
                cursor = anchor_end_ms
                continue
            freq = random.randint(430, 450)
            # small jitter around 440 Hz
            amp = la_strength + random.uniform(-0.05, 0.06)
            segments.append((seg_ms / 1000.0, freq, amp))
            cursor += seg_ms
            continue

        seg_ms = random.randint(180, 750)
        seg_ms = min(seg_ms, remaining)
        if random.random() < 0.25:
            freq = random.randint(300, 1200)
            amp = bg_strength + random.uniform(-0.04, 0.06)
        else:
            freq = 0
            amp = bg_strength * 0.6
        segments.append((seg_ms / 1000.0, freq, amp))
        cursor += seg_ms

    signal = []
    for sec, freq, amp in segments:
        n = max(1, int(sec * sample_rate))
        if n <= 0:
            continue
        if freq <= 0:
            for _ in range(n):
                signal.append(0.0)
            continue

        phase = random.random() * 2 * math.pi
        if freq == la_freq:
            frequency = freq + random.uniform(-10.0, 10.0)
        else:
            frequency = float(freq)
        for index in range(n):
            t = (index / sample_rate)
            v = math.sin(2 * math.pi * frequency * t + phase)
            amp_clipped = max(0.08, min(0.65, amp))
            signal.append(v * amp_clipped)

    with wave.open(str(path), "wb") as wav:
        wav.setnchannels(1)
        wav.setsampwidth(2)
        wav.setframerate(sample_rate)
        raw = bytearray()
        for value in signal:
            raw.extend(int(max(-1.0, min(1.0, value)) * 32767).to_bytes(2, "little", signed=True))
        wav.writeframes(raw)


def start_player(wav_path: Path, volume: float | None) -> subprocess.Popen[bytes] | None:
    player = None
    if shutil.which("afplay"):
        cmd = ["/usr/bin/afplay"]
        if volume is not None:
            cmd.extend(["-v", f"{volume:.2f}"])
        cmd.append(str(wav_path))
        player = subprocess.Popen(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    elif shutil.which("ffplay"):
        cmd = ["ffplay", "-nodisp", "-autoexit", "-loglevel", "error", "-stream_loop", "0", "-t", "20", str(wav_path)]
        player = subprocess.Popen(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    else:
        print("[error] aucun lecteur audio trouvé (afplay/ffplay)", file=sys.stderr)
    return player


def run(port: str, baud: int, test_duration: float, sample_rate: int, seed: int | None, volume: float | None) -> int:
    print(f"[info] port={port} baud={baud}")
    with serial.Serial(port, baud, timeout=0.2) as ser:
        time.sleep(1.2)
        ser.reset_input_buffer()
        ser.reset_output_buffer()

        send(ser, "RESET", 1.5)
        send(ser, "SC_LOAD DEFAULT", 2.0)

        if not wait_screen(ser, "SCENE_LOCKED", 6.0):
            print("[error] SCENE_LOCKED non atteinte", file=sys.stderr)
            return 1
        print("[ok] SCENE_LOCKED")

        send(ser, "SC_EVENT serial BTN_NEXT", 0.8)
        if not wait_screen(ser, "SCENE_LA_DETECTOR", 6.0):
            print("[error] SCENE_LA_DETECTOR non atteinte", file=sys.stderr)
            return 1
        print("[ok] SCENE_LA_DETECTOR")

        send(ser, "MIC_TUNER_STATUS 1 150", 0.6)
        send(ser, "HW_STATUS", 0.3)

        with tempfile.TemporaryDirectory(prefix="zacus_la_test_") as td:
            wav_path = Path(td) / "la_rand.wav"
            generate_wav(wav_path, test_duration, sample_rate, seed)
            print(f"[info] wav={wav_path}")

            player = start_player(wav_path, volume)
            if player is None:
                return 1

            try:
                start = time.time()
                deadline = start + test_duration
                metrics: list[dict] = []
                stable_hit = False
                lines_total = 0
                lock_hits = 0

                print(f"[info] lancement audio ({test_duration:.1f}s)")
                while time.time() < deadline:
                    chunk = read_lines(ser, 0.15)
                    if not chunk:
                        continue
                    lines_total += len(chunk)
                    for line in chunk:
                        payload = parse_tuner_status(line)
                        if payload is None:
                            continue
                        metrics.append(payload)
                        if payload["lock"] == 1:
                            lock_hits += 1
                        if payload["stable_ms"] >= 3000:
                            stable_hit = True
                        if len(metrics) >= 2000:
                            break
                    if lock_hits > 0:
                        # keep playing briefly to ensure transition has time to fire
                        if time.time() - start >= 8.0:
                            break
                    if stable_hit and time.time() - start >= 6.0:
                        break

                if player.poll() is None:
                    player.terminate()
                    try:
                        player.wait(timeout=0.8)
                    except subprocess.TimeoutExpired:
                        player.kill()

            finally:
                if player and player.poll() is None:
                    player.terminate()
                    try:
                        player.wait(timeout=0.8)
                    except subprocess.TimeoutExpired:
                        player.kill()

            if not metrics:
                print("[fail] pas de MIC_TUNER_STATUS pendant le test", file=sys.stderr)
                return 1

            max_stable = max(m["stable_ms"] for m in metrics)
            max_conf = max(m["conf"] for m in metrics)
            max_level = max(m["level"] for m in metrics)
            max_gain = max(m["gain"] for m in metrics)
            max_freq = max(metrics, key=lambda x: x["conf"])["freq_hz"]

            print(f"[res] samples={len(metrics)} lines={lines_total} lock_hits={lock_hits}")
            print(f"[res] freq(max conf)={max_freq}Hz conf={max_conf} level={max_level} gain={max_gain} stable={max_stable}ms")

            if stable_hit and max_stable >= 3000:
                print("[pass] LA détecté (stabilité >= 3000ms)")
                return 0

            print("[fail] LA non détecté avec stabilité suffisante")
            return 1


def main() -> int:
    parser = argparse.ArgumentParser(description="Test LA detection with random tones from PC speakers.")
    parser.add_argument("--port", default="", help="Serial port (auto usbmodem if absent)")
    parser.add_argument("--baud", type=int, default=115200, help="Serial baud rate")
    parser.add_argument("--duration", type=float, default=20.0, help="Test duration in seconds")
    parser.add_argument("--sample-rate", type=int, default=22050, help="WAV sample rate")
    parser.add_argument("--seed", type=int, default=None, help="Seed for random generation")
    parser.add_argument("--volume", type=float, default=None, help="Volume factor for afplay/ffplay")
    args = parser.parse_args()

    port = args.port.strip() or detect_port()
    if not port:
        print("[error] aucun port détecté (usbmodem attendu)", file=sys.stderr)
        return 2

    try:
        return run(port, args.baud, args.duration, args.sample_rate, args.seed, args.volume)
    except Exception as exc:  # noqa: BLE001
        print(f"[error] {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
