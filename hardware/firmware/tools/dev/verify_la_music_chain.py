#!/usr/bin/env python3
"""Verify LA detection under music-like load on Freenove."""

from __future__ import annotations

import argparse
from datetime import datetime
import json
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


STATUS_RE = re.compile(r"\bSTATUS\b.*\bscreen=([A-Z0-9_]+)\b")
STATUS_KV_RE = re.compile(r"\b([A-Za-z_]+)=([^\s]+)")
AUDIO_STATUS_RE = re.compile(r"\bAUDIO_STATUS\b.*?\bplaying=(\d+)\b.*?\btrack=([^\s]+)")
TUNER_RE = re.compile(
    r"\bMIC_TUNER_STATUS\b.*?\bfreq=(\d+)\b.*?\bcents=(-?\d+)\b.*?\bconf=(\d+)\b.*?\blevel=(\d+)\b"
    r".*?\bgain=(\d+)\b.*?\bla_gate=(\d+)\b.*?\bla_match=(\d+)\b.*?\bla_lock=(\d+)\b"
    r".*?\bla_pending=(\d+)\b.*?\bla_stable_ms=(\d+)\b.*?\bla_pct=(\d+)\b",
    re.IGNORECASE,
)
PANIC_RE = re.compile(r"\b(Guru Meditation|panic|abort|assert failed|reboot|Watchdog|CPU\d+ panic)\b", re.IGNORECASE)
_INACTIVE_TRACK_VALUES = frozenset({"", "0", "n/a", "none", "unknown", "-"})
_KNOWN_NON_NUMERIC_STATUS_KEYS = {
    "screen",
    "step",
    "scenario",
    "pack",
    "track",
    "codec",
    "ip",
    "state",
    "mode",
    "profile",
    "fx",
    "net",
}


def normalize_scene_id(scene: str | None) -> str | None:
    if not scene:
        return None
    if scene == "SCENE_LA_DETECT":
        return "SCENE_LA_DETECTOR"
    return scene


def detect_port() -> str | None:
    env_port = os.getenv("ZACUS_PORT_ESP32", "").strip()
    if env_port:
        return env_port

    modem_ports: list[str] = []
    fallback_ports: list[str] = []
    for port in list_ports.comports():
        device = str(port.device)
        if "usbmodem" in device:
            modem_ports.append(device)
        elif "usbserial" in device or "SLAB_USBtoUART" in device:
            fallback_ports.append(device)

    if modem_ports:
        return sorted(modem_ports)[0]
    if fallback_ports:
        return sorted(fallback_ports)[0]
    return None


def make_default_log_path() -> Path:
    log_root = Path(__file__).resolve().parents[2] / "logs"
    log_root.mkdir(parents=True, exist_ok=True)
    return log_root / f"verify_la_music_chain_{datetime.now().strftime('%Y%m%d_%H%M%S')}.log"


def setup_logger(log_path: Path | None) -> tuple[callable, callable, Path]:
    resolved = log_path
    if resolved is None:
        resolved = make_default_log_path()
    resolved.parent.mkdir(parents=True, exist_ok=True)

    lines: list[str] = []

    def log(message: str, *, is_error: bool = False) -> None:
        lines.append(message)
        print(message, file=sys.stderr if is_error else sys.stdout)

    def flush() -> None:
        resolved.write_text("\n".join(lines) + "\n", encoding="utf-8")

    return (log, flush, resolved)


def read_lines(ser: serial.Serial, timeout_s: float) -> list[str]:
    deadline = time.time() + timeout_s
    lines: list[str] = []
    pending = ""
    while time.time() < deadline:
        try:
            available = ser.in_waiting
        except serial.SerialException:
            time.sleep(0.02)
            continue
        if available <= 0:
            time.sleep(0.02)
            continue
        try:
            data = ser.read(available)
        except serial.SerialException:
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
    try:
        ser.write((command + "\n").encode("utf-8"))
        ser.flush()
    except serial.SerialException:
        return []
    time.sleep(0.08)
    return read_lines(ser, timeout_s)


def query_status(ser: serial.Serial, scene: str | None = None, timeout_s: float = 0.8) -> dict[str, str | int] | None:
    lines = send(ser, "STATUS", timeout_s)
    for line in lines:
        status = parse_status(line)
        if status is None:
            continue
        if "screen" in status and isinstance(status["screen"], str):
            status["screen"] = normalize_scene_id(status["screen"]) or status["screen"]
        if scene is not None and isinstance(scene, str):
            scene = normalize_scene_id(scene)
        if scene is not None and status.get("screen") != scene:
            continue
        return status
    return None


def parse_audio_status(line: str) -> dict[str, str | int] | None:
    match = AUDIO_STATUS_RE.search(line)
    if not match:
        return None
    track = match.group(2)
    try:
        return {"playing": int(match.group(1)), "track": track}
    except ValueError:
        return None


def query_audio_status(ser: serial.Serial, timeout_s: float = 0.4) -> dict[str, str | int] | None:
    for line in send(ser, "AUDIO_STATUS", timeout_s):
        parsed = parse_audio_status(line)
        if parsed is not None:
            return parsed
    return None


def is_track_playing(value: str | int | None) -> bool:
    return isinstance(value, str) and value not in _INACTIVE_TRACK_VALUES and "n/a" not in value.lower()


def is_scene_audio_ready(status: dict[str, str | int] | None, audio: dict[str, str | int] | None) -> bool:
    if status is not None:
        if status.get("audio") == 1:
            return True
        if status.get("media_play") == 1:
            return True
        if is_track_playing(status.get("track")):
            return True
    if audio is not None and audio.get("playing") == 1:
        return True
    return False


def wait_screen(
    ser: serial.Serial,
    expected: str,
    timeout_s: float,
    *,
    min_stable_samples: int = 2,
) -> int:
    deadline = time.time() + timeout_s
    started_at = time.time()
    normalized_expected = normalize_scene_id(expected)
    stable_count = 0
    while time.time() < deadline:
        status = query_status(ser, expected, 0.5)
        if status is not None and status.get("screen") == normalized_expected:
            stable_count += 1
            if stable_count >= max(1, min_stable_samples):
                return int((time.time() - started_at) * 1000)
        else:
            stable_count = 0
        time.sleep(0.15)
    return 0


def parse_status(line: str) -> dict[str, str | int] | None:
    match = STATUS_RE.search(line)
    if not match:
        return None

    payload: dict[str, str | int] = {"screen": match.group(1)}
    for key, value in STATUS_KV_RE.findall(line):
        if key in _KNOWN_NON_NUMERIC_STATUS_KEYS:
            payload[key] = value
        elif key == "audio":
            try:
                payload[key] = int(value)
            except ValueError:
                continue
        else:
            try:
                payload[key] = int(value)
            except ValueError:
                payload[key] = value
    return payload


def wait_scene_audio(
    ser: serial.Serial,
    scene: str,
    timeout_s: float,
    *,
    require_scene: bool = True,
    min_stable_samples: int = 2,
) -> int:
    deadline = time.time() + timeout_s
    started_at = time.time()
    stable_count = 0
    normalized_scene = normalize_scene_id(scene)
    if timeout_s <= 0:
        return 0
    while time.time() < deadline:
        status = query_status(ser, None, 0.4)
        if status is None:
            time.sleep(0.05)
            continue
        if require_scene and status.get("screen") != normalized_scene:
            stable_count = 0
            time.sleep(0.05)
            continue
        audio_status = query_audio_status(ser, 0.22)
        if is_scene_audio_ready(status, audio_status):
            stable_count += 1
            if stable_count >= max(1, min_stable_samples):
                return int((time.time() - started_at) * 1000)
        else:
            stable_count = 0
        time.sleep(0.05)
    return 0


def ensure_scene_loaded(
    ser: serial.Serial,
    target: str,
    timeout_s: float,
    *,
    log: callable | None = None,
    min_stable_samples: int = 1,
) -> int:
    """Ensure board is on target scene, adding explicit BTN_NEXT steering for LA flow."""
    normalized_target = normalize_scene_id(target)
    if not normalized_target:
        return 0

    # If already here, keep a short debounce.
    scene_ms = wait_screen(
        ser,
        normalized_target,
        max(0.25, timeout_s * 0.25),
        min_stable_samples=min_stable_samples,
    )
    if scene_ms > 0:
        return scene_ms

    for attempt in range(2):
        if log is not None:
            log(f"[step] navigation vers {normalized_target}: BTN_NEXT tentative {attempt + 1}")
        send(ser, "SC_EVENT serial BTN_NEXT", 0.4)
        scene_ms = wait_screen(
            ser,
            normalized_target,
            max(0.5, timeout_s * 0.25),
            min_stable_samples=min_stable_samples,
        )
        if scene_ms > 0:
            return scene_ms

    if log is not None:
        log("[step] relance du scénario DEFAULT pour remise en phase")
    send(ser, "SC_LOAD DEFAULT", 1.0)
    # Remettre la sequence sur l'etape LA de maniere deterministe:
    # LOCKED -> (BTN_NEXT) BROKEN -> (BTN_NEXT) LA_DETECTOR
    locked = wait_screen(ser, "SCENE_LOCKED", 4.0)
    if locked <= 0:
        return 0
    for attempt in range(2):
        if log is not None:
            log(f"[step] relance + BTN_NEXT {attempt + 1}/2")
        send(ser, "SC_EVENT serial BTN_NEXT", 0.4)
        scene_ms = wait_screen(
            ser,
            normalized_target,
            max(1.0, timeout_s * 0.20),
            min_stable_samples=min_stable_samples,
        )
        if scene_ms > 0:
            return scene_ms

    return 0


def wait_first_tuner_sample(ser: serial.Serial, timeout_s: float) -> tuple[dict[str, int] | None, int | None]:
    started_at = time.time()
    deadline = time.time() + timeout_s
    while time.time() < deadline:
        for line in read_lines(ser, 0.25):
            payload = parse_tuner_status(line)
            if payload is not None:
                return payload, int((time.time() - started_at) * 1000)
        time.sleep(0.05)
    return None, None


def parse_tuner_status(line: str) -> dict[str, int] | None:
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


def choose_launch_music(path: Path | None, volume: float, *, loop: bool = False) -> subprocess.Popen[bytes] | None:
    if path is None:
        return None

    if not path.exists():
        print(f"[error] music file not found: {path}", file=sys.stderr)
        return None

    if shutil.which("afplay"):
        cmd = ["/usr/bin/afplay"]
        if volume is not None:
            clamped = max(0.0, min(1.0, volume))
            cmd.extend(["-v", f"{clamped:.2f}"])
        cmd.append(str(path))
        return subprocess.Popen(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

    if shutil.which("ffplay"):
        cmd = [
            "ffplay",
            "-nodisp",
            "-autoexit" if not loop else "-stream_loop",
            "-1" if loop else "",
            "-loglevel",
            "error",
            "-volume",
            str(int(max(0, min(100, int(volume * 100))))),
            str(path),
        ]
        cmd = [entry for entry in cmd if entry != ""]
        return subprocess.Popen(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

    print("[error] no supported audio player found (afplay/ffplay)", file=sys.stderr)
    return None


def generate_segmented_wav(
    wav_path: Path,
    duration_s: float,
    sample_rate: int,
    target_hz: int,
    stable_ms: int,
    seed: int | None,
    bg_volume: float,
    la_volume: float,
) -> None:
    if seed is not None:
        random.seed(seed)

    segments: list[tuple[float, float, float]] = []
    cursor = 0
    total_ms = max(4000, int(duration_s * 1000))
    la_anchor_ratio = 0.70
    target_anchor_ms = int(total_ms * la_anchor_ratio)
    min_anchor_ms = min(
        max(stable_ms + 3000, 6500),
        max(3000, total_ms - 2500),
    )
    anchor_length_ms = max(min_anchor_ms, min(target_anchor_ms, total_ms - 2500))

    if anchor_length_ms >= total_ms - 500:
        anchor_start = 500
        anchor_end = total_ms
    else:
        anchor_start_min = 500
        anchor_start_max = max(anchor_start_min, int(total_ms * 0.20))
        anchor_start = random.randint(anchor_start_min, anchor_start_max)
        anchor_end = min(total_ms - 300, anchor_start + anchor_length_ms)
        if anchor_end <= anchor_start:
            anchor_start = 500
            anchor_end = total_ms

    while cursor < total_ms:
        remaining = total_ms - cursor
        if cursor < anchor_start:
            seg_ms = min(random.randint(80, 220), anchor_start - cursor)
            if random.random() < 0.45:
                freq = float(random.randint(260, 1200))
                amp = 0.05 + (random.random() * 0.22 * bg_volume)
            else:
                freq = 0.0
                amp = 0.02 * bg_volume
            segments.append((seg_ms / 1000.0, freq, amp))
            cursor += seg_ms
            continue

        if cursor < anchor_end:
            if cursor == anchor_start:
                burst_ms = min(
                    max(stable_ms + 1200, 4200),
                    max(4000, anchor_end - cursor - 800),
                )
                segments.append((burst_ms / 1000.0, float(target_hz), 0.95))
                cursor += burst_ms
                continue

            # Keep LA dominant, with short disturbances only.
            if random.random() < 0.04 and (cursor + 60) < anchor_end:
                seg_ms = random.randint(60, 120)
                freq = float(random.randint(260, 1100))
                amp = 0.08 + (random.random() * 0.08 * bg_volume)
                segments.append((seg_ms / 1000.0, freq, amp))
                cursor += seg_ms
                continue

            seg_ms = min(random.randint(450, 900), anchor_end - cursor)
            freq = float(random.randint(max(260, target_hz - 3), target_hz + 3))
            amp = max(0.2, (0.70 + (random.random() * 0.25)) * la_volume)
            amp = min(0.95, amp)
            segments.append((seg_ms / 1000.0, freq, amp))
            cursor += seg_ms
            continue

        seg_ms = min(random.randint(120, 360), remaining)
        if random.random() < 0.40:
            if remaining > 2600 and random.random() < 0.15:
                freq = float(random.randint(max(240, target_hz - 24), target_hz + 24))
                amp = 0.22 + (random.random() * 0.22 * la_volume)
            else:
                freq = float(random.randint(260, 1200))
                amp = 0.07 + (random.random() * 0.18 * bg_volume)
        else:
            freq = 0.0
            amp = 0.025 * bg_volume
        segments.append((seg_ms / 1000.0, freq, amp))
        cursor += seg_ms

    signal: list[float] = []
    phase = random.uniform(0.0, 2 * math.pi)
    for sec, freq, amp in segments:
        sample_count = max(1, int(sec * sample_rate))
        if freq <= 0.0:
            signal.extend([0.0] * sample_count)
            continue

        phase_step = 2.0 * math.pi * freq / float(sample_rate)
        for _ in range(sample_count):
            phase = (phase + phase_step) % (2.0 * math.pi)
            signal.append(math.sin(phase) * amp)

    with wave.open(str(wav_path), "wb") as wav:
        wav.setnchannels(1)
        wav.setsampwidth(2)
        wav.setframerate(sample_rate)
        raw = bytearray()
        for sample in signal:
            clamped = max(-1.0, min(1.0, sample))
            raw.extend(int(clamped * 32767).to_bytes(2, "little", signed=True))
        wav.writeframes(raw)


def stop_player(process: subprocess.Popen[bytes] | None) -> None:
    if process is None:
        return
    if process.poll() is not None:
        return
    process.terminate()
    try:
        process.wait(timeout=0.8)
    except subprocess.TimeoutExpired:
        process.kill()


def run(args: argparse.Namespace) -> int:
    log, flush_log, log_path = setup_logger(Path(args.log_file).expanduser() if args.log_file else None)
    log(f"[info] logs -> {log_path}")
    stable_hit = False
    try:
        port = args.port.strip() or detect_port()
        if not port:
            log("[error] no serial port detected (usbmodem expected)", is_error=True)
            return 2

        bg_music = Path(args.bg_music).expanduser() if args.bg_music else None
        out_json = Path(args.out_json).expanduser() if args.out_json else None
        metrics = {
            "samples": 0,
            "scene_locked_ms": None,
            "scene_la_entry_ms": None,
            "first_lock_ms": None,
            "latency_to_lock": None,
            "max_stable_ms": 0,
            "freq_at_lock": 0,
            "conf_max": 0,
            "audio_ready": False,
            "audio_ready_ms": None,
            "panic_seen": False,
        }

        log(f"[info] port={port} baud={args.baud} target_hz={args.target_hz} stable_ms={args.stable_ms}")
        with serial.Serial(port, args.baud, timeout=0.2) as ser:
            time.sleep(1.0)
            ser.reset_input_buffer()
            ser.reset_output_buffer()

            send(ser, "RESET", 1.2)
            send(ser, "SC_LOAD DEFAULT", 1.6)
            scene_locked_ms = wait_screen(ser, "SCENE_LOCKED", 6.0)
            metrics["scene_locked_ms"] = scene_locked_ms
            if scene_locked_ms <= 0:
                log("[error] failed to reach SCENE_LOCKED", is_error=True)
                return 1
            log(f"[ok] SCENE_LOCKED ({scene_locked_ms}ms)")

            # Forcer la scene LA en cas de retour inattendu ou d'etat intermediaire.
            scene_la_ms = ensure_scene_loaded(ser, "SCENE_LA_DETECTOR", 6.0, log=log, min_stable_samples=2)
            metrics["scene_la_entry_ms"] = scene_la_ms
            if scene_la_ms <= 0:
                log("[error] failed to enter SCENE_LA_DETECTOR", is_error=True)
                return 1
            log(f"[ok] SCENE_LA_DETECTOR ({scene_la_ms}ms)")

            scene_stabilize_ms = int(args.scene_stabilize_ms)
            if scene_stabilize_ms > 0:
                log(f"[step] stabilization scène: {scene_stabilize_ms}ms")
                time.sleep(scene_stabilize_ms / 1000.0)

            send(ser, "MIC_TUNER_STATUS 1 140", 0.3)

            audio_warmup_ms = int(args.audio_warmup_ms)
            if audio_warmup_ms > 0:
                log(f"[step] attente démarrage MP3 scène: {audio_warmup_ms}ms")
                audio_ready_ms = wait_scene_audio(ser, "SCENE_LA_DETECTOR", audio_warmup_ms / 1000.0, min_stable_samples=2)
                if audio_ready_ms > 0:
                    metrics["audio_ready"] = True
                    metrics["audio_ready_ms"] = audio_ready_ms
                    log(f"[ok] lecture MP3 scène détectée ({audio_ready_ms}ms)")
                else:
                    metrics["audio_ready"] = False
                    log("[warn] aucune lecture MP3 détectée pendant la phase de chauffe")
            else:
                metrics["audio_ready_ms"] = 0

            first_tuner_sample_ms_arg = int(args.first_tuner_sample_ms)

            bg_player = None
            la_player = None
            last_scene = "SCENE_LA_DETECTOR"
            next_scene: str | None = None
            start_time = 0.0

            with tempfile.TemporaryDirectory(prefix="zacus_la_music_") as tmpdir:
                wav_path = Path(tmpdir) / "la_tone.wav"
                generate_segmented_wav(
                    wav_path,
                    args.duration,
                    args.sample_rate,
                    args.target_hz,
                    args.stable_ms,
                    args.seed,
                    args.bg_volume,
                    args.la_volume,
                )
                log(f"[info] source={wav_path}")

                bg_player = choose_launch_music(bg_music, args.bg_volume, loop=True)
                if bg_music is not None and bg_player is None:
                    stop_player(bg_player)
                    return 1

                la_player = choose_launch_music(wav_path, args.la_volume, loop=False)
                if la_player is None:
                    stop_player(bg_player)
                    return 1

                try:
                    tune_start = time.time()
                    start_time = tune_start
                    first_tuner_sample_ms = None
                    if first_tuner_sample_ms_arg > 0:
                        first_sample, first_tuner_sample_ms = wait_first_tuner_sample(
                            ser, first_tuner_sample_ms_arg / 1000.0
                        )
                        if first_sample is None:
                            log("[warn] aucun échantillon MIC_TUNER_STATUS reçu au démarrage")
                        else:
                            log(
                                f"[step] premier échantillon MIC_TUNER_STATUS à {first_tuner_sample_ms}ms "
                                f"freq={first_sample['freq_hz']} conf={first_sample['conf']} level={first_sample['level']} "
                                f"stable_ms={first_sample['stable_ms']}"
                            )

                    last_stable_ms = -1
                    next_status_check_ms = 0.0
                    deadline = time.time() + args.duration
                    while time.time() < deadline:
                        now = time.time()
                        if now >= next_status_check_ms:
                            status = query_status(ser, timeout_s=0.25)
                            if status is not None:
                                current_screen = status.get("screen")
                                if isinstance(current_screen, str) and current_screen != last_scene:
                                    next_scene = current_screen
                                    last_scene = current_screen
                                    log(f"[step] screen={current_screen}")
                            next_status_check_ms = now + 0.8

                        lines = read_lines(ser, 0.2)
                        if not lines:
                            continue

                        now_ms = int((time.time() - start_time) * 1000)
                        for line in lines:
                            if PANIC_RE.search(line):
                                metrics["panic_seen"] = True
                                log(f"[error] panic/reboot marker: {line}", is_error=True)
                                return 1

                            status_match = STATUS_RE.search(line)
                            if status_match:
                                current_scene = status_match.group(1)
                                if current_scene != last_scene:
                                    next_scene = current_scene
                                    last_scene = current_scene
                                    log(f"[step] screen={current_scene}")

                            payload = parse_tuner_status(line)
                            if payload is None:
                                continue
                            if payload["stable_ms"] != last_stable_ms:
                                last_stable_ms = payload["stable_ms"]
                                log(
                                    f"[state] stable_ms={payload['stable_ms']}ms pct={payload['pct']} "
                                    f"lock={payload['lock']} match={payload['match']} freq={payload['freq_hz']}"
                                )
                            if first_tuner_sample_ms is None:
                                first_tuner_sample_ms = now_ms
                            metrics["samples"] += 1
                            metrics["conf_max"] = max(metrics["conf_max"], payload["conf"])
                            metrics["max_stable_ms"] = max(metrics["max_stable_ms"], payload["stable_ms"])

                            if payload["lock"] == 1:
                                if metrics["first_lock_ms"] is None:
                                    metrics["first_lock_ms"] = now_ms
                                    metrics["freq_at_lock"] = payload["freq_hz"]

                            if payload["stable_ms"] >= args.stable_ms:
                                if metrics["latency_to_lock"] is None:
                                    metrics["latency_to_lock"] = now_ms
                                stable_hit = True
                                log(
                                    f"[ok] locked at {now_ms}ms stable={payload['stable_ms']}ms "
                                    f"freq={payload['freq_hz']} conf={payload['conf']} level={payload['level']}"
                                )
                                break

                        if stable_hit:
                            # Keep room for serial transition flush.
                            time.sleep(0.45)
                            break
                finally:
                    stop_player(la_player)
                    stop_player(bg_player)

                if out_json:
                    with out_json.open("w", encoding="utf-8") as fd:
                        json.dump(
                            {
                                **metrics,
                                "scene_stabilize_ms": scene_stabilize_ms,
                                "audio_warmup_ms": audio_warmup_ms,
                                "first_tuner_sample_ms": first_tuner_sample_ms,
                                "audio_ready_scene_ms": metrics["audio_ready_ms"],
                                "expected_stable_ms": args.stable_ms,
                                "duration_s": args.duration,
                            },
                            fd,
                            indent=2,
                        )
                log(f"[out-json] {out_json}")

            if not stable_hit:
                log("[fail] LA lock never reached", is_error=True)
                log(
                    f"[res] samples={metrics['samples']} first_lock_ms={metrics['first_lock_ms']} "
                    f"latency_ms={metrics['latency_to_lock']} max_stable_ms={metrics['max_stable_ms']} "
                    f"scene_locked_ms={metrics['scene_locked_ms']} scene_la_entry_ms={metrics['scene_la_entry_ms']} "
                    f"conf_max={metrics['conf_max']} panic_seen={int(metrics['panic_seen'])} "
                    f"audio_ready={int(metrics['audio_ready'])} audio_ready_ms={metrics['audio_ready_ms']} "
                    f"first_tuner_sample_ms={first_tuner_sample_ms}"
                )
                return 1

            if next_scene and next_scene != "SCENE_LA_DETECTOR":
                log(f"[info] transition to {next_scene}")

            log(
                "[pass] LA stable reached "
                f"first_lock_ms={metrics['first_lock_ms']} "
                f"latency_to_lock={metrics['latency_to_lock']} "
                f"max_stable_ms={metrics['max_stable_ms']} "
                f"freq_at_lock={metrics['freq_at_lock']} "
                f"scene_locked_ms={metrics['scene_locked_ms']} scene_la_entry_ms={metrics['scene_la_entry_ms']} "
                f"conf_max={metrics['conf_max']} "
                f"panic_seen={int(metrics['panic_seen'])} "
                f"audio_ready={int(metrics['audio_ready'])} audio_ready_ms={metrics['audio_ready_ms']} "
                f"first_tuner_sample_ms={first_tuner_sample_ms}"
            )
            return 0
    finally:
        flush_log()


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="LA chain verification with generated tones and music context.")
    parser.add_argument("--port", default="", help="Serial port (auto usbmodem if absent)")
    parser.add_argument("--baud", type=int, default=115200, help="Serial baud rate")
    parser.add_argument("--duration", type=float, default=20.0, help="Test duration in seconds")
    parser.add_argument("--target-hz", type=int, default=440, help="Target LA frequency in Hz")
    parser.add_argument("--stable-ms", type=int, default=3000, help="Required stable_ms threshold")
    parser.add_argument("--sample-rate", type=int, default=22050, help="WAV sample rate for generated LA")
    parser.add_argument("--bg-music", default="", help="Optional background music file to loop during test")
    parser.add_argument("--bg-volume", type=float, default=0.5, help="Background music volume multiplier")
    parser.add_argument("--la-volume", type=float, default=0.7, help="Generated LA volume multiplier")
    parser.add_argument("--seed", type=int, default=None, help="Seed for random generation")
    parser.add_argument("--out-json", default="", help="Optional path to save metrics JSON")
    parser.add_argument("--scene-stabilize-ms", type=int, default=1200, help="Scene stabilize delay in ms")
    parser.add_argument("--audio-warmup-ms", type=int, default=2200, help="Scene MP3 warmup delay in ms")
    parser.add_argument("--first-tuner-sample-ms", type=int, default=1800, help="Delay to get first MIC_TUNER_STATUS sample in ms")
    parser.add_argument("--log-file", default="", help="Optional explicit log file (default: hardware/firmware/logs/verify_la_music_chain_*.log)")
    return parser.parse_args()


def main() -> int:
    return run(parse_args())


if __name__ == "__main__":
    raise SystemExit(main())
