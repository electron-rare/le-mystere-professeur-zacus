#!/usr/bin/env python3
"""Basic validator for audio manifest entries."""
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
    parser = argparse.ArgumentParser(description="Validate Zacus audio manifest")
    parser.add_argument("manifest", type=Path)
    args = parser.parse_args()

    if not args.manifest.exists():
        print(f"Manifest missing: {args.manifest}")
        return 1

    manifest = yaml.safe_load(args.manifest.read_text())
    if not isinstance(manifest, dict):
        print("Manifest must be a mapping")
        return 1

    packs = manifest.get("packs")
    if not isinstance(packs, list) or not packs:
        print("Manifest packs must be a non-empty list")
        return 1

    invalid = False
    seen: set[str] = set()
    for pack in packs:
        if not isinstance(pack, dict):
            print("Each pack entry must be a mapping")
            invalid = True
            continue
        pack_id = pack.get("id")
        if not pack_id or not isinstance(pack_id, str):
            print("Pack missing id")
            invalid = True
        elif pack_id in seen:
            print(f"Duplicate pack id {pack_id}")
            invalid = True
        else:
            seen.add(pack_id)

        files = pack.get("files")
        if not isinstance(files, list) or not files:
            print(f"pack {pack_id or 'unknown'} files must be a non-empty list")
            invalid = True
        else:
            for item in files:
                if not isinstance(item, str) or not item.strip():
                    print(f"pack {pack_id or 'unknown'} has invalid file entry")
                    invalid = True

        duration = pack.get("duration_ms")
        if duration is not None and (not isinstance(duration, int) or duration <= 0):
            print(f"pack {pack_id or 'unknown'} duration must be a positive integer")
            invalid = True

    if invalid:
        return 1
    print(f"[audio-validate] {args.manifest} valid ({len(seen)} packs)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
