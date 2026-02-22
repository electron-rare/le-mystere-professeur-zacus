#!/usr/bin/env python3
"""Validate printable manifest YAML files against expected prompts."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path
from typing import Optional

try:
    import yaml
except ImportError:  # pragma: no cover
    print("Missing dependency: install PyYAML (pip install pyyaml) to validate printables manifests.")
    sys.exit(1)

REPO_ROOT = Path(__file__).resolve().parents[2]
DEFAULT_MANIFEST = REPO_ROOT / "printables/manifests/zacus_v1_printables.yaml"
REQUIRED_MANIFEST_KEYS = ["manifest_id", "version", "scenario_id", "items"]


def load_manifest(path: Path) -> dict:
    try:
        return yaml.safe_load(path.read_text())
    except OSError as exc:
        raise ValueError(f"Cannot read manifest {path}: {exc}")
    except yaml.YAMLError as exc:
        raise ValueError(f"Invalid YAML in {path}: {exc}")


def resolve_manifest_path(raw_path: str) -> Path:
    candidate = Path(raw_path).expanduser()
    if candidate.is_absolute():
        return candidate

    cwd_path = Path.cwd() / candidate
    if cwd_path.exists():
        return cwd_path

    root_path = REPO_ROOT / candidate
    if root_path.exists():
        return root_path

    return cwd_path


def resolve_prompt_path(prompt: str, manifest_path: Path) -> Optional[Path]:
    prompt_path = Path(prompt)
    candidates: list[Path] = []
    if prompt_path.is_absolute():
        candidates.append(prompt_path)
    else:
        # Typical layout: printables/manifests/<manifest>.yaml -> printables/
        candidates.extend(
            [
                manifest_path.parent.parent / prompt_path,
                REPO_ROOT / "printables" / prompt_path,
                REPO_ROOT / prompt_path,
            ]
        )

    for path in candidates:
        if path.exists():
            return path
    return None


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
        prompt_value = str(entry["prompt"])
        prompt_path = resolve_prompt_path(prompt_value, path)
        if prompt_path is None:
            errors.append(f"missing prompt file for {entry['id']}: {prompt_value}")
    return errors


def main() -> int:
    parser = argparse.ArgumentParser(description="Validate printables manifest YAML files.")
    parser.add_argument("manifest", nargs="?", default=str(DEFAULT_MANIFEST))
    args = parser.parse_args()

    manifest_path = resolve_manifest_path(args.manifest)
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
