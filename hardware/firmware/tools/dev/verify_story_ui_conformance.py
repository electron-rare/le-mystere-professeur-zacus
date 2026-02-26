#!/usr/bin/env python3
"""Deep UI conformance verification against data/story/screens payloads.

The script reuses deep transition traversal (all selectable on_event/after_ms
transitions) and validates that UI_SCENE_STATUS matches the local `/data`
screen payloads for each reached step.
"""

from __future__ import annotations

import argparse
import glob
import json
import re
import time
from collections import defaultdict, deque
from dataclasses import dataclass
from datetime import datetime
from pathlib import Path
from typing import Iterable

import serial

STATUS_RE = re.compile(
    r"\bSTATUS\b.*\bscenario=([A-Z0-9_]+)\b.*\bstep=([A-Z0-9_]+)\b.*\bscreen=([A-Z0-9_]+)\b"
)
ALIAS_RE = re.compile(r'\{"([A-Z0-9_]+)",\s*"([A-Z0-9_]+)"\}')

DYNAMIC_FIELD_ALLOWLIST: dict[str, set[str]] = {
    "SCENE_QR_DETECTOR": {"subtitle"},
    "SCENE_CAMERA_SCAN": {"subtitle"},
    "SCENE_WIN_ETAPE": {"subtitle"},
    "SCENE_WIN_ETAPE1": {"subtitle"},
    "SCENE_WIN_ETAPE2": {"subtitle"},
}


@dataclass
class TransitionCase:
    scenario_id: str
    source_step: str
    index: int
    transition_id: str
    trigger: str
    event_type: str
    event_name: str
    after_ms: int
    priority: int
    target_step: str
    select_name: str | None = None
    selectable: bool = False


def event_name_matches(expected: str, actual: str) -> bool:
    if expected == "":
        return True
    return expected == actual


def selected_on_event(
    step_transitions: list[TransitionCase], event_type: str, actual_name: str
) -> TransitionCase | None:
    selected = None
    for transition in step_transitions:
        if transition.trigger != "on_event":
            continue
        if transition.event_type != event_type:
            continue
        if not event_name_matches(transition.event_name, actual_name):
            continue
        if selected is None or transition.priority > selected.priority:
            selected = transition
    return selected


def selected_after_ms(
    step_transitions: list[TransitionCase], elapsed_ms: int
) -> TransitionCase | None:
    selected = None
    for transition in step_transitions:
        if transition.trigger != "after_ms":
            continue
        if elapsed_ms < transition.after_ms:
            continue
        if selected is None or transition.priority > selected.priority:
            selected = transition
    return selected


def read_lines(ser: serial.Serial, timeout_s: float) -> list[str]:
    lines: list[str] = []
    end = time.time() + timeout_s
    while time.time() < end:
        try:
            raw = ser.readline()
        except serial.SerialException:
            # USB serial can transiently flap during board reset.
            time.sleep(0.2)
            break
        if not raw:
            continue
        line = raw.decode("utf-8", errors="ignore").strip()
        if line:
            lines.append(line)
    return lines


def send_cmd(ser: serial.Serial, cmd: str, timeout_s: float = 1.0) -> list[str]:
    try:
        ser.write((cmd + "\n").encode("utf-8"))
        ser.flush()
    except serial.SerialException:
        time.sleep(0.2)
        return []
    time.sleep(0.16)
    return read_lines(ser, timeout_s)


def fetch_status(
    ser: serial.Serial, retries: int = 8, timeout_s: float = 0.7
) -> tuple[str, str, str, list[str]] | None:
    for _ in range(retries):
        lines = send_cmd(ser, "STATUS", timeout_s)
        for line in reversed(lines):
            match = STATUS_RE.search(line)
            if match:
                return match.group(1), match.group(2), match.group(3), lines
    return None


def fetch_ui_scene_status(
    ser: serial.Serial, retries: int = 8, timeout_s: float = 0.6
) -> tuple[dict, list[str]] | None:
    for _ in range(retries):
        lines = send_cmd(ser, "UI_SCENE_STATUS", timeout_s)
        for line in reversed(lines):
            start = line.find("{")
            if start < 0:
                continue
            try:
                payload = json.loads(line[start:])
            except json.JSONDecodeError:
                continue
            if isinstance(payload, dict) and "scene_id" in payload:
                return payload, lines
    return None


def _event_cmd(event_type: str, event_name: str | None) -> str:
    event_name_text = event_name if event_name else ""
    if event_type:
        return f"SC_EVENT {event_type} {event_name_text}".strip()
    return f"SC_EVENT_RAW {event_name_text}".strip()


def _scene_aliases(registry_path: Path) -> dict[str, str]:
    aliases: dict[str, str] = {}
    if not registry_path.exists():
        return aliases
    text = registry_path.read_text(encoding="utf-8")
    for alias, canonical in ALIAS_RE.findall(text):
        aliases[alias] = canonical
    return aliases


def _canonical_scene_id(scene_id: str, aliases: dict[str, str]) -> str:
    return aliases.get(scene_id, scene_id)


def _normalize_effect_token(token: str) -> str:
    normalized = token.strip().lower().replace("-", "_")
    if normalized in {"", "steady"}:
        return "none"
    if normalized == "camera_flash":
        return "glitch"
    if normalized == "reward":
        return "celebrate"
    return normalized


def _normalize_transition_token(token: str) -> str:
    normalized = token.strip().lower().replace("-", "_")
    if normalized in {"", "off"}:
        return "none"
    if normalized in {"crossfade"}:
        return "fade"
    if normalized in {"left"}:
        return "slide_left"
    if normalized in {"right"}:
        return "slide_right"
    if normalized in {"up"}:
        return "slide_up"
    if normalized in {"down"}:
        return "slide_down"
    if normalized in {"zoom_in"}:
        return "zoom"
    if normalized in {"flash", "camera_flash"}:
        return "glitch"
    if normalized in {"wipe"}:
        return "slide_left"
    return normalized


def _parse_hex_rgb(value: str | None) -> int | None:
    if not value:
        return None
    text = value.strip()
    if text.startswith("#"):
        text = text[1:]
    if not text:
        return None
    try:
        return int(text, 16)
    except ValueError:
        return None


def _rgb24_to_rgb565(value: int) -> int:
    red = (value >> 16) & 0xFF
    green = (value >> 8) & 0xFF
    blue = value & 0xFF
    return ((red & 0xF8) << 8) | ((green & 0xFC) << 3) | (blue >> 3)


def _apply_text_case(mode: str, value: str) -> str:
    normalized = mode.strip().lower()
    if normalized == "upper":
        return value.upper()
    if normalized == "lower":
        return value.lower()
    return value


def _read_bool_chain(payload: dict, keys: list[tuple[str, ...]], fallback: bool) -> bool:
    result = fallback
    for path in keys:
        cursor = payload
        valid = True
        for key in path:
            if not isinstance(cursor, dict) or key not in cursor:
                valid = False
                break
            cursor = cursor[key]
        if valid and isinstance(cursor, bool):
            result = cursor
    return result


def _read_int_chain(payload: dict, keys: list[tuple[str, ...]], fallback: int) -> int:
    for path in keys:
        cursor = payload
        valid = True
        for key in path:
            if not isinstance(cursor, dict) or key not in cursor:
                valid = False
                break
            cursor = cursor[key]
        if valid and isinstance(cursor, int):
            return cursor
    return fallback


def _read_str_chain(payload: dict, keys: list[tuple[str, ...]], fallback: str) -> str:
    for path in keys:
        cursor = payload
        valid = True
        for key in path:
            if not isinstance(cursor, dict) or key not in cursor:
                valid = False
                break
            cursor = cursor[key]
        if valid and isinstance(cursor, str) and cursor != "":
            return cursor
    return fallback


def _extract_timeline_keyframes(
    payload: dict, base_effect: str, base_speed: int, base_theme: tuple[int, int, int]
) -> list[tuple[str, int, tuple[int, int, int]]]:
    timeline_nodes: list[dict] = []
    timeline = payload.get("timeline")
    visual_timeline = payload.get("visual", {}).get("timeline") if isinstance(payload.get("visual"), dict) else None
    if isinstance(timeline, list):
        timeline_nodes = [node for node in timeline if isinstance(node, dict)]
    elif isinstance(timeline, dict):
        keyframes = timeline.get("keyframes")
        frames = timeline.get("frames")
        if isinstance(keyframes, list):
            timeline_nodes = [node for node in keyframes if isinstance(node, dict)]
        elif isinstance(frames, list):
            timeline_nodes = [node for node in frames if isinstance(node, dict)]
    elif isinstance(visual_timeline, list):
        timeline_nodes = [node for node in visual_timeline if isinstance(node, dict)]
    elif isinstance(visual_timeline, dict):
        keyframes = visual_timeline.get("keyframes")
        frames = visual_timeline.get("frames")
        if isinstance(keyframes, list):
            timeline_nodes = [node for node in keyframes if isinstance(node, dict)]
        elif isinstance(frames, list):
            timeline_nodes = [node for node in frames if isinstance(node, dict)]

    result: list[tuple[str, int, tuple[int, int, int]]] = [(base_effect, base_speed, base_theme)]
    current_effect = base_effect
    current_speed = base_speed
    current_theme = base_theme
    for node in timeline_nodes:
        effect_token = node.get("effect") or node.get("fx")
        if isinstance(effect_token, str) and effect_token:
            current_effect = _normalize_effect_token(effect_token)
        for speed_key in ("speed_ms", "effect_speed_ms", "speed"):
            speed_value = node.get(speed_key)
            if isinstance(speed_value, int):
                current_speed = speed_value
                break

        frame_bg = None
        frame_accent = None
        frame_text = None
        theme_node = node.get("theme")
        if isinstance(theme_node, dict):
            frame_bg = _parse_hex_rgb(theme_node.get("bg"))
            frame_accent = _parse_hex_rgb(theme_node.get("accent"))
            frame_text = _parse_hex_rgb(theme_node.get("text"))
        frame_bg = frame_bg if frame_bg is not None else _parse_hex_rgb(node.get("bg"))
        frame_accent = frame_accent if frame_accent is not None else _parse_hex_rgb(node.get("accent"))
        frame_text = frame_text if frame_text is not None else _parse_hex_rgb(node.get("text"))
        frame_bg_565 = _rgb24_to_rgb565(frame_bg) if frame_bg is not None else None
        frame_accent_565 = _rgb24_to_rgb565(frame_accent) if frame_accent is not None else None
        frame_text_565 = _rgb24_to_rgb565(frame_text) if frame_text is not None else None
        current_theme = (
            frame_bg_565 if frame_bg_565 is not None else current_theme[0],
            frame_accent_565 if frame_accent_565 is not None else current_theme[1],
            frame_text_565 if frame_text_565 is not None else current_theme[2],
        )
        result.append((current_effect, current_speed, current_theme))
    return result


def _scene_expected(payload: dict) -> dict:
    title = _read_str_chain(
        payload,
        [("title",), ("content", "title"), ("visual", "title")],
        "",
    )
    subtitle = _read_str_chain(
        payload,
        [("subtitle",), ("content", "subtitle"), ("visual", "subtitle")],
        "",
    )
    symbol = _read_str_chain(
        payload,
        [("symbol",), ("content", "symbol"), ("visual", "symbol")],
        "",
    )
    effect = _normalize_effect_token(
        _read_str_chain(payload, [("effect",), ("visual", "effect"), ("content", "effect")], "none")
    )
    effect_speed_ms = _read_int_chain(
        payload,
        [("effect_speed_ms",), ("visual", "effect_speed_ms")],
        0,
    )
    transition = _normalize_transition_token(
        _read_str_chain(payload, [("transition", "effect"), ("transition", "type"), ("visual", "transition")], "fade")
    )
    transition_ms = _read_int_chain(
        payload,
        [("transition", "duration_ms"), ("transition", "ms"), ("visual", "transition_ms")],
        240,
    )
    show_title = _read_bool_chain(
        payload,
        [("show_title",), ("visual", "show_title"), ("content", "show_title"), ("text", "show_title")],
        False,
    )
    show_subtitle = _read_bool_chain(
        payload,
        [("show_subtitle",), ("visual", "show_subtitle"), ("text", "show_subtitle")],
        True,
    )
    show_symbol = _read_bool_chain(
        payload,
        [("show_symbol",), ("visual", "show_symbol"), ("content", "show_symbol"), ("text", "show_symbol")],
        True,
    )

    title = _apply_text_case(payload.get("text", {}).get("title_case", "") if isinstance(payload.get("text"), dict) else "", title)
    subtitle = _apply_text_case(
        payload.get("text", {}).get("subtitle_case", "") if isinstance(payload.get("text"), dict) else "", subtitle
    )

    bg = _parse_hex_rgb(
        _read_str_chain(payload, [("theme", "bg"), ("visual", "theme", "bg"), ("bg",)], "")
    )
    accent = _parse_hex_rgb(
        _read_str_chain(payload, [("theme", "accent"), ("visual", "theme", "accent"), ("accent",)], "")
    )
    text = _parse_hex_rgb(
        _read_str_chain(payload, [("theme", "text"), ("visual", "theme", "text"), ("text",)], "")
    )
    if bg is None:
        bg = 0
    if accent is None:
        accent = 0
    if text is None:
        text = 0
    theme = (_rgb24_to_rgb565(bg), _rgb24_to_rgb565(accent), _rgb24_to_rgb565(text))
    timeline = _extract_timeline_keyframes(payload, effect, effect_speed_ms, theme)
    allowed_effects = sorted({frame_effect for frame_effect, _, _ in timeline})
    allowed_speeds = sorted({frame_speed for _, frame_speed, _ in timeline})
    allowed_themes = sorted({frame_theme for _, _, frame_theme in timeline})
    return {
        "title": title,
        "subtitle": subtitle,
        "symbol": symbol,
        "show_title": show_title,
        "show_subtitle": show_subtitle and bool(subtitle),
        "show_symbol": show_symbol,
        "transition": transition,
        "transition_ms": transition_ms,
        "allowed_effects": allowed_effects,
        "allowed_speeds": allowed_speeds,
        "allowed_themes": allowed_themes,
        "timeline_dynamic": len(timeline) > 1,
    }


def _fnv1a_hash(payload_text: str) -> int:
    value = 2166136261
    for byte in payload_text.encode("utf-8"):
        value ^= byte
        value = (value * 16777619) & 0xFFFFFFFF
    return value


def _load_payload_for_scene(
    firmware_root: Path, scene_id: str, aliases: dict[str, str]
) -> tuple[dict | None, Path | None, str]:
    canonical = _canonical_scene_id(scene_id, aliases)
    story_path = firmware_root / "data" / "story" / "screens" / f"{canonical}.json"
    if story_path.exists():
        return json.loads(story_path.read_text(encoding="utf-8")), story_path, canonical
    return None, None, canonical


def build_model(
    scenario_path: Path,
) -> tuple[
    str,
    str,
    dict[str, dict],
    dict[str, list[TransitionCase]],
    list[TransitionCase],
    dict[str, list[TransitionCase]],
]:
    payload = json.loads(scenario_path.read_text(encoding="utf-8"))
    scenario_id = payload["id"]
    initial_step = payload["initial_step"]
    steps = {step["step_id"]: step for step in payload["steps"]}

    transitions_by_step: dict[str, list[TransitionCase]] = defaultdict(list)
    all_transitions: list[TransitionCase] = []

    for step in payload["steps"]:
        source_step = step["step_id"]
        for index, tr in enumerate(step.get("transitions", [])):
            transition = TransitionCase(
                scenario_id=scenario_id,
                source_step=source_step,
                index=index,
                transition_id=tr.get("id", f"{source_step}:{index}"),
                trigger=tr.get("trigger", ""),
                event_type=tr.get("event_type", ""),
                event_name=(tr.get("event_name") or ""),
                after_ms=int(tr.get("after_ms", 0) or 0),
                priority=int(tr.get("priority", 0) or 0),
                target_step=tr.get("target_step_id", ""),
            )
            transitions_by_step[source_step].append(transition)
            all_transitions.append(transition)

    for transitions in transitions_by_step.values():
        names_by_type: dict[str, set[str]] = defaultdict(set)
        for transition in transitions:
            if transition.trigger == "on_event" and transition.event_name:
                names_by_type[transition.event_type].add(transition.event_name)

        for transition in transitions:
            if transition.trigger == "on_event":
                candidates = list(names_by_type[transition.event_type])
                if transition.event_name:
                    candidates.insert(0, transition.event_name)
                candidates.extend(
                    [
                        "__AUTO_EVT__",
                        "BTN_NEXT",
                        "UNLOCK",
                        "UNLOCK_QR",
                        "AUDIO_DONE",
                        "FORCE_DONE",
                        "ACK_WIN1",
                        "ACK_WIN2",
                        "ETAPE2_DUE",
                        "LA_TIMEOUT",
                        "ANY",
                    ]
                )
                deduped: list[str] = []
                for candidate in candidates:
                    if candidate not in deduped:
                        deduped.append(candidate)
                for actual_name in deduped:
                    selected = selected_on_event(
                        transitions, transition.event_type, actual_name
                    )
                    if selected is transition:
                        transition.selectable = True
                        transition.select_name = actual_name
                        break
            elif transition.trigger == "after_ms":
                selected = selected_after_ms(transitions, transition.after_ms)
                if selected is transition:
                    transition.selectable = True

    edges: dict[str, list[TransitionCase]] = defaultdict(list)
    for transition in all_transitions:
        if transition.selectable:
            edges[transition.source_step].append(transition)

    paths: dict[str, list[TransitionCase]] = {initial_step: []}
    queue: deque[str] = deque([initial_step])
    while queue:
        current = queue.popleft()
        for transition in edges.get(current, []):
            target = transition.target_step
            if not target or target in paths:
                continue
            paths[target] = paths[current] + [transition]
            queue.append(target)

    return (
        scenario_id,
        initial_step,
        steps,
        transitions_by_step,
        all_transitions,
        paths,
    )


def verify(
    port: str,
    baud: int,
    scenario_files: Iterable[Path],
    firmware_root: Path,
    log_path: Path,
) -> int:
    models = [build_model(path) for path in scenario_files]
    if not models:
        print("No scenarios found to verify", flush=True)
        return 1

    aliases = _scene_aliases(
        firmware_root / "hardware" / "libs" / "story" / "src" / "resources" / "screen_scene_registry.cpp"
    )
    log_path.parent.mkdir(parents=True, exist_ok=True)

    blocked: list[TransitionCase] = []
    unreachable: list[TransitionCase] = []
    runtime_results: list[tuple[bool, str, str, str]] = []
    missing_payload_logs: list[str] = []

    ser = serial.Serial(port, baud, timeout=0.2)
    time.sleep(1.0)
    ser.reset_input_buffer()

    with log_path.open("w", encoding="utf-8") as log:
        def log_line(text: str) -> None:
            print(text, flush=True)
            log.write(text + "\n")
            log.flush()

        def collect_missing_payload(lines: list[str]) -> None:
            for line in lines:
                if "[UI] missing scene payload" in line:
                    missing_payload_logs.append(line)

        def verify_ui_status(
            status_scenario: str,
            status_step: str,
            status_scene: str,
            context: str,
        ) -> tuple[bool, str]:
            ui_status_result = fetch_ui_scene_status(ser)
            if ui_status_result is None:
                return False, "ui_scene_status_missing"
            ui_status, ui_lines = ui_status_result
            collect_missing_payload(ui_lines)

            if not bool(ui_status.get("valid", False)):
                return False, "ui_scene_status_invalid"

            ui_scene = str(ui_status.get("scene_id", ""))
            ui_step = str(ui_status.get("step_id", ""))
            ui_scenario = str(ui_status.get("scenario_id", ""))
            if ui_scene != status_scene:
                return False, f"ui_scene_mismatch:{ui_scene}->{status_scene}"
            if ui_step != status_step:
                return False, f"ui_step_mismatch:{ui_step}->{status_step}"
            if ui_scenario != status_scenario:
                return False, f"ui_scenario_mismatch:{ui_scenario}->{status_scenario}"

            payload, payload_path, canonical_scene = _load_payload_for_scene(
                firmware_root, status_scene, aliases
            )
            if payload is None or payload_path is None:
                return False, f"payload_missing:{canonical_scene}"

            expected = _scene_expected(payload)
            allowed_fields = DYNAMIC_FIELD_ALLOWLIST.get(canonical_scene, set())
            mismatches: list[str] = []

            def check(field: str, actual_value, expected_value) -> None:
                if field in allowed_fields:
                    return
                if actual_value != expected_value:
                    mismatches.append(f"{field}={actual_value!r} expected={expected_value!r}")

            check("title", str(ui_status.get("title", "")), expected["title"])
            check("subtitle", str(ui_status.get("subtitle", "")), expected["subtitle"])
            check("symbol", str(ui_status.get("symbol", "")), expected["symbol"])
            check("show_title", bool(ui_status.get("show_title", False)), expected["show_title"])
            check(
                "show_subtitle",
                bool(ui_status.get("show_subtitle", False)),
                expected["show_subtitle"],
            )
            check("show_symbol", bool(ui_status.get("show_symbol", False)), expected["show_symbol"])
            check("transition", str(ui_status.get("transition", "")), expected["transition"])
            check(
                "transition_ms",
                int(ui_status.get("transition_ms", 0) or 0),
                expected["transition_ms"],
            )

            if not expected["timeline_dynamic"]:
                ui_effect = str(ui_status.get("effect", ""))
                if ui_effect not in expected["allowed_effects"]:
                    mismatches.append(
                        f"effect={ui_effect!r} expected_in={expected['allowed_effects']!r}"
                    )
                ui_speed = int(ui_status.get("effect_speed_ms", 0) or 0)
                if ui_speed not in expected["allowed_speeds"]:
                    mismatches.append(
                        f"effect_speed_ms={ui_speed} expected_in={expected['allowed_speeds']!r}"
                    )
                ui_theme = (
                    int(ui_status.get("bg_rgb", 0) or 0),
                    int(ui_status.get("accent_rgb", 0) or 0),
                    int(ui_status.get("text_rgb", 0) or 0),
                )
                if ui_theme not in expected["allowed_themes"]:
                    mismatches.append(
                        f"theme={ui_theme!r} expected_in={expected['allowed_themes']!r}"
                    )

            payload_origin = str(ui_status.get("payload_origin", ""))
            payload_crc = int(ui_status.get("payload_crc", 0) or 0)
            if payload_origin and not payload_origin.startswith("/sd/"):
                local_origin_path = firmware_root / "data" / payload_origin.lstrip("/")
                if local_origin_path.exists():
                    origin_text = local_origin_path.read_text(encoding="utf-8")
                    expected_crc = _fnv1a_hash(origin_text)
                    if expected_crc != payload_crc:
                        mismatches.append(
                            f"payload_crc={payload_crc} expected_crc={expected_crc} origin={payload_origin}"
                        )
                else:
                    mismatches.append(f"payload_origin_missing_local:{payload_origin}")
            else:
                canonical_text = payload_path.read_text(encoding="utf-8")
                expected_crc = _fnv1a_hash(canonical_text)
                if expected_crc != payload_crc:
                    mismatches.append(
                        f"payload_crc={payload_crc} expected_crc={expected_crc} canonical={canonical_scene}"
                    )

            if mismatches:
                return False, f"{context} " + "; ".join(mismatches)
            return True, "ok"

        for (
            scenario_id,
            initial_step,
            _steps,
            _transitions_by_step,
            all_transitions,
            paths,
        ) in models:
            log_line(
                f"=== {scenario_id} initial={initial_step} transitions={len(all_transitions)} ==="
            )

            for transition in all_transitions:
                if not transition.selectable:
                    blocked.append(transition)
                    log_line(
                        f"STATIC_BLOCKED {scenario_id} {transition.transition_id} "
                        f"source={transition.source_step} trigger={transition.trigger} "
                        f"type={transition.event_type} name={transition.event_name!r} "
                        f"prio={transition.priority} target={transition.target_step}"
                    )

            testable = [transition for transition in all_transitions if transition.selectable]
            for idx, transition in enumerate(testable, start=1):
                if transition.source_step not in paths:
                    unreachable.append(transition)
                    log_line(
                        f"STATIC_UNREACHABLE_SOURCE {scenario_id} {transition.transition_id} "
                        f"source={transition.source_step}"
                    )
                    continue

                log_line(f"[test] {scenario_id} {idx}/{len(testable)} {transition.transition_id}")

                reset_lines = send_cmd(ser, "RESET", 1.0)
                collect_missing_payload(reset_lines)
                load_lines = send_cmd(ser, f"SC_LOAD {scenario_id}", 1.4)
                collect_missing_payload(load_lines)
                if not any(
                    f"ACK SC_LOAD id={scenario_id} ok=1" in line for line in load_lines
                ):
                    runtime_results.append(
                        (False, scenario_id, transition.transition_id, "load_failed")
                    )
                    log_line(f"FAIL {scenario_id} {transition.transition_id} load_failed")
                    continue

                status = fetch_status(ser)
                if status is None:
                    runtime_results.append(
                        (
                            False,
                            scenario_id,
                            transition.transition_id,
                            "status_missing_after_load",
                        )
                    )
                    log_line(
                        f"FAIL {scenario_id} {transition.transition_id} status_missing_after_load"
                    )
                    continue
                status_scenario, status_step, status_screen, status_lines = status
                collect_missing_payload(status_lines)
                if status_scenario != scenario_id:
                    runtime_results.append(
                        (
                            False,
                            scenario_id,
                            transition.transition_id,
                            f"scenario_mismatch:{status_scenario}",
                        )
                    )
                    log_line(
                        f"FAIL {scenario_id} {transition.transition_id} scenario_mismatch={status_scenario}"
                    )
                    continue

                ui_ok, ui_reason = verify_ui_status(
                    status_scenario,
                    status_step,
                    status_screen,
                    "after_load",
                )
                if not ui_ok:
                    runtime_results.append(
                        (
                            False,
                            scenario_id,
                            transition.transition_id,
                            f"ui_mismatch:{ui_reason}",
                        )
                    )
                    log_line(
                        f"FAIL {scenario_id} {transition.transition_id} ui_mismatch={ui_reason}"
                    )
                    continue

                path_edges = paths[transition.source_step]
                path_ok = True
                path_error = ""
                for edge in path_edges:
                    if edge.trigger == "on_event":
                        edge_lines = send_cmd(
                            ser, _event_cmd(edge.event_type, edge.select_name), 1.2
                        )
                        collect_missing_payload(edge_lines)
                        edge_status = fetch_status(ser, retries=8, timeout_s=0.6)
                        if edge_status is None:
                            path_ok = False
                            path_error = f"path_status_missing:{edge.transition_id}"
                            break
                        edge_scenario, edge_step, edge_screen, edge_status_lines = edge_status
                        collect_missing_payload(edge_status_lines)
                        if edge_step != edge.target_step:
                            path_ok = False
                            path_error = (
                                f"path_step_mismatch:{edge.transition_id}:"
                                f"{edge_step}->{edge.target_step}"
                            )
                            break
                        ui_ok, ui_reason = verify_ui_status(
                            edge_scenario, edge_step, edge_screen, f"path:{edge.transition_id}"
                        )
                        if not ui_ok:
                            path_ok = False
                            path_error = f"path_ui_mismatch:{ui_reason}"
                            break
                    elif edge.trigger == "after_ms":
                        deadline = time.time() + (edge.after_ms / 1000.0) + 3.0
                        hit = False
                        while time.time() < deadline:
                            edge_status = fetch_status(ser, retries=1, timeout_s=0.7)
                            if edge_status is None:
                                continue
                            edge_scenario, edge_step, edge_screen, edge_status_lines = edge_status
                            collect_missing_payload(edge_status_lines)
                            if edge_step == edge.target_step:
                                ui_ok, ui_reason = verify_ui_status(
                                    edge_scenario,
                                    edge_step,
                                    edge_screen,
                                    f"path_after:{edge.transition_id}",
                                )
                                if not ui_ok:
                                    path_ok = False
                                    path_error = f"path_ui_mismatch:{ui_reason}"
                                hit = path_ok
                                break
                        if not hit:
                            if not path_error:
                                path_error = f"path_after_ms_timeout:{edge.transition_id}"
                            path_ok = False
                            break
                    else:
                        path_ok = False
                        path_error = f"unsupported_path_trigger:{edge.trigger}"
                        break

                if not path_ok:
                    runtime_results.append(
                        (False, scenario_id, transition.transition_id, path_error)
                    )
                    log_line(f"FAIL {scenario_id} {transition.transition_id} {path_error}")
                    continue

                source_status = fetch_status(ser, retries=5, timeout_s=0.6)
                if source_status is None:
                    runtime_results.append(
                        (
                            False,
                            scenario_id,
                            transition.transition_id,
                            "status_missing_before_transition",
                        )
                    )
                    log_line(
                        f"FAIL {scenario_id} {transition.transition_id} status_missing_before_transition"
                    )
                    continue
                source_scenario, current_step, current_screen, source_status_lines = source_status
                collect_missing_payload(source_status_lines)
                if current_step != transition.source_step:
                    runtime_results.append(
                        (
                            False,
                            scenario_id,
                            transition.transition_id,
                            f"source_step_mismatch:{current_step}->{transition.source_step}",
                        )
                    )
                    log_line(
                        f"FAIL {scenario_id} {transition.transition_id} "
                        f"source_step_mismatch={current_step}->{transition.source_step}"
                    )
                    continue
                ui_ok, ui_reason = verify_ui_status(
                    source_scenario, current_step, current_screen, "before_transition"
                )
                if not ui_ok:
                    runtime_results.append(
                        (
                            False,
                            scenario_id,
                            transition.transition_id,
                            f"ui_mismatch:{ui_reason}",
                        )
                    )
                    log_line(
                        f"FAIL {scenario_id} {transition.transition_id} ui_mismatch={ui_reason}"
                    )
                    continue

                if transition.trigger == "on_event":
                    transition_lines = send_cmd(
                        ser, _event_cmd(transition.event_type, transition.select_name), 1.2
                    )
                    collect_missing_payload(transition_lines)
                    target_status = fetch_status(ser, retries=8, timeout_s=0.6)
                    if target_status is None:
                        runtime_results.append(
                            (
                                False,
                                scenario_id,
                                transition.transition_id,
                                "status_missing_after_transition",
                            )
                        )
                        log_line(
                            f"FAIL {scenario_id} {transition.transition_id} status_missing_after_transition"
                        )
                        continue
                    target_scenario, new_step, new_screen, target_status_lines = target_status
                    collect_missing_payload(target_status_lines)
                    if new_step != transition.target_step:
                        runtime_results.append(
                            (
                                False,
                                scenario_id,
                                transition.transition_id,
                                f"target_mismatch:{new_step}->{transition.target_step}",
                            )
                        )
                        log_line(
                            f"FAIL {scenario_id} {transition.transition_id} "
                            f"target_mismatch={new_step}->{transition.target_step}"
                        )
                        continue
                    ui_ok, ui_reason = verify_ui_status(
                        target_scenario, new_step, new_screen, "after_transition"
                    )
                    if not ui_ok:
                        runtime_results.append(
                            (
                                False,
                                scenario_id,
                                transition.transition_id,
                                f"ui_mismatch:{ui_reason}",
                            )
                        )
                        log_line(
                            f"FAIL {scenario_id} {transition.transition_id} ui_mismatch={ui_reason}"
                        )
                        continue
                    runtime_results.append((True, scenario_id, transition.transition_id, "ok"))
                    log_line(
                        f"PASS {scenario_id} {transition.transition_id} "
                        f"source={transition.source_step} target={transition.target_step} "
                        f"via={transition.event_type}:{transition.select_name}"
                    )
                elif transition.trigger == "after_ms":
                    deadline = time.time() + (transition.after_ms / 1000.0) + 4.0
                    hit = False
                    last_step = current_step
                    last_screen = current_screen
                    last_scenario = source_scenario
                    while time.time() < deadline:
                        target_status = fetch_status(ser, retries=1, timeout_s=0.7)
                        if target_status is None:
                            continue
                        scenario_seen, new_step, new_screen, target_status_lines = target_status
                        collect_missing_payload(target_status_lines)
                        last_scenario = scenario_seen
                        last_step = new_step
                        last_screen = new_screen
                        if new_step == transition.target_step:
                            ui_ok, ui_reason = verify_ui_status(
                                scenario_seen, new_step, new_screen, "after_ms_transition"
                            )
                            if not ui_ok:
                                runtime_results.append(
                                    (
                                        False,
                                        scenario_id,
                                        transition.transition_id,
                                        f"ui_mismatch:{ui_reason}",
                                    )
                                )
                                log_line(
                                    f"FAIL {scenario_id} {transition.transition_id} ui_mismatch={ui_reason}"
                                )
                                hit = False
                                break
                            hit = True
                            break
                    if not hit:
                        if runtime_results and runtime_results[-1][1] == scenario_id and runtime_results[-1][2] == transition.transition_id:
                            continue
                        runtime_results.append(
                            (
                                False,
                                scenario_id,
                                transition.transition_id,
                                f"after_ms_timeout:last_step={last_step}",
                            )
                        )
                        log_line(
                            f"FAIL {scenario_id} {transition.transition_id} "
                            f"after_ms_timeout expected={transition.target_step} got={last_step} screen={last_screen} scenario={last_scenario}"
                        )
                        continue
                    runtime_results.append((True, scenario_id, transition.transition_id, "ok"))
                    log_line(
                        f"PASS {scenario_id} {transition.transition_id} "
                        f"source={transition.source_step} target={transition.target_step} "
                        f"via=after_ms:{transition.after_ms}"
                    )
                else:
                    runtime_results.append(
                        (
                            False,
                            scenario_id,
                            transition.transition_id,
                            f"unsupported_trigger:{transition.trigger}",
                        )
                    )
                    log_line(
                        f"FAIL {scenario_id} {transition.transition_id} unsupported_trigger={transition.trigger}"
                    )

    ser.close()

    pass_count = sum(1 for status, *_ in runtime_results if status)
    fail_results = [result for result in runtime_results if not result[0]]

    print("--- SUMMARY ---", flush=True)
    print(f"log={log_path}", flush=True)
    print(
        f"runtime_tested={len(runtime_results)} pass={pass_count} fail={len(fail_results)} "
        f"static_blocked={len(blocked)} static_unreachable_source={len(unreachable)} "
        f"missing_payload_logs={len(missing_payload_logs)}",
        flush=True,
    )

    if blocked:
        print("blocked transitions:", flush=True)
        for transition in blocked:
            print(
                f"  {transition.scenario_id}:{transition.transition_id} source={transition.source_step} "
                f"type={transition.event_type} name={transition.event_name!r} prio={transition.priority}",
                flush=True,
            )

    if unreachable:
        print("unreachable source steps:", flush=True)
        for transition in unreachable:
            print(
                f"  {transition.scenario_id}:{transition.transition_id} source={transition.source_step}",
                flush=True,
            )

    if missing_payload_logs:
        print("missing payload runtime logs:", flush=True)
        for line in missing_payload_logs:
            print(f"  {line}", flush=True)

    if fail_results:
        print("runtime failures:", flush=True)
        for _, scenario_id, transition_id, reason in fail_results:
            print(f"  {scenario_id}:{transition_id} reason={reason}", flush=True)
        return 1

    if blocked or unreachable or missing_payload_logs:
        return 2

    return 0


def resolve_first_usbmodem_port() -> str | None:
    candidates = sorted(glob.glob("/dev/cu.usbmodem*"))
    if not candidates:
        return None
    return candidates[0]


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Deep UI conformance verification for all story scenarios"
    )
    parser.add_argument("--port", default="", help="Serial port (default: first /dev/cu.usbmodem*)")
    parser.add_argument("--baud", type=int, default=115200, help="Serial baud")
    parser.add_argument(
        "--scenarios-glob",
        default="data/story/scenarios/*.json",
        help="Glob for scenario JSON files",
    )
    parser.add_argument(
        "--firmware-root",
        default=".",
        help="Firmware root that contains data/story/screens and registry files",
    )
    parser.add_argument("--log", default="", help="Optional log path")
    args = parser.parse_args()

    port = args.port or resolve_first_usbmodem_port()
    if not port:
        print("No serial port found. Use --port /dev/cu.usbmodemXXX", flush=True)
        return 1

    scenario_files = [Path(path) for path in sorted(glob.glob(args.scenarios_glob))]
    firmware_root = Path(args.firmware_root).resolve()
    if args.log:
        log_path = Path(args.log)
    else:
        stamp = datetime.now().strftime("%Y%m%d-%H%M%S")
        log_path = Path(f"artifacts/rc_live/ui_conformance_verify_{stamp}.log")
    return verify(port, args.baud, scenario_files, firmware_root, log_path)


if __name__ == "__main__":
    raise SystemExit(main())
