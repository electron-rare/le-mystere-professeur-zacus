#!/usr/bin/env python3
"""Check ESP-NOW contract drift between Zacus and RTC_BL_PHONE docs.

Usage:
  python3 tools/dev/check_cross_repo_espnow_contract.py --rtc-repo <path-vers-RTC_BL_PHONE>

Exit code:
  0 => no drift
  1 => drift found
  2 => argument/input error
"""

from __future__ import annotations

import argparse
import os
import re
import sys
import json
from pathlib import Path
from typing import Dict, Iterable, Tuple


KNOWN_COMMANDS = {
    "ESPNOW_SEND",
    "STATUS",
    "WIFI_STATUS",
    "ESPNOW_STATUS",
    "UNLOCK",
    "NEXT",
    "WIFI_DISCONNECT",
    "WIFI_RECONNECT",
    "ESPNOW_ON",
    "ESPNOW_OFF",
    "STORY_REFRESH_SD",
    "SC_EVENT",
    "RING",
    "SCENE",
    "SCENE_GOTO",
}

KNOWN_ACTION_WILDCARDS = {"HW_*", "AUDIO_*", "MEDIA_*"}

# Commands that are intentionally Zacus-side extensions and are not required
# to exist in RTC docs to keep cross-repo compatibility green.
OPTIONAL_ZACUS_ONLY_COMMANDS = {"SCENE_GOTO"}

ZACUS_FILES = [
    Path("docs/ESP_NOW_API_CONTRACT_FREENOVE_V1.md"),
    Path("docs/ESP_NOW_API_CONTRACT_FREENOVE_V1_QUICK.md"),
    Path("docs/ESP_NOW_API_CONTRACT_FREENOVE_V1_MINI_SCHEMA.md"),
]

RTC_FILES = [
    Path("docs/espnow_api_v1.md"),
    Path("docs/espnow_contract.md"),
]


def _split_scene_token(token: str) -> str:
    token = token.strip("`*")
    if not token:
        return ""
    if token.startswith("SCENE "):
        return "SCENE"
    return token


def _extract_commands_from_text(text: str) -> Tuple[set[str], set[str]]:
    commands: set[str] = set()
    wildcards: set[str] = set()

    for raw_line in text.splitlines():
        line = raw_line.strip()
        if not line or line.startswith("```"):
            continue

        # Inline markdown code blocks and generic quoted terms.
        for seg in re.findall(r"`([^`]+)`", line):
            token = seg.strip()
            if not token:
                continue
            # Ignore common non-command metadata fragments.
            if token.upper() in {
                "JSON",
                "CMD",
                "COMMAND",
                "ACTION",
                "RAW",
                "TYPE",
                "ACK",
                "OK",
                "N/A",
            }:
                continue
            token = _split_scene_token(token)
            token = re.split(r"\s+", token)[0]
            if token in KNOWN_COMMANDS:
                commands.add(token)

        # Commands often are plain uppercase tokens in bullet rows.
        for token in re.findall(r"\b[A-Z][A-Z0-9_]{1,}\b", line):
            if token == "SCENE" or token in KNOWN_COMMANDS:
                commands.add(token)

        # Support action wildcards explicitly documented by both repos.
        for wildcard in KNOWN_ACTION_WILDCARDS:
            if wildcard in line:
                wildcards.add(wildcard)

    commands.discard("SCENE_WIN")
    commands.discard("SCENE_LA_DETECTOR")
    return commands, wildcards


def parse_docs(base: Path, files: Iterable[Path]) -> Tuple[set[str], set[str], dict[str, Tuple[set[str], set[str]]]]:
    missing: list[Path] = []
    all_cmds: set[str] = set()
    all_wc: set[str] = set()
    by_file: dict[str, Tuple[set[str], set[str]]] = {}

    for rel in files:
        path = base / rel
        if not path.exists():
            missing.append(rel)
            continue
        text = path.read_text(encoding="utf-8")
        cmds, wcs = _extract_commands_from_text(text)
        by_file[str(rel)] = (cmds, wcs)
        all_cmds |= cmds
        all_wc |= wcs
    if missing:
        raise FileNotFoundError(
            "Docs manquantes: " + ", ".join(str(m) for m in missing)
        )
    return all_cmds, all_wc, by_file


def _format_items(label: str, items: Iterable[str]) -> str:
    values = sorted(items)
    if not values:
        return f"{label}: <none>"
    return f"{label}: " + ", ".join(values)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--rtc-repo",
        default=os.environ.get("RTC_BL_PHONE_REPO", os.environ.get("RTC_REPO", "")),
        help="Chemin local du repo RTC_BL_PHONE (lecture seule).",
    )
    parser.add_argument(
        "--json",
        action="store_true",
        help="Afficher un résume structuré JSON-like.",
    )
    args = parser.parse_args()

    if not args.rtc_repo:
        print(
            "[ERROR] --rtc-repo manquant (ou variable RTC_BL_PHONE_REPO / RTC_REPO non definie)",
            file=sys.stderr,
        )
        return 2

    zacus_root = Path(__file__).resolve().parents[2]
    rtc_root = Path(args.rtc_repo).expanduser().resolve()

    try:
        zacus_cmds, zacus_wc, zacus_file_map = parse_docs(zacus_root, ZACUS_FILES)
        rtc_cmds, rtc_wc, rtc_file_map = parse_docs(rtc_root, RTC_FILES)
    except FileNotFoundError as exc:
        print(f"[ERROR] {exc}", file=sys.stderr)
        return 2
    except Exception as exc:  # pragma: no cover - fail-safe
        print(f"[ERROR] {exc}", file=sys.stderr)
        return 2

    missing_in_rtc = (zacus_cmds - rtc_cmds) - OPTIONAL_ZACUS_ONLY_COMMANDS
    missing_in_zacus = rtc_cmds - zacus_cmds
    optional_zacus_only = (zacus_cmds - rtc_cmds) & OPTIONAL_ZACUS_ONLY_COMMANDS
    missing_wc_in_rtc = zacus_wc - rtc_wc
    missing_wc_in_zacus = rtc_wc - zacus_wc

    if args.json:
        print(
            json.dumps(
                {
                    "zacus_cmds": sorted(zacus_cmds),
                    "rtc_cmds": sorted(rtc_cmds),
                    "drift_zacus_to_rtc": sorted(missing_in_rtc),
                    "drift_rtc_to_zacus": sorted(missing_in_zacus),
                    "optional_zacus_only": sorted(optional_zacus_only),
                    "wildcard_zacus": sorted(zacus_wc),
                    "wildcard_rtc": sorted(rtc_wc),
                    "wildcard_drift_zacus_to_rtc": sorted(missing_wc_in_rtc),
                    "wildcard_drift_rtc_to_zacus": sorted(missing_wc_in_zacus),
                    "files": {
                        "zacus": {
                            name: {
                                "commands": sorted(cmds),
                                "wildcards": sorted(wilds),
                            }
                            for name, (cmds, wilds) in zacus_file_map.items()
                        },
                        "rtc": {
                            name: {
                                "commands": sorted(cmds),
                                "wildcards": sorted(wilds),
                            }
                            for name, (cmds, wilds) in rtc_file_map.items()
                        },
                    },
                },
                indent=2,
                ensure_ascii=False,
                sort_keys=True,
            )
        )
    else:
        print("[REPORT] ESP-NOW contract cross-repo")
        print(f"Zacus repo root: {zacus_root}")
        print(f"RTC repo root  : {rtc_root}")
        print(_format_items("Commands Zacus", zacus_cmds))
        print(_format_items("Commands RTC", rtc_cmds))
        print(_format_items("Wildcard Zacus", zacus_wc))
        print(_format_items("Wildcard RTC", rtc_wc))
        print(_format_items("MISSING in RTC", missing_in_rtc))
        print(_format_items("MISSING in Zacus", missing_in_zacus))
        print(_format_items("OPTIONAL Zacus-only", optional_zacus_only))
        print(_format_items("Wildcard missing in RTC", missing_wc_in_rtc))
        print(_format_items("Wildcard missing in Zacus", missing_wc_in_zacus))

        if by_file := {k: [sorted(v[0]), sorted(v[1])] for k, v in (zacus_file_map | rtc_file_map).items()}:
            print("[TRACE] Parse summary (commands):")
            for name, (cmds, wilds) in by_file.items():
                print(f" - {name}: {sorted(cmds) if cmds else ['<none>']}")

    if missing_in_rtc or missing_in_zacus or missing_wc_in_rtc or missing_wc_in_zacus:
        print("[DRIFT] Divergence détectée", file=sys.stderr)
        return 1

    print("[PASS] Contrats alignés sur les commandes détectées")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
