#!/usr/bin/env python3
"""Verify scene text visibility + WS2812 policy across Freenove scenes."""

from __future__ import annotations

import argparse
import json
import re
import time
from dataclasses import dataclass
from datetime import datetime
from pathlib import Path
from typing import Any

import serial

FATAL_MARKERS = ("PANIC", "Guru Meditation", "ASSERT", "ABORT", "REBOOT", "rst:")
HW_STATUS_RE = re.compile(
    r"\bHW_STATUS\b.*\bws2812=(\d).*?\bauto=(\d).*?\bled=(\d+),(\d+),(\d+).*?\bscene=([A-Z0-9_]+)"
)
ACK_SCENE_GOTO = "ACK SCENE_GOTO ok=1"
ACK_HW_LED_AUTO = "ACK HW_LED_AUTO ok=1"
PATTERN_SCENES = {"SCENE_LOCKED", "SCENE_BROKEN", "SCENE_SIGNAL_SPIKE", "SCENE_LA_DETECTOR", "SCENE_SEARCH"}
DYNAMIC_SUBTITLE_SCENES = {"SCENE_QR_DETECTOR", "SCENE_CAMERA_SCAN", "SCENE_WIN_ETAPE", "SCENE_WIN_ETAPE1", "SCENE_WIN_ETAPE2"}
LEVEL_SYNC_SCENES = {"SCENE_U_SON_PROTO"}
TUNER_SCENES = {"SCENE_LA_DETECTOR", "SCENE_SEARCH"}


@dataclass(frozen=True)
class LedPaletteEntry:
    red: int
    green: int
    blue: int
    brightness: int
    pulse: bool


@dataclass(frozen=True)
class SceneUiExpected:
    title: str
    subtitle: str
    show_title: bool
    show_subtitle: bool


def fail(msg: str) -> None:
    raise RuntimeError(msg)


def feed_fatal(line: str) -> None:
    for marker in FATAL_MARKERS:
        if marker in line:
            fail(f"fatal marker detected: {line}")


def read_lines(ser: serial.Serial, timeout_s: float) -> list[str]:
    lines: list[str] = []
    deadline = time.time() + timeout_s
    while time.time() < deadline:
        raw = ser.readline()
        if not raw:
            continue
        line = raw.decode("utf-8", errors="ignore").strip()
        if not line:
            continue
        feed_fatal(line)
        lines.append(line)
    return lines


def send_cmd(ser: serial.Serial, cmd: str, timeout_s: float = 1.0) -> list[str]:
    ser.write((cmd + "\n").encode("utf-8"))
    ser.flush()
    return read_lines(ser, timeout_s)


def fetch_ui_scene_status(ser: serial.Serial, retries: int = 10, timeout_s: float = 0.7) -> dict[str, Any]:
    for _ in range(retries):
        lines = send_cmd(ser, "UI_SCENE_STATUS", timeout_s)
        for line in reversed(lines):
            if not line.startswith("{"):
                continue
            try:
                payload = json.loads(line)
            except json.JSONDecodeError:
                continue
            if isinstance(payload, dict) and "scene_id" in payload:
                return payload
    fail("missing UI_SCENE_STATUS payload")


def fetch_hw_status(ser: serial.Serial, retries: int = 10, timeout_s: float = 0.7) -> dict[str, Any]:
    for _ in range(retries):
        lines = send_cmd(ser, "HW_STATUS", timeout_s)
        for line in reversed(lines):
            match = HW_STATUS_RE.search(line)
            if match:
                return {
                    "ws2812": int(match.group(1)),
                    "auto": int(match.group(2)),
                    "led": (int(match.group(3)), int(match.group(4)), int(match.group(5))),
                    "scene_id": match.group(6),
                    "raw": line,
                }
    fail("missing HW_STATUS line")


def parse_canonical_scenes(registry_path: Path) -> list[str]:
    text = registry_path.read_text(encoding="utf-8")
    block_match = re.search(r"constexpr\s+ScreenSceneDef\s+kScenes\[\]\s*=\s*\{(.*?)\n\};", text, flags=re.S)
    if not block_match:
        fail(f"unable to parse scene registry block: {registry_path}")
    block = block_match.group(1)
    scene_ids = re.findall(r'\{"([A-Z0-9_]+)",\s*\d+U,\s*\d+U\}', block)
    if not scene_ids:
        fail(f"no scenes parsed from registry: {registry_path}")
    return scene_ids


def parse_led_palette(palette_path: Path) -> tuple[dict[str, LedPaletteEntry], LedPaletteEntry]:
    text = palette_path.read_text(encoding="utf-8")
    block_match = re.search(
        r"constexpr\s+HardwareManager::LedPaletteEntry\s+kLedPalette\[\]\s*=\s*\{(.*?)\n\};",
        text,
        flags=re.S,
    )
    if not block_match:
        fail(f"unable to parse led palette block: {palette_path}")
    block = block_match.group(1)
    entries = re.findall(
        r'\{"([A-Z0-9_]+)",\s*(\d+)U,\s*(\d+)U,\s*(\d+)U,\s*(\d+)U,\s*(true|false)\}',
        block,
    )
    if not entries:
        fail(f"no palette entries parsed: {palette_path}")
    palette: dict[str, LedPaletteEntry] = {}
    default_entry: LedPaletteEntry | None = None
    for scene_id, red, green, blue, brightness, pulse in entries:
        entry = LedPaletteEntry(
            red=int(red),
            green=int(green),
            blue=int(blue),
            brightness=int(brightness),
            pulse=(pulse == "true"),
        )
        if scene_id == "__DEFAULT__":
            default_entry = entry
        else:
            palette[scene_id] = entry
    if default_entry is None:
        fail("missing __DEFAULT__ palette entry")
    return palette, default_entry


def apply_text_case(mode: str, value: str) -> str:
    token = mode.strip().lower()
    if token == "upper":
        return value.upper()
    if token == "lower":
        return value.lower()
    return value


def read_bool_chain(payload: dict[str, Any], paths: list[tuple[str, ...]], fallback: bool) -> bool:
    result = fallback
    for path in paths:
        cursor: Any = payload
        valid = True
        for key in path:
            if not isinstance(cursor, dict) or key not in cursor:
                valid = False
                break
            cursor = cursor[key]
        if valid and isinstance(cursor, bool):
            result = cursor
    return result


def read_str_chain(payload: dict[str, Any], paths: list[tuple[str, ...]], fallback: str) -> str:
    for path in paths:
        cursor: Any = payload
        valid = True
        for key in path:
            if not isinstance(cursor, dict) or key not in cursor:
                valid = False
                break
            cursor = cursor[key]
        if valid and isinstance(cursor, str) and cursor != "":
            return cursor
    return fallback


def expected_ui_from_screen(screen_payload: dict[str, Any]) -> SceneUiExpected:
    title = read_str_chain(screen_payload, [("title",), ("content", "title"), ("visual", "title")], "")
    subtitle = read_str_chain(screen_payload, [("subtitle",), ("content", "subtitle"), ("visual", "subtitle")], "")
    text_cfg = screen_payload.get("text")
    title = apply_text_case(text_cfg.get("title_case", "") if isinstance(text_cfg, dict) else "", title)
    subtitle = apply_text_case(text_cfg.get("subtitle_case", "") if isinstance(text_cfg, dict) else "", subtitle)
    show_title = read_bool_chain(
        screen_payload,
        [("show_title",), ("visual", "show_title"), ("content", "show_title"), ("text", "show_title")],
        False,
    )
    show_subtitle = read_bool_chain(
        screen_payload,
        [("show_subtitle",), ("visual", "show_subtitle"), ("text", "show_subtitle")],
        True,
    )
    return SceneUiExpected(
        title=title,
        subtitle=subtitle,
        show_title=show_title,
        show_subtitle=show_subtitle and bool(subtitle),
    )


def verify_ui_status(scene_id: str, ui_status: dict[str, Any], expected: SceneUiExpected) -> None:
    ui_scene_id = str(ui_status.get("scene_id", ""))
    if ui_scene_id != scene_id:
        fail(f"{scene_id}: UI scene mismatch got={ui_scene_id}")
    if str(ui_status.get("title", "")) != expected.title:
        fail(f"{scene_id}: title mismatch got={ui_status.get('title')!r} expected={expected.title!r}")
    if scene_id not in DYNAMIC_SUBTITLE_SCENES and str(ui_status.get("subtitle", "")) != expected.subtitle:
        fail(f"{scene_id}: subtitle mismatch got={ui_status.get('subtitle')!r} expected={expected.subtitle!r}")
    if bool(ui_status.get("show_title", False)) != expected.show_title:
        fail(
            f"{scene_id}: show_title mismatch got={bool(ui_status.get('show_title', False))} expected={expected.show_title}"
        )
    if bool(ui_status.get("show_subtitle", False)) != expected.show_subtitle:
        fail(
            f"{scene_id}: show_subtitle mismatch got={bool(ui_status.get('show_subtitle', False))} "
            f"expected={expected.show_subtitle}"
        )


def verify_led_status(scene_id: str, hw_status: dict[str, Any], palette_entry: LedPaletteEntry) -> None:
    if hw_status["ws2812"] != 1:
        fail(f"{scene_id}: ws2812 expected=1 got={hw_status['ws2812']}")
    if hw_status["auto"] != 1:
        fail(f"{scene_id}: led_auto expected=1 got={hw_status['auto']}")
    if hw_status["scene_id"] != scene_id:
        fail(f"{scene_id}: hw scene mismatch got={hw_status['scene_id']}")

    led = tuple(hw_status["led"])
    expected = (palette_entry.red, palette_entry.green, palette_entry.blue)

    if not palette_entry.pulse:
        if led != expected:
            fail(f"{scene_id}: exact LED mismatch got={led} expected={expected}")
        return

    if scene_id in LEVEL_SYNC_SCENES:
        if led == (0, 0, 0):
            return
        boosted_max = tuple(min(255, int(channel * 1.35) + 4) for channel in expected)
        if led[0] > boosted_max[0] or led[1] > boosted_max[1] or led[2] > boosted_max[2]:
            fail(f"{scene_id}: level-sync LED exceeds expected max got={led} expected_max={boosted_max}")
        return

    if scene_id in PATTERN_SCENES:
        if scene_id in TUNER_SCENES and sum(led) == 0:
            return
        if sum(led) <= 0:
            fail(f"{scene_id}: tolerant LED expected non-black pattern got={led}")
        return

    mins = tuple(int(channel * 0.15) for channel in expected)
    if not (
        mins[0] <= led[0] <= expected[0]
        and mins[1] <= led[1] <= expected[1]
        and mins[2] <= led[2] <= expected[2]
    ):
        fail(f"{scene_id}: tolerant LED mismatch got={led} expected_range={mins}..{expected}")


def ensure_led_auto_on(ser: serial.Serial) -> None:
    lines = send_cmd(ser, "HW_LED_AUTO ON", 1.0)
    if not any(ACK_HW_LED_AUTO in line for line in lines):
        fail("missing ACK HW_LED_AUTO ok=1")


def verify_scene(
    ser: serial.Serial,
    scene_id: str,
    expected_ui: SceneUiExpected,
    palette_entry: LedPaletteEntry,
) -> None:
    # Keep scene checks deterministic: stop ongoing audio around SCENE_GOTO so
    # scenario AUDIO_DONE transitions do not immediately override the target scene.
    send_cmd(ser, "AUDIO_STOP", 0.35)
    lines = send_cmd(ser, f"SCENE_GOTO {scene_id}", 1.0)
    send_cmd(ser, "AUDIO_STOP", 0.35)
    if not any(ACK_SCENE_GOTO in line for line in lines):
        fail(f"{scene_id}: missing ACK SCENE_GOTO ok=1")
    ensure_led_auto_on(ser)

    # Poll quickly to capture the target scene before runtime transitions kick in.
    ui_status: dict[str, Any] | None = None
    hw_status: dict[str, Any] | None = None
    for _ in range(24):
        ui_status = fetch_ui_scene_status(ser, retries=1, timeout_s=0.18)
        hw_status = fetch_hw_status(ser, retries=1, timeout_s=0.18)
        if hw_status["scene_id"] == scene_id and str(ui_status.get("scene_id", "")) == scene_id:
            break
        time.sleep(0.03)
    if ui_status is None or hw_status is None:
        fail(f"{scene_id}: missing ui/hw status")

    # Pulse scenes can legitimately report black on a given sample; retry briefly
    # and keep the first non-black snapshot before evaluating tolerant LED rules.
    if palette_entry.pulse and sum(hw_status["led"]) == 0:
        for _ in range(10):
            candidate = fetch_hw_status(ser, retries=2, timeout_s=0.3)
            if candidate["scene_id"] != scene_id:
                time.sleep(0.05)
                continue
            hw_status = candidate
            if sum(candidate["led"]) > 0:
                break
            time.sleep(0.05)

    verify_ui_status(scene_id, ui_status, expected_ui)
    verify_led_status(scene_id, hw_status, palette_entry)


def main() -> int:
    parser = argparse.ArgumentParser(description="Verify scene text + WS2812 matrix for Freenove")
    parser.add_argument("--port", required=True, help="Serial port (example: /dev/cu.usbmodemXXXX)")
    parser.add_argument("--baud", type=int, default=115200, help="Serial baud")
    parser.add_argument("--repo-root", default=str(Path(__file__).resolve().parents[4]), help="Repository root")
    parser.add_argument("--log", default="", help="Optional log file path")
    args = parser.parse_args()

    repo_root = Path(args.repo_root).resolve()
    firmware_root = repo_root / "hardware" / "firmware"
    registry_path = firmware_root / "hardware" / "libs" / "story" / "src" / "resources" / "screen_scene_registry.cpp"
    palette_path = firmware_root / "hardware" / "firmware" / "ui_freenove_allinone" / "src" / "drivers" / "board" / "hardware_manager.cpp"
    screens_dir = firmware_root / "data" / "story" / "screens"

    stamp = datetime.now().strftime("%Y%m%d-%H%M%S")
    log_path = Path(args.log) if args.log else (firmware_root / "artifacts" / "rc_live" / f"scene_text_led_matrix_{stamp}.log")
    log_path.parent.mkdir(parents=True, exist_ok=True)

    scene_ids = parse_canonical_scenes(registry_path)
    palette, default_palette = parse_led_palette(palette_path)

    expected_ui_by_scene: dict[str, SceneUiExpected] = {}
    for scene_id in scene_ids:
        screen_path = screens_dir / f"{scene_id}.json"
        if not screen_path.exists():
            fail(f"missing screen json: {screen_path}")
        with screen_path.open("r", encoding="utf-8") as fh:
            payload = json.load(fh)
        expected_ui_by_scene[scene_id] = expected_ui_from_screen(payload)

    with serial.Serial(args.port, args.baud, timeout=0.25) as ser, log_path.open("w", encoding="utf-8") as log:
        def log_line(text: str) -> None:
            print(text, flush=True)
            log.write(text + "\n")
            log.flush()

        log_line(f"[info] port={args.port} baud={args.baud}")
        log_line(f"[info] scenes={len(scene_ids)}")
        read_lines(ser, 0.5)
        ensure_led_auto_on(ser)

        for idx, scene_id in enumerate(scene_ids, start=1):
            entry = palette.get(scene_id, default_palette)
            verify_scene(ser, scene_id, expected_ui_by_scene[scene_id], entry)
            mode = "exact" if not entry.pulse else "tolerant"
            log_line(
                f"[ok] scene {idx}/{len(scene_ids)} {scene_id} mode={mode} led={entry.red}/{entry.green}/{entry.blue}"
            )

    print(f"[ok] matrix verification complete log={log_path}", flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
