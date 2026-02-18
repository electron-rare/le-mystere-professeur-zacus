#!/usr/bin/env python3
"""Resolve firmware paths across legacy and migrated repository layouts."""

from __future__ import annotations

import argparse
import os
from pathlib import Path


def _script_root() -> Path:
    return Path(__file__).resolve().parents[2]


def fw_root() -> Path:
    explicit = os.environ.get("FW_ROOT")
    if explicit:
        path = Path(explicit).resolve()
        if path.exists():
            return path

    candidate = _script_root()
    if (candidate / "hardware/firmware/esp32_audio/src").exists():
        return candidate
    if (candidate / "esp32_audio/src").exists():
        return candidate
    return candidate


def _first_existing(*paths: Path) -> Path:
    for path in paths:
        if path.exists():
            return path
    return paths[0]


def fw_story_src() -> Path:
    root = fw_root()
    return _first_existing(
        root / "hardware/libs/story/src",
        root / "hardware/firmware/esp32_audio/src/story",
        root / "esp32_audio/src/story",
    )


def fw_ui_oled_src() -> Path:
    root = fw_root()
    return _first_existing(
        root / "hardware/firmware/ui/esp8266_oled/src",
        root / "ui/esp8266_oled/src",
    )


def fw_ui_tft_src() -> Path:
    root = fw_root()
    return _first_existing(
        root / "hardware/firmware/ui/rp2040_tft/src",
        root / "ui/rp2040_tft/src",
    )


def fw_story_specs_dir() -> Path:
    root = fw_root()
    return _first_existing(
        root / "docs/protocols/story_specs/scenarios",
        root / "story_generator/story_specs/scenarios",
    )


def fw_esp32_src_root() -> Path:
    root = fw_root()
    return _first_existing(
        root / "hardware/firmware/esp32_audio/src",
        root / "esp32_audio/src",
    )


def _resolve(kind: str) -> Path:
    mapping = {
        "fw_root": fw_root,
        "fw_story_src": fw_story_src,
        "fw_ui_oled_src": fw_ui_oled_src,
        "fw_ui_tft_src": fw_ui_tft_src,
        "fw_story_specs_dir": fw_story_specs_dir,
        "fw_esp32_src_root": fw_esp32_src_root,
    }
    return mapping[kind]()


def main() -> int:
    parser = argparse.ArgumentParser(description="Resolve firmware layout paths.")
    parser.add_argument(
        "kind",
        choices=[
            "fw_root",
            "fw_story_src",
            "fw_ui_oled_src",
            "fw_ui_tft_src",
            "fw_story_specs_dir",
            "fw_esp32_src_root",
        ],
    )
    args = parser.parse_args()
    print(_resolve(args.kind))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

