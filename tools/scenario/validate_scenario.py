#!/usr/bin/env python3
"""Validate scenario YAML files for Le Mystère du Professeur Zacus."""

import argparse
import sys
from pathlib import Path

try:
    import yaml
except ImportError:  # pragma: no cover
    sys.exit("Missing dependency: install PyYAML (pip install pyyaml) to validate scenarios.")

REQUIRED_SCENE_KEYS = ["id", "title", "version", "players", "duration_minutes", "canon", "stations", "puzzles", "solution", "solution_unique"]


class ValidationError(Exception):
    pass


def load_yaml(path: Path) -> dict:
    try:
        return yaml.safe_load(path.read_text())
    except yaml.YAMLError as exc:
        raise ValidationError(f"Invalid YAML in {path}: {exc}")


def validate_scenario(path: Path) -> None:
    data = load_yaml(path)
    if not isinstance(data, dict):
        raise ValidationError(f"Scenario {path} is not a YAML mapping")

    missing = [key for key in REQUIRED_SCENE_KEYS if key not in data]
    if missing:
        raise ValidationError(f"Missing required keys: {', '.join(missing)}")

    players = data["players"]
    if not (isinstance(players, dict) and "min" in players and "max" in players):
        raise ValidationError("`players` must be a mapping with `min` and `max`")
    if not (isinstance(players["min"], int) and isinstance(players["max"], int)):
        raise ValidationError("`players.min` and `players.max` must be integers")
    if players["min"] > players["max"]:
        raise ValidationError("`players.min` must be <= `players.max`")
    if players["min"] < 6:
        raise ValidationError("`players.min` should be at least 6")

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
        if not all(field in station for field in ("name", "focus", "clue")):
            raise ValidationError(f"Station #{idx} is missing one of name/focus/clue")

    puzzles = data["puzzles"]
    if not (isinstance(puzzles, list) and puzzles):
        raise ValidationError("`puzzles` must be a non-empty list")
    for idx, puzzle in enumerate(puzzles, start=1):
        if not all(field in puzzle for field in ("id", "type", "clue", "effect")):
            raise ValidationError(f"Puzzle #{idx} is missing id/type/clue/effect")

    solution = data["solution"]
    if not all(field in solution for field in ("culprit", "motive", "method", "proof")):
        raise ValidationError("`solution` must include culprit, motive, method, proof")
    proof = solution["proof"]
    if not (isinstance(proof, list) and len(proof) >= 3):
        raise ValidationError("`solution.proof` must be a list of at least 3 strings")
    if data.get("solution_unique") is not True:
        raise ValidationError("`solution_unique` must be true for canonical scenarios")

    canon = data["canon"]
    if not (isinstance(canon, dict) and "timeline" in canon):
        raise ValidationError("`canon` must contain a timeline")
    timeline = canon["timeline"]
    if not (isinstance(timeline, list) and timeline):
        raise ValidationError("`canon.timeline` must be a non-empty list")
    for idx, entry in enumerate(timeline, start=1):
        if not ("label" in entry and "note" in entry):
            raise ValidationError(f"canon.timeline entry #{idx} must include label and note")

    print(f"[scenario-validate] ok {path.name} (players {players['min']}-{players['max']}, duration {duration['min']}-{duration['max']})")


def main() -> int:
    parser = argparse.ArgumentParser(description="Validate a Zacus scenario YAML file.")
    parser.add_argument("paths", nargs="+", help="Path(s) to scenario YAML")
    args = parser.parse_args()

    errors = False
    for raw_path in args.paths:
        path = Path(raw_path)
        try:
            validate_scenario(path)
        except ValidationError as exc:
            errors = True
            print(f"[scenario-validate] error {path.name} -> {exc}")
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
