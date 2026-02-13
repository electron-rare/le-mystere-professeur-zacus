#!/usr/bin/env python3
"""Validate audio manifest YAML files for Le Mystère du Professeur Zacus."""

import argparse
import sys
from pathlib import Path

try:
    import yaml
except ImportError:  # pragma: no cover
    sys.exit("Missing dependency: install PyYAML (pip install pyyaml) to validate audio manifests.")

REQUIRED_KEYS = ["manifest_id", "version", "scenario_id", "tracks"]


class ValidationError(Exception):
    pass


def load_yaml(path: Path) -> dict:
    try:
        return yaml.safe_load(path.read_text())
    except yaml.YAMLError as exc:
        raise ValidationError(f"Invalid YAML in {path}: {exc}")


def validate_manifest(path: Path) -> None:
    data = load_yaml(path)
    if not isinstance(data, dict):
        raise ValidationError("Manifest file must be a mapping")

    missing = [key for key in REQUIRED_KEYS if key not in data]
    if missing:
        raise ValidationError(f"Missing required keys: {', '.join(missing)}")

    tracks = data["tracks"]
    if not (isinstance(tracks, list) and tracks):
        raise ValidationError("`tracks` must be a non-empty list")

    for idx, track in enumerate(tracks, start=1):
        for field in ("id", "title", "source", "cues"):
            if field not in track:
                raise ValidationError(f"Track #{idx} is missing `{field}`")
            if not track[field]:
                raise ValidationError(f"Track #{idx} field `{field}` must be non-empty")
        source_path = Path(track["source"])
        if not source_path.exists():
            raise ValidationError(f"Track #{idx} source file not found: {source_path}")

    print(f"[audio-validate] ok {path.name} (tracks {len(tracks)})")


def main() -> int:
    parser = argparse.ArgumentParser(description="Validate an audio manifest YAML file.")
    parser.add_argument("paths", nargs="+", help="Manifest file paths")
    args = parser.parse_args()

    errors = False
    for raw_path in args.paths:
        path = Path(raw_path)
        try:
            validate_manifest(path)
        except ValidationError as exc:
            errors = True
            print(f"[audio-validate] error {path.name} -> {exc}")
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
