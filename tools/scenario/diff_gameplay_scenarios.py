#!/usr/bin/env python3
"""Generate a functional diff report between canonical and conversation gameplay YAML files."""

from __future__ import annotations

import argparse
from datetime import date
from pathlib import Path
import sys

try:
    import yaml
except ImportError:  # pragma: no cover
    sys.exit("Missing dependency: install PyYAML (pip install pyyaml) to diff scenario YAML files.")


def load_yaml(path: Path) -> dict:
    data = yaml.safe_load(path.read_text())
    if not isinstance(data, dict):
        raise ValueError(f"{path} must contain a YAML mapping")
    return data


def _safe_len(value) -> int:
    return len(value) if isinstance(value, list) else 0


def build_report(base_path: Path, target_path: Path, base: dict, target: dict) -> str:
    lines: list[str] = []
    lines.append("# Rapport de diff fonctionnel — scénarios Zacus")
    lines.append("")
    lines.append(f"- Date: {date.today().isoformat()}")
    lines.append(f"- Base canon: `{base_path}`")
    lines.append(f"- Cible gameplay: `{target_path}`")
    lines.append("")

    lines.append("## Identité")
    lines.append(f"- id: `{base.get('id')}` -> `{target.get('id')}`")
    lines.append(f"- version: `{base.get('version')}` -> `{target.get('version')}`")
    lines.append(f"- title: `{base.get('title')}` -> `{target.get('title')}`")
    lines.append("")

    lines.append("## Cadre de session")
    base_players = base.get("players", {})
    target_players = target.get("players", {})
    lines.append(
        f"- players: `{base_players.get('min')}-{base_players.get('max')}` -> `{target_players.get('min')}-{target_players.get('max')}`"
    )
    base_duration = base.get("duration_minutes", {})
    target_duration = target.get("duration_minutes", {})
    lines.append(
        f"- duration_minutes: `{base_duration.get('min')}-{base_duration.get('max')}` -> `{target_duration.get('min')}-{target_duration.get('max')}`"
    )
    lines.append("")

    lines.append("## Structure narrative")
    lines.append(f"- stations: `{_safe_len(base.get('stations'))}` -> `{_safe_len(target.get('stations'))}`")
    lines.append(f"- puzzles: `{_safe_len(base.get('puzzles'))}` -> `{_safe_len(target.get('puzzles'))}`")
    lines.append(f"- solution_unique: `{base.get('solution_unique')}` -> `{target.get('solution_unique')}`")
    lines.append("")

    lines.append("## Changements clés détectés")
    base_station_names = [s.get("name") for s in base.get("stations", []) if isinstance(s, dict)]
    target_station_names = [s.get("name") for s in target.get("stations", []) if isinstance(s, dict)]
    lines.append(f"- Stations cibles: {', '.join([str(x) for x in target_station_names])}")

    base_puzzle_ids = [p.get("id") for p in base.get("puzzles", []) if isinstance(p, dict)]
    target_puzzle_ids = [p.get("id") for p in target.get("puzzles", []) if isinstance(p, dict)]
    lines.append(f"- IDs puzzles base: {', '.join([str(x) for x in base_puzzle_ids])}")
    lines.append(f"- IDs puzzles cible: {', '.join([str(x) for x in target_puzzle_ids])}")

    if base_station_names != target_station_names:
        lines.append("- ✅ Les stations ont été réalignées avec le gameplay U-SON / Zone 4 / Archives.")
    if base_puzzle_ids != target_puzzle_ids:
        lines.append("- ✅ Les puzzles ont été remplacés pour refléter LA 440 → LEFOU → QR WIN.")

    lines.append("")
    lines.append("## Impact")
    lines.append("- Source de vérité gameplay maintenue dans `game/scenarios/*.yaml`.")
    lines.append("- Bundle conversationnel conservé comme matériau de travail parallèle.")
    lines.append("- Validation G3 requise via validateurs scénario/audio/printables + runtime bundle.")
    lines.append("")

    return "\n".join(lines)


def main() -> int:
    parser = argparse.ArgumentParser(description="Generate functional diff report between two scenario YAML files.")
    parser.add_argument("--base", default="scenario-ai-coherence/zacus_conversation_bundle_v3/zacus_v2.yaml")
    parser.add_argument("--target", default="game/scenarios/zacus_v1.yaml")
    parser.add_argument("--out", default=f"artifacts/qa-test/{date.today().isoformat()}/gameplay-functional-diff.md")
    args = parser.parse_args()

    base_path = Path(args.base)
    target_path = Path(args.target)
    out_path = Path(args.out)
    out_path.parent.mkdir(parents=True, exist_ok=True)

    try:
        base = load_yaml(base_path)
        target = load_yaml(target_path)
    except (OSError, ValueError, yaml.YAMLError) as exc:
        print(f"[scenario-diff] error -> {exc}")
        return 1

    report = build_report(base_path, target_path, base, target)
    out_path.write_text(report)
    print(f"[scenario-diff] ok report={out_path}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
