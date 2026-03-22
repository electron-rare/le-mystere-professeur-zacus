#!/usr/bin/env python3
"""Validate Zacus scenario YAML files (V2 by default)."""

import argparse
import sys
from pathlib import Path

try:
    import yaml
except ImportError:  # pragma: no cover
    sys.exit("Missing dependency: install PyYAML (pip install pyyaml) to validate scenarios.")

REQUIRED_V1_KEYS = [
    "id",
    "title",
    "version",
    "players",
    "duration_minutes",
    "canon",
    "stations",
    "puzzles",
    "solution",
    "solution_unique",
]

REQUIRED_V2_KEYS = [
    "id",
    "title",
    "version",
    "players",
    "duration",
    "canon",
    "acts",
    "stations",
    "puzzles",
    "steps_narrative",
]

EVENT_TYPES = {
    "button",
    "serial",
    "timer",
    "audio_done",
    "unlock",
    "espnow",
    "action",
}


class ValidationError(Exception):
    pass


def load_yaml(path: Path) -> dict:
    try:
        data = yaml.safe_load(path.read_text())
    except OSError as exc:
        raise ValidationError(f"Cannot read {path}: {exc}")
    except yaml.YAMLError as exc:
        raise ValidationError(f"Invalid YAML in {path}: {exc}")
    if not isinstance(data, dict):
        raise ValidationError(f"Scenario {path} is not a YAML mapping")
    return data


def validate_players(players: dict) -> None:
    if not (isinstance(players, dict) and "min" in players and "max" in players):
        raise ValidationError("`players` must be a mapping with `min` and `max`")
    if not (isinstance(players["min"], int) and isinstance(players["max"], int)):
        raise ValidationError("`players.min` and `players.max` must be integers")
    if players["min"] > players["max"]:
        raise ValidationError("`players.min` must be <= `players.max`")
    if players["min"] < 6:
        raise ValidationError("`players.min` should be at least 6")


def validate_v1(data: dict) -> tuple[int, int]:
    missing = [key for key in REQUIRED_V1_KEYS if key not in data]
    if missing:
        raise ValidationError(f"Missing required keys: {', '.join(missing)}")

    validate_players(data["players"])

    duration = data["duration_minutes"]
    if not (isinstance(duration, dict) and "min" in duration and "max" in duration):
        raise ValidationError("`duration_minutes` must define `min` and `max`")
    if duration["min"] > duration["max"]:
        raise ValidationError("`duration_minutes.min` must be <= `duration_minutes.max`")
    if duration["min"] < 60 or duration["max"] > 90:
        raise ValidationError("Duration should stay within 60-90 minutes")

    stations = data["stations"]
    if not (isinstance(stations, list) and stations):
        raise ValidationError("`stations` must be a non-empty list")
    for idx, station in enumerate(stations, start=1):
        if not isinstance(station, dict) or not all(field in station for field in ("name", "focus", "clue")):
            raise ValidationError(f"Station #{idx} is missing one of name/focus/clue")

    puzzles = data["puzzles"]
    if not (isinstance(puzzles, list) and puzzles):
        raise ValidationError("`puzzles` must be a non-empty list")
    for idx, puzzle in enumerate(puzzles, start=1):
        if not isinstance(puzzle, dict) or not all(field in puzzle for field in ("id", "type", "clue", "effect")):
            raise ValidationError(f"Puzzle #{idx} is missing id/type/clue/effect")

    solution = data["solution"]
    if not isinstance(solution, dict) or not all(field in solution for field in ("culprit", "motive", "method", "proof")):
        raise ValidationError("`solution` must include culprit, motive, method, proof")

    proof = solution["proof"]
    if not (isinstance(proof, list) and len(proof) >= 3):
        raise ValidationError("`solution.proof` must be a list of at least 3 strings")

    if data.get("solution_unique") is not True:
        raise ValidationError("`solution_unique` must be true for canonical scenarios")

    canon = data["canon"]
    if not (isinstance(canon, dict) and isinstance(canon.get("timeline"), list) and canon["timeline"]):
        raise ValidationError("`canon.timeline` must be a non-empty list")

    for idx, entry in enumerate(canon["timeline"], start=1):
        if not isinstance(entry, dict) or not ("label" in entry and "note" in entry):
            raise ValidationError(f"canon.timeline entry #{idx} must include label and note")

    players = data["players"]
    return players["min"], players["max"]


def validate_v2(data: dict) -> tuple[int, int, int]:
    missing = [key for key in REQUIRED_V2_KEYS if key not in data]
    if missing:
        raise ValidationError(f"Missing required keys: {', '.join(missing)}")

    if not isinstance(data["version"], int):
        raise ValidationError("`version` must be an integer")

    validate_players(data["players"])

    duration = data["duration"]
    required_duration = ("act_1_minutes", "act_2_minutes", "total_minutes")
    if not isinstance(duration, dict) or not all(k in duration for k in required_duration):
        raise ValidationError("`duration` must define act_1_minutes, act_2_minutes, total_minutes")

    for key in required_duration:
        if not isinstance(duration[key], int) or duration[key] <= 0:
            raise ValidationError(f"`duration.{key}` must be a positive integer")
    if duration["total_minutes"] < max(duration["act_1_minutes"], duration["act_2_minutes"]):
        raise ValidationError("`duration.total_minutes` must be >= each act duration")

    canon = data["canon"]
    if not isinstance(canon, dict):
        raise ValidationError("`canon` must be a mapping")
    for key in ("introduction", "stakes"):
        if not isinstance(canon.get(key), str) or not canon[key].strip():
            raise ValidationError(f"`canon.{key}` must be a non-empty string")

    acts = data["acts"]
    if not isinstance(acts, list) or not acts:
        raise ValidationError("`acts` must be a non-empty list")

    runtime_steps_from_acts: list[str] = []
    for idx, act in enumerate(acts, start=1):
        if not isinstance(act, dict):
            raise ValidationError(f"Act #{idx} must be a mapping")
        for field in ("act", "title", "goal", "runtime_steps"):
            if field not in act:
                raise ValidationError(f"Act #{idx} is missing `{field}`")
        if not isinstance(act["runtime_steps"], list) or not act["runtime_steps"]:
            raise ValidationError(f"Act #{idx} `runtime_steps` must be a non-empty list")
        for step_id in act["runtime_steps"]:
            if not isinstance(step_id, str) or not step_id:
                raise ValidationError(f"Act #{idx} has invalid runtime step id")
            runtime_steps_from_acts.append(step_id)

    steps_narrative = data["steps_narrative"]
    if not isinstance(steps_narrative, list) or not steps_narrative:
        raise ValidationError("`steps_narrative` must be a non-empty list")

    narrative_step_ids: list[str] = []
    seen_step_ids: set[str] = set()
    for idx, step in enumerate(steps_narrative, start=1):
        if not isinstance(step, dict):
            raise ValidationError(f"steps_narrative entry #{idx} must be a mapping")
        for field in ("step_id", "scene", "act", "timebox_minutes", "narrative"):
            if field not in step:
                raise ValidationError(f"steps_narrative entry #{idx} is missing `{field}`")
        step_id = step["step_id"]
        if not isinstance(step_id, str) or not step_id:
            raise ValidationError(f"steps_narrative entry #{idx} has invalid `step_id`")
        if step_id in seen_step_ids:
            raise ValidationError(f"Duplicate step_id in steps_narrative: {step_id}")
        seen_step_ids.add(step_id)
        narrative_step_ids.append(step_id)

    stations = data["stations"]
    if not isinstance(stations, list) or not stations:
        raise ValidationError("`stations` must be a non-empty list")
    for idx, station in enumerate(stations, start=1):
        if not isinstance(station, dict):
            raise ValidationError(f"Station #{idx} must be a mapping")
        for field in ("id", "act", "name", "focus", "firmware_anchor_step", "clue"):
            if field not in station:
                raise ValidationError(f"Station #{idx} is missing `{field}`")
        anchor = station["firmware_anchor_step"]
        if anchor not in seen_step_ids:
            raise ValidationError(f"Station #{idx} references unknown firmware step: {anchor}")

    puzzles = data["puzzles"]
    if not isinstance(puzzles, list) or not puzzles:
        raise ValidationError("`puzzles` must be a non-empty list")
    for idx, puzzle in enumerate(puzzles, start=1):
        if not isinstance(puzzle, dict):
            raise ValidationError(f"Puzzle #{idx} must be a mapping")
        for field in ("id", "act", "type", "objective", "firmware_step"):
            if field not in puzzle:
                raise ValidationError(f"Puzzle #{idx} is missing `{field}`")
        if puzzle["firmware_step"] not in seen_step_ids:
            raise ValidationError(
                f"Puzzle #{idx} references unknown firmware_step: {puzzle['firmware_step']}"
            )

    firmware = data.get("firmware", {})
    firmware_step_ids: list[str] = []
    if isinstance(firmware, dict) and isinstance(firmware.get("steps"), list):
        raw_steps = firmware["steps"]
        if not raw_steps:
            raise ValidationError("`firmware.steps` must not be empty when defined")
        for idx, step in enumerate(raw_steps, start=1):
            if not isinstance(step, dict):
                raise ValidationError(f"`firmware.steps[{idx}]` must be a mapping")
            for field in ("step_id", "screen_scene_id", "audio_pack_id", "actions", "apps", "transitions"):
                if field not in step:
                    raise ValidationError(f"`firmware.steps[{idx}]` is missing `{field}`")
            step_id = step["step_id"]
            if not isinstance(step_id, str) or not step_id:
                raise ValidationError(f"`firmware.steps[{idx}].step_id` must be a non-empty string")
            if step_id in firmware_step_ids:
                raise ValidationError(f"Duplicate step_id in `firmware.steps`: {step_id}")
            if not isinstance(step["screen_scene_id"], str) or not step["screen_scene_id"]:
                raise ValidationError(f"`firmware.steps[{idx}].screen_scene_id` must be a non-empty string")
            if not isinstance(step["audio_pack_id"], str):
                raise ValidationError(f"`firmware.steps[{idx}].audio_pack_id` must be a string")
            if not isinstance(step["actions"], list):
                raise ValidationError(f"`firmware.steps[{idx}].actions` must be a list")
            if not isinstance(step["apps"], list):
                raise ValidationError(f"`firmware.steps[{idx}].apps` must be a list")
            if not isinstance(step["transitions"], list):
                raise ValidationError(f"`firmware.steps[{idx}].transitions` must be a list")
            firmware_step_ids.append(step_id)

        initial_step = firmware.get("initial_step")
        if not isinstance(initial_step, str) or not initial_step:
            raise ValidationError("`firmware.initial_step` must be a non-empty string when `firmware.steps` is defined")
        if initial_step not in firmware_step_ids:
            raise ValidationError("`firmware.initial_step` must exist in `firmware.steps.step_id`")
        if firmware_step_ids != narrative_step_ids:
            raise ValidationError("`firmware.steps.step_id` order must match `steps_narrative.step_id` order")

        allowed_step_ids = set(firmware_step_ids)
        for idx, step in enumerate(raw_steps, start=1):
            for transition_idx, transition in enumerate(step["transitions"], start=1):
                if not isinstance(transition, dict):
                    raise ValidationError(
                        f"`firmware.steps[{idx}].transitions[{transition_idx}]` must be a mapping"
                    )
                for field in ("event_type", "event_name", "target_step_id", "priority", "after_ms"):
                    if field not in transition:
                        raise ValidationError(
                            f"`firmware.steps[{idx}].transitions[{transition_idx}]` is missing `{field}`"
                        )
                event_type = transition["event_type"]
                if event_type not in EVENT_TYPES:
                    raise ValidationError(
                        f"`firmware.steps[{idx}].transitions[{transition_idx}].event_type` is unsupported: {event_type}"
                    )
                if transition["target_step_id"] not in allowed_step_ids:
                    raise ValidationError(
                        f"`firmware.steps[{idx}].transitions[{transition_idx}].target_step_id` references an unknown step"
                    )
                if not isinstance(transition["event_name"], str) or not transition["event_name"]:
                    raise ValidationError(
                        f"`firmware.steps[{idx}].transitions[{transition_idx}].event_name` must be a non-empty string"
                    )
                if not isinstance(transition["priority"], int):
                    raise ValidationError(
                        f"`firmware.steps[{idx}].transitions[{transition_idx}].priority` must be an integer"
                    )
                if not isinstance(transition["after_ms"], int) or transition["after_ms"] < 0:
                    raise ValidationError(
                        f"`firmware.steps[{idx}].transitions[{transition_idx}].after_ms` must be a non-negative integer"
                    )

    if isinstance(firmware, dict) and isinstance(firmware.get("steps_reference_order"), list):
        ref_steps = firmware["steps_reference_order"]
        if narrative_step_ids != ref_steps:
            raise ValidationError(
                "`firmware.steps_reference_order` must match `steps_narrative.step_id` order"
            )
        if firmware_step_ids and ref_steps != firmware_step_ids:
            raise ValidationError(
                "`firmware.steps_reference_order` must match `firmware.steps.step_id` order"
            )
    elif runtime_steps_from_acts:
        # Preserve deterministic ordering checks even without firmware.steps_reference_order.
        if narrative_step_ids != runtime_steps_from_acts:
            raise ValidationError(
                "`steps_narrative.step_id` order must match concatenated `acts.runtime_steps`"
            )

    if isinstance(data.get("qr"), dict):
        payload = data["qr"].get("payload")
        if not isinstance(payload, str) or not payload.strip():
            raise ValidationError("`qr.payload` must be a non-empty string when qr is defined")

    players = data["players"]
    return players["min"], players["max"], duration["total_minutes"]


def detect_schema(data: dict) -> str:
    if "duration" in data or "acts" in data:
        return "v2"
    if "duration_minutes" in data or "solution" in data:
        return "v1"
    return "unknown"


def validate_scenario(path: Path, allow_legacy: bool) -> None:
    data = load_yaml(path)
    schema = detect_schema(data)

    if schema == "v2":
        min_players, max_players, total_minutes = validate_v2(data)
        print(
            f"[scenario-validate] ok {path.name} "
            f"(schema v2, players {min_players}-{max_players}, total {total_minutes} min)"
        )
        return

    if schema == "v1":
        if not allow_legacy:
            raise ValidationError("legacy V1 scenario detected; migrate to V2 or pass --allow-legacy")
        min_players, max_players = validate_v1(data)
        print(
            f"[scenario-validate] ok {path.name} "
            f"(schema v1 legacy, players {min_players}-{max_players})"
        )
        return

    raise ValidationError("unable to detect scenario schema (expected V2)")


def main() -> int:
    parser = argparse.ArgumentParser(description="Validate Zacus scenario YAML files (V2 by default).")
    parser.add_argument("paths", nargs="+", help="Path(s) to scenario YAML")
    parser.add_argument(
        "--allow-legacy",
        action="store_true",
        help="Allow legacy V1 scenarios during migration.",
    )
    args = parser.parse_args()

    errors = False
    for raw_path in args.paths:
        path = Path(raw_path)
        try:
            validate_scenario(path, allow_legacy=args.allow_legacy)
        except ValidationError as exc:
            errors = True
            print(f"[scenario-validate] error {path.name} -> {exc}")
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
