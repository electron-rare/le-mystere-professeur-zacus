#!/usr/bin/env python3
"""Validate printable manifest YAML files against expected prompts."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

try:
    import yaml
except ImportError:  # pragma: no cover
    print("Missing dependency: install PyYAML (pip install pyyaml) to validate printables manifests.")
    sys.exit(1)

BASE_DIR = Path("printables")
REQUIRED_MANIFEST_KEYS = ["manifest_id", "version", "scenario_id", "items"]


def load_manifest(path: Path) -> dict:
    try:
        return yaml.safe_load(path.read_text())
    except yaml.YAMLError as exc:
        raise ValueError(f"Invalid YAML in {path}: {exc}")


def validate_manifest(path: Path) -> list[str]:
    manifest = load_manifest(path)
    missing = [key for key in REQUIRED_MANIFEST_KEYS if key not in manifest]
    if missing:
        raise ValueError(f"Manifest missing keys: {', '.join(missing)}")

    items = manifest.get("items")
    if not isinstance(items, list) or not items:
        raise ValueError("`items` must be a non-empty list")

    errors: list[str] = []
    for entry in items:
        if not isinstance(entry, dict):
            errors.append("manifest item must be a mapping")
            continue
        if "id" not in entry:
            errors.append("manifest item missing `id`")
            continue
        if "prompt" not in entry:
            errors.append(f"item {entry.get('id', '<unknown>')} missing `prompt`")
            continue
        prompt_path = BASE_DIR / entry["prompt"]
        if not prompt_path.exists():
            errors.append(f"missing prompt file for {entry['id']}: {prompt_path}")
    return errors


def main() -> int:
    parser = argparse.ArgumentParser(description="Validate printables manifest YAML files.")
    parser.add_argument("manifest", nargs="?", default="printables/manifests/zacus_v1_printables.yaml")
    args = parser.parse_args()

    manifest_path = Path(args.manifest)
    if not manifest_path.exists():
        print(f"Manifest file not found: {manifest_path}")
        return 1

    try:
        errors = validate_manifest(manifest_path)
    except ValueError as exc:
        print(f"[printables-validate] error: {exc}")
        return 1

    if errors:
        for error in errors:
            print(f"[printables-validate] missing: {error}")
        return 1

    print(f"[printables-validate] OK {manifest_path.name} items={len(load_manifest(manifest_path)['items'])}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
