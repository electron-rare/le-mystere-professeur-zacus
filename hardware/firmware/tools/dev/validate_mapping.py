#!/usr/bin/env python3
"""Cross-check Freenove mapping between PlatformIO, config header, and RC doc."""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path
from typing import Dict, List, Tuple


ROOT = Path(__file__).resolve().parents[2]
DEFAULT_PLATFORMIO = ROOT / "platformio.ini"
DEFAULT_RC = ROOT / "docs/RC_FINAL_BOARD.md"
DEFAULT_CONFIG_CANDIDATES = (
    ROOT / "hardware/firmware/ui_freenove_allinone/include/ui_freenove_config.h",
    ROOT / "ui_freenove_allinone/include/ui_freenove_config.h",
)

RE_ENV = re.compile(r"^\[env:([^\]]+)\]\s*$")
RE_DEFINE = re.compile(r"^\s*#define\s+([A-Z0-9_]+)\s+(.+?)\s*$")
RE_INT = re.compile(r"^-?\d+$")

FREENOVE_ENV = "freenove_esp32s3"
FREENOVE_FULL_ENV = "freenove_esp32s3_full_with_ui"
REQUIRED_MACROS = (
    "FREENOVE_LCD_WIDTH",
    "FREENOVE_LCD_HEIGHT",
    "FREENOVE_TFT_SCK",
    "FREENOVE_TFT_MOSI",
    "FREENOVE_TFT_MISO",
    "FREENOVE_TFT_CS",
    "FREENOVE_TFT_DC",
    "FREENOVE_TFT_RST",
    "FREENOVE_TFT_BL",
    "FREENOVE_BTN_ANALOG_PIN",
    "FREENOVE_I2S_WS",
    "FREENOVE_I2S_BCK",
    "FREENOVE_I2S_DOUT",
)
OPTIONAL_TOUCH_MACROS = ("FREENOVE_TOUCH_CS", "FREENOVE_TOUCH_IRQ")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--platformio", default=str(DEFAULT_PLATFORMIO), help="Path to platformio.ini")
    parser.add_argument("--config", default="", help="Path to ui_freenove_config.h")
    parser.add_argument("--rc", default=str(DEFAULT_RC), help="Path to docs/RC_FINAL_BOARD.md")
    parser.add_argument("--output", default="", help="Optional JSON output path")
    return parser.parse_args()


def resolve_config_path(raw: str) -> Path:
    if raw:
        return Path(raw).resolve()
    for candidate in DEFAULT_CONFIG_CANDIDATES:
        if candidate.exists():
            return candidate
    return DEFAULT_CONFIG_CANDIDATES[0]


def parse_platformio(path: Path) -> Tuple[Dict[str, Dict[str, str]], Dict[str, List[str]]]:
    sections: Dict[str, Dict[str, str]] = {}
    raw_lines: Dict[str, List[str]] = {}
    current = ""

    for line in path.read_text(encoding="utf-8").splitlines():
        m = RE_ENV.match(line.strip())
        if m:
            current = m.group(1).strip()
            sections.setdefault(current, {})
            raw_lines.setdefault(current, [])
            continue
        if not current:
            continue
        raw_lines[current].append(line)
        if "=" in line:
            key, val = line.split("=", 1)
            sections[current][key.strip()] = val.strip()
    return sections, raw_lines


def parse_macros(path: Path) -> Dict[str, str]:
    macros: Dict[str, str] = {}
    for raw in path.read_text(encoding="utf-8").splitlines():
        line = raw.split("//", 1)[0].rstrip()
        if not line:
            continue
        m = RE_DEFINE.match(line)
        if not m:
            continue
        name, value = m.group(1), m.group(2).strip()
        macros[name] = value
    return macros


def parse_rc_mapping(path: Path) -> Dict[str, str]:
    mapping: Dict[str, str] = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        if not line.startswith("|"):
            continue
        cells = [cell.strip() for cell in line.split("|")[1:-1]]
        if len(cells) < 3:
            continue
        macro = cells[2]
        pin = cells[1]
        if not macro.startswith("FREENOVE_"):
            continue
        mapping[macro] = pin
    return mapping


def normalize_int(value: str) -> int | None:
    token = value.strip()
    if token.startswith("(") and token.endswith(")"):
        token = token[1:-1].strip()
    if not RE_INT.match(token):
        return None
    return int(token)


def validate(
    platformio_path: Path, config_path: Path, rc_path: Path
) -> Tuple[List[str], List[str], Dict[str, object]]:
    errors: List[str] = []
    warnings: List[str] = []
    details: Dict[str, object] = {
        "paths": {
            "platformio": str(platformio_path),
            "config": str(config_path),
            "rc": str(rc_path),
        }
    }

    if not platformio_path.exists():
        errors.append(f"Missing file: {platformio_path}")
    if not config_path.exists():
        errors.append(f"Missing file: {config_path}")
    if not rc_path.exists():
        errors.append(f"Missing file: {rc_path}")
    if errors:
        return errors, warnings, details

    sections, raw_lines = parse_platformio(platformio_path)
    macros = parse_macros(config_path)
    rc_map = parse_rc_mapping(rc_path)
    details["platformio_envs"] = sorted(sections.keys())

    if FREENOVE_ENV not in sections:
        errors.append(f"Missing PlatformIO env [{FREENOVE_ENV}]")
    if FREENOVE_FULL_ENV not in sections:
        errors.append(f"Missing PlatformIO env [{FREENOVE_FULL_ENV}]")

    if FREENOVE_ENV in sections:
        extends_val = sections[FREENOVE_ENV].get("extends", "")
        if extends_val != f"env:{FREENOVE_FULL_ENV}":
            errors.append(
                f"[{FREENOVE_ENV}] extends should be env:{FREENOVE_FULL_ENV}, got: {extends_val or '<empty>'}"
            )

    if FREENOVE_FULL_ENV in raw_lines:
        full_env_text = "\n".join(raw_lines[FREENOVE_FULL_ENV])
        if "-include ui_freenove_config.h" not in full_env_text:
            errors.append(f"[{FREENOVE_FULL_ENV}] missing -include ui_freenove_config.h in build flags")
        if "ui_freenove_allinone/include" not in full_env_text:
            errors.append(f"[{FREENOVE_FULL_ENV}] missing ui_freenove_allinone include path")

    for macro in REQUIRED_MACROS:
        if macro not in macros:
            errors.append(f"Missing macro in config: {macro}")
            continue
        if macro not in rc_map:
            errors.append(f"Missing macro in RC table: {macro}")
            continue
        cfg_int = normalize_int(macros[macro])
        rc_int = normalize_int(rc_map[macro])
        if cfg_int is None or rc_int is None:
            if macros[macro] != rc_map[macro]:
                warnings.append(
                    f"Non-numeric compare for {macro}: config='{macros[macro]}' rc='{rc_map[macro]}'"
                )
        elif cfg_int != rc_int:
            errors.append(f"Pin mismatch {macro}: config={cfg_int} rc={rc_int}")

    has_touch = normalize_int(macros.get("FREENOVE_HAS_TOUCH", "0")) == 1
    if has_touch:
        for macro in OPTIONAL_TOUCH_MACROS:
            if macro not in macros:
                errors.append(f"Touch enabled but missing macro in config: {macro}")
            if macro not in rc_map:
                errors.append(f"Touch enabled but missing macro in RC table: {macro}")

    details["required_macros_checked"] = list(REQUIRED_MACROS)
    details["touch_enabled"] = has_touch
    details["rc_macros_found"] = sorted(k for k in rc_map if k.startswith("FREENOVE_"))
    return errors, warnings, details


def main() -> int:
    args = parse_args()
    platformio_path = Path(args.platformio).resolve()
    config_path = resolve_config_path(args.config)
    rc_path = Path(args.rc).resolve()

    errors, warnings, details = validate(platformio_path, config_path, rc_path)
    result = {
        "status": "PASS" if not errors else "FAIL",
        "errors": errors,
        "warnings": warnings,
        "details": details,
    }

    if args.output:
        output_path = Path(args.output).resolve()
        output_path.parent.mkdir(parents=True, exist_ok=True)
        output_path.write_text(json.dumps(result, indent=2, ensure_ascii=True) + "\n", encoding="utf-8")

    if errors:
        print("[FAIL] Freenove mapping mismatch detected:")
        for err in errors:
            print(f" - {err}")
    else:
        print("[PASS] Freenove mapping is consistent across platformio/config/RC.")

    for warning in warnings:
        print(f"[WARN] {warning}")

    return 1 if errors else 0


if __name__ == "__main__":
    raise SystemExit(main())
