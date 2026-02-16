#!/usr/bin/env python3
"""Load cockpit command registry from YAML."""

from __future__ import annotations

from pathlib import Path
from typing import Any, Dict, List, Optional


def _parse_scalar(value: str) -> str:
    raw = value.strip()
    if raw in ("", "null", "None"):
        return ""
    if raw.startswith("[") and raw.endswith("]"):
        items = raw[1:-1].strip()
        if not items:
            return ""
        return items
    if (raw.startswith("\"") and raw.endswith("\"")) or (
        raw.startswith("'") and raw.endswith("'")
    ):
        return raw[1:-1]
    return raw


def _parse_inline_list(value: str) -> List[str]:
    raw = value.strip()
    if raw == "[]":
        return []
    if raw.startswith("[") and raw.endswith("]"):
        body = raw[1:-1].strip()
        if not body:
            return []
        parts = [p.strip() for p in body.split(",")]
        return [_parse_scalar(p) for p in parts if p]
    return [_parse_scalar(raw)]


def _parse_simple_yaml(text: str) -> Dict[str, Any]:
    data: Dict[str, Any] = {"commands": []}
    current: Optional[Dict[str, Any]] = None
    list_key: Optional[str] = None

    for raw_line in text.splitlines():
        line = raw_line.rstrip()
        if not line.strip() or line.strip().startswith("#"):
            continue
        if line.startswith("commands:"):
            continue
        if line.lstrip().startswith("- "):
            if current is not None:
                data["commands"].append(current)
            current = {}
            list_key = None
            content = line.lstrip()[2:].strip()
            # Only parse key:value pairs; skip list items without colons
            if content and ":" in content:
                key, value = content.split(":", 1)
                current[key.strip()] = _parse_scalar(value)
            continue

        if current is None:
            continue

        stripped = line.strip()
        if stripped.endswith(":") and ":" not in stripped[:-1]:
            key = stripped[:-1].strip()
            current[key] = []
            list_key = key
            continue

        if list_key and stripped.startswith("- "):
            current[list_key].append(_parse_scalar(stripped[2:]))
            continue

        if ":" in stripped:
            key, value = stripped.split(":", 1)
            key = key.strip()
            value = value.strip()
            if value.startswith("["):
                current[key] = _parse_inline_list(value)
            else:
                current[key] = _parse_scalar(value)
            list_key = None
        # Skip lines that don't match expected patterns (e.g., multiline scalar content)

    if current is not None:
        data["commands"].append(current)

    return data


def load_registry(path: Path) -> Dict[str, Any]:
    text = path.read_text(encoding="utf-8")
    try:
        import yaml  # type: ignore

        return yaml.safe_load(text) or {"commands": []}
    except Exception:
        return _parse_simple_yaml(text)
