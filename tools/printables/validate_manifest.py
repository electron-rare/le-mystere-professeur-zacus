#!/usr/bin/env python3
"""Validator for printable manifest files."""
from __future__ import annotations

import argparse
import sys
from pathlib import Path

try:
    import yaml
except ImportError as exc:
    print("Missing dependency: install PyYAML", exc)
    sys.exit(1)


def main() -> int:
    parser = argparse.ArgumentParser(description="Validate printable manifest")
    parser.add_argument("manifest", type=Path)
    args = parser.parse_args()

    if not args.manifest.exists():
        print(f"Manifest missing: {args.manifest}")
        return 1

    manifest = yaml.safe_load(args.manifest.read_text())
    if not isinstance(manifest, dict):
        print("Manifest must be a mapping")
        return 1

    items = manifest.get("items")
    if not isinstance(items, list) or not items:
        print("Manifest items must be a non-empty list")
        return 1

    invalid = False
    seen: set[str] = set()
    for item in items:
        if not isinstance(item, dict):
            print("Each printable entry must be a mapping")
            invalid = True
            continue
        item_id = item.get("id")
        if not item_id or not isinstance(item_id, str):
            print("Printable entry missing id")
            invalid = True
        elif item_id in seen:
            print(f"Duplicate printable id {item_id}")
            invalid = True
        else:
            seen.add(item_id)

        if not item.get("type"):
            print(f"Printable {item_id or 'unknown'} missing type")
            invalid = True
        prompt = item.get("prompt")
        if not isinstance(prompt, str) or not prompt.strip():
            print(f"Printable {item_id or 'unknown'} missing prompt path")
            invalid = True
        version = item.get("version")
        if not isinstance(version, int) or version <= 0:
            print(f"Printable {item_id or 'unknown'} version must be positive")
            invalid = True

    if invalid:
        return 1
    print(f"[printables-validate] {args.manifest} OK ({len(seen)} items)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
