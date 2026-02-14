#!/usr/bin/env python3
"""Export Zacus scenarios into simple Markdown briefs for the kit and docs."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

try:
    import yaml
except ImportError:  # pragma: no cover
    print("Missing dependency: install PyYAML (pip install pyyaml) to export scenarios.")
    sys.exit(1)

KIT_DIR = Path("kit-maitre-du-jeu/_generated")
DOCS_DIR = Path("docs/_generated")


def normalize(value: str | None) -> str:
    if value is None:
        return ""
    return " ".join(str(value).strip().split())


def section(title: str, lines: list[str]) -> list[str]:
    output = [f"## {title}"] if title else []
    if title and lines:
        output.append("")
    output.extend(lines)
    output.append("")
    return output


def build_script_doc(scenario: dict) -> str:
    title = scenario.get("title") or scenario.get("id")
    canon = scenario.get("canon", {})
    timeline = canon.get("timeline", [])
    stations = scenario.get("stations", [])
    puzzles = scenario.get("puzzles", [])
    solution = scenario.get("solution", {})

    lines = [f"# Script — {title}", ""]
    lines += section("Introduction", [normalize(canon.get("introduction")) or "(à compléter)"])

    if timeline:
        lines += section("Chronologie canon", [f"- **{entry.get('label', 'Moment')}** — {normalize(entry.get('note'))}" for entry in timeline])
    else:
        lines += section("Chronologie canon", ["- À compléter dans le YAML."])

    if stations:
        lines += section("Stations", [f"- **{station.get('name', 'Station')}** — {normalize(station.get('focus'))}" for station in stations])
    else:
        lines += section("Stations", ["- Pas de stations définies."])

    if puzzles:
        lines += section("Puzzles", [f"- {puzzle.get('id')} ({puzzle.get('type')}) — {normalize(puzzle.get('clue'))}" for puzzle in puzzles])
    else:
        lines += section("Puzzles", ["- Aucun puzzle renseigné."])

    solution_lines = [f"- Coupable : {solution.get('culprit', 'à définir')}", f"- Mobile : {normalize(solution.get('motive'))}", f"- Méthode : {normalize(solution.get('method'))}"]
    proof = solution.get("proof") or []
    if proof:
        solution_lines.append("- Preuves :")
        solution_lines += [f"  - {normalize(item)}" for item in proof]
    else:
        solution_lines.append("- Preuves : à compléter (au moins 3).")

    lines += section("Solution canon", solution_lines)
    lines += section("Notes pratiques", [f"- Solution unique : {scenario.get('solution_unique')}"],)
    return "\n".join(lines).strip() + "\n"


def build_checklist_doc(scenario: dict, scenario_path: Path) -> str:
    players = scenario.get("players", {})
    duration = scenario.get("duration_minutes", {})
    stations = scenario.get("stations", [])
    puzzles = scenario.get("puzzles", [])
    solution = scenario.get("solution", {})
    proof = solution.get("proof") or []

    lines = ["# Checklist scénario", ""]
    lines += section("Configurer", [
        f"- Scénario : {scenario.get('title') or scenario.get('id')}",
        f"- Fichier source : {scenario_path}",
        f"- Participants : {players.get('min', '?')}–{players.get('max', '?')}",
        f"- Durée : {duration.get('min', '?')}–{duration.get('max', '?')} min",
        f"- Stations prévues : {len(stations)}",
        f"- Puzzles : {len(puzzles)}",
        f"- Preuves listées : {len(proof)}",
    ])
    lines += section("Matériel recommandé", [
        "- Enveloppes numérotées",
        "- Badges détective + fiches d’enquête",
        "- Audio : audio/manifests/zacus_v1_audio.yaml",
        "- Printables : suivre printables/manifests/zacus_v1_printables.yaml",
    ])
    lines += section("Vérifications", [
        "- python3 tools/scenario/validate_scenario.py game/scenarios/zacus_v1.yaml",
        "- python3 tools/printables/validate_manifest.py printables/manifests/zacus_v1_printables.yaml",
    ])
    return "\n".join(lines).strip() + "\n"


def build_anti_chaos_doc(scenario: dict) -> str:
    players = scenario.get("players", {})
    duration = scenario.get("duration_minutes", {})
    lines = ["# Guide anti-chaos généré", ""]
    lines += section("Règles", [
        f"- Max {players.get('max', '?')} enfants : rotation toutes les 15 min.",
        f"- Durée cible {duration.get('min', '?')}–{duration.get('max', '?')} min : deux pauses courtes.",
        "- Un seul enfant parle à la fois, signal sonore pour les transitions.",
        "- Indices dans leurs enveloppes jusqu’à validation du chef de piste.",
    ])
    lines += section("Interventions", [
        "- Propose un mini-défi express si ça déraille.",
        "- Donne un rôle d’observateur aux enfants en retrait.",
    ])
    return "\n".join(lines).strip() + "\n"


def build_roles_doc(scenario: dict) -> str:
    players = scenario.get("players", {})
    lines = ["# Répartition des rôles", ""]
    lines += section("Rôles principaux", [
        "- Chef de piste : centralise les indices et relit la timeline.",
        "- Chronométreur : marque les phases (exploration, synthèse, accusation).",
        "- Analyste technique : gère les codes et les puzzles logiques.",
        "- Archiviste : organise les preuves (empreintes, logs, mobile).",
        "- Témoin narrateur : redonne les consignes anti-chaos et raconte.",
        "- Gardien des objets : protège les enveloppes, badges et fiches.",
    ])
    lines += section("Rôles optionnels", [
        "- Détective des émotions : remet le calme si besoin.",
        "- Cartographe : trace le parcours des stations.",
        "- Ambassadeur : valide que rien n’a été oublié.",
        "- Journaliste : consigne la conclusion finale.",
    ])
    return "\n".join(lines).strip() + "\n"


def build_stations_doc(scenario: dict) -> str:
    stations = scenario.get("stations", [])
    puzzles = scenario.get("puzzles", [])
    lines = ["# Stations détaillées", ""]
    if stations:
        for station in stations:
            lines += [f"## {station.get('name', 'Station')}", "", f"- Focus : {normalize(station.get('focus'))}", f"- Clue : {normalize(station.get('clue'))}", ""]
    else:
        lines += ["- Aucune station définie dans le scénario."]
    if puzzles:
        lines += ["## Puzzles", ""]
        for puzzle in puzzles:
            lines += [f"- {puzzle.get('id')} ({puzzle.get('type')}) : {normalize(puzzle.get('clue'))}", f"  Effet : {normalize(puzzle.get('effect'))}", ""]
    return "\n".join(lines).strip() + "\n"


def build_brief_doc(scenario: dict) -> str:
    title = scenario.get("title") or scenario.get("id")
    players = scenario.get("players", {})
    duration = scenario.get("duration_minutes", {})
    canon = scenario.get("canon", {})
    timeline = canon.get("timeline", [])
    stations = scenario.get("stations", [])
    puzzles = scenario.get("puzzles", [])
    solution = scenario.get("solution", {})

    lines = [f"# SCENARIO BRIEF — {title}", ""]
    lines += section("Stats", [
        f"- Joueurs : {players.get('min', '?')}–{players.get('max', '?')}",
        f"- Durée : {duration.get('min', '?')}–{duration.get('max', '?')} min",
        f"- Solution unique : {scenario.get('solution_unique')}",
    ])
    lines += section("Introduction", [normalize(canon.get('introduction')) or "(à compléter)"])
    if timeline:
        lines += section("Chronologie", [f"- **{entry.get('label')}** — {normalize(entry.get('note'))}" for entry in timeline])
    else:
        lines += section("Chronologie", ["- À compléter."])
    if stations:
        lines += section("Stations", [f"- **{station.get('name')}** — {normalize(station.get('focus'))} | Indice : {normalize(station.get('clue'))}" for station in stations])
    else:
        lines += section("Stations", ["- À compléter."])
    if puzzles:
        lines += section("Puzzles", [f"- {puzzle.get('id')} ({puzzle.get('type')}) — {normalize(puzzle.get('clue'))}\n  Effet : {normalize(puzzle.get('effect'))}" for puzzle in puzzles])
    else:
        lines += section("Puzzles", ["- À compléter."])
    solution_lines = [f"- Coupable : {solution.get('culprit', 'à définir')}", f"- Mobile : {normalize(solution.get('motive'))}", f"- Méthode : {normalize(solution.get('method'))}"]
    proof = solution.get("proof") or []
    if proof:
        solution_lines += ["- Preuves:"] + [f"  - {normalize(item)}" for item in proof]
    lines += section("Solution", solution_lines)
    notes = scenario.get("notes")
    if notes:
        lines += section("Notes", [normalize(notes)])
    return "\n".join(lines).strip() + "\n"


def write_file(path: Path, content: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content)


def export_files(scenario_path: Path) -> None:
    if not scenario_path.exists():
        print(f"Scenario file not found: {scenario_path}")
        sys.exit(1)
    scenario = yaml.safe_load(scenario_path.read_text())
    files = {
        KIT_DIR / "script.md": build_script_doc(scenario),
        KIT_DIR / "checklist.md": build_checklist_doc(scenario, scenario_path),
        KIT_DIR / "anti-chaos.md": build_anti_chaos_doc(scenario),
        KIT_DIR / "roles.md": build_roles_doc(scenario),
        KIT_DIR / "stations.md": build_stations_doc(scenario),
        DOCS_DIR / "SCENARIO_BRIEF.md": build_brief_doc(scenario),
    }
    for path, content in files.items():
        write_file(path, content)
    print("[export] generated Markdown briefs for", scenario_path.name)


def main() -> int:
    parser = argparse.ArgumentParser(description="Export Zacus scenario briefs as Markdown.")
    parser.add_argument("scenario", type=Path, help="Path to the scenario YAML file")
    args = parser.parse_args()

    export_files(args.scenario)
    return 0


if __name__ == "__main__":
    sys.exit(main())
