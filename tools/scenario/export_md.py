#!/usr/bin/env python3
"""Export Zacus scenarios into Markdown briefs for kit/docs."""

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


def detect_schema(scenario: dict) -> str:
    if "duration" in scenario or "acts" in scenario:
        return "v2"
    if "duration_minutes" in scenario or "solution" in scenario:
        return "v1"
    return "unknown"


def to_export_model(scenario: dict) -> dict:
    schema = detect_schema(scenario)
    if schema == "v1":
        return scenario

    if schema != "v2":
        raise ValueError("Unsupported scenario schema: expected V2 or legacy V1")

    scenario_id = str(scenario.get("id", ""))
    title = scenario.get("title") or scenario_id
    players = scenario.get("players", {})
    duration = scenario.get("duration", {})
    total_minutes = duration.get("total_minutes")
    if not isinstance(total_minutes, int) or total_minutes <= 0:
        total_minutes = 90

    canon = scenario.get("canon", {})
    acts = scenario.get("acts", []) if isinstance(scenario.get("acts"), list) else []

    timeline = []
    for idx, act in enumerate(acts, start=1):
        if not isinstance(act, dict):
            continue
        label = act.get("title") or f"Acte {idx}"
        note = act.get("goal") or ""
        timeline.append({"label": label, "note": note})

    stations = []
    for station in scenario.get("stations", []):
        if not isinstance(station, dict):
            continue
        stations.append(
            {
                "name": station.get("name", "Station"),
                "focus": station.get("focus", ""),
                "clue": station.get("clue", ""),
            }
        )

    puzzles = []
    for puzzle in scenario.get("puzzles", []):
        if not isinstance(puzzle, dict):
            continue
        clue = puzzle.get("objective") or puzzle.get("clue") or puzzle.get("rule_real") or ""
        effect = (
            puzzle.get("success_condition")
            or puzzle.get("validation_mode")
            or puzzle.get("fail_safe")
            or puzzle.get("note")
            or ""
        )
        puzzles.append(
            {
                "id": puzzle.get("id", "puzzle"),
                "type": puzzle.get("type", ""),
                "clue": clue,
                "effect": effect,
            }
        )

    firmware_steps = []
    firmware = scenario.get("firmware")
    if isinstance(firmware, dict) and isinstance(firmware.get("steps_reference_order"), list):
        firmware_steps = [str(step) for step in firmware["steps_reference_order"] if step]

    proof = []
    for puzzle in puzzles[:3]:
        if puzzle.get("effect"):
            proof.append(normalize(str(puzzle["effect"])))
    if len(proof) < 3 and isinstance(scenario.get("steps_narrative"), list):
        for step in scenario["steps_narrative"]:
            if not isinstance(step, dict):
                continue
            text = normalize(step.get("narrative"))
            if text:
                proof.append(text)
            if len(proof) >= 3:
                break
    while len(proof) < 3:
        proof.append("Validation terrain a confirmer via les checkpoints runtime.")

    method = "Progression V2 par etapes runtime."
    if firmware_steps:
        method = "Progression V2: " + " -> ".join(firmware_steps)

    export_model = {
        "id": scenario_id,
        "title": title,
        "version": scenario.get("version"),
        "players": {
            "min": players.get("min", "?"),
            "max": players.get("max", "?"),
        },
        "duration_minutes": {
            "min": total_minutes,
            "max": total_minutes,
        },
        "canon": {
            "introduction": canon.get("introduction", ""),
            "timeline": timeline,
        },
        "stations": stations,
        "puzzles": puzzles,
        "solution": {
            "culprit": "Aucun coupable unique, epreuve orchestrée par Zacus.",
            "motive": canon.get("stakes", ""),
            "method": method,
            "proof": proof,
        },
        "solution_unique": True,
        "notes": scenario.get("notes", ""),
    }
    return export_model


def manifests_for_scenario(scenario_id: str) -> tuple[str, str]:
    if scenario_id == "zacus_v1":
        return (
            "audio/manifests/zacus_v1_audio.yaml",
            "printables/manifests/zacus_v1_printables.yaml",
        )
    return (
        "audio/manifests/zacus_v2_audio.yaml",
        "printables/manifests/zacus_v2_printables.yaml",
    )


def build_script_doc(model: dict) -> str:
    title = model.get("title") or model.get("id")
    canon = model.get("canon", {})
    timeline = canon.get("timeline", [])
    stations = model.get("stations", [])
    puzzles = model.get("puzzles", [])
    solution = model.get("solution", {})

    lines = [f"# Script - {title}", ""]
    lines += section("Introduction", [normalize(canon.get("introduction")) or "(a completer)"])

    if timeline:
        lines += section(
            "Chronologie canon",
            [f"- **{entry.get('label', 'Moment')}** - {normalize(entry.get('note'))}" for entry in timeline],
        )
    else:
        lines += section("Chronologie canon", ["- A completer dans le YAML."])

    if stations:
        lines += section(
            "Stations",
            [f"- **{station.get('name', 'Station')}** - {normalize(station.get('focus'))}" for station in stations],
        )
    else:
        lines += section("Stations", ["- Pas de stations definies."])

    if puzzles:
        lines += section(
            "Puzzles",
            [f"- {puzzle.get('id')} ({puzzle.get('type')}) - {normalize(puzzle.get('clue'))}" for puzzle in puzzles],
        )
    else:
        lines += section("Puzzles", ["- Aucun puzzle renseigne."])

    solution_lines = [
        f"- Coupable : {solution.get('culprit', 'a definir')}",
        f"- Mobile : {normalize(solution.get('motive'))}",
        f"- Methode : {normalize(solution.get('method'))}",
    ]
    proof = solution.get("proof") or []
    if proof:
        solution_lines.append("- Preuves :")
        solution_lines += [f"  - {normalize(item)}" for item in proof]
    else:
        solution_lines.append("- Preuves : a completer (au moins 3).")

    lines += section("Solution canon", solution_lines)
    lines += section("Notes pratiques", [f"- Solution unique : {model.get('solution_unique')}"])
    return "\n".join(lines).strip() + "\n"


def build_checklist_doc(model: dict, scenario_path: Path) -> str:
    players = model.get("players", {})
    duration = model.get("duration_minutes", {})
    stations = model.get("stations", [])
    puzzles = model.get("puzzles", [])
    solution = model.get("solution", {})
    proof = solution.get("proof") or []
    scenario_id = str(model.get("id", ""))
    audio_manifest, printables_manifest = manifests_for_scenario(scenario_id)

    lines = ["# Checklist scenario", ""]
    lines += section(
        "Configurer",
        [
            f"- Scenario : {model.get('title') or scenario_id}",
            f"- Fichier source : {scenario_path}",
            f"- Participants : {players.get('min', '?')}-{players.get('max', '?')}",
            f"- Duree : {duration.get('min', '?')}-{duration.get('max', '?')} min",
            f"- Stations prevues : {len(stations)}",
            f"- Puzzles : {len(puzzles)}",
            f"- Preuves listees : {len(proof)}",
        ],
    )
    lines += section(
        "Materiel recommande",
        [
            "- Enveloppes numerotees",
            "- Badges detective + fiches d'enquete",
            f"- Audio : {audio_manifest}",
            f"- Printables : suivre {printables_manifest}",
        ],
    )
    lines += section(
        "Verifications",
        [
            f"- python3 tools/scenario/validate_scenario.py {scenario_path}",
            f"- python3 tools/audio/validate_manifest.py {audio_manifest}",
            f"- python3 tools/printables/validate_manifest.py {printables_manifest}",
        ],
    )
    return "\n".join(lines).strip() + "\n"


def build_anti_chaos_doc(model: dict) -> str:
    players = model.get("players", {})
    duration = model.get("duration_minutes", {})
    lines = ["# Guide anti-chaos genere", ""]
    lines += section(
        "Regles",
        [
            f"- Max {players.get('max', '?')} enfants : rotation toutes les 15 min.",
            f"- Duree cible {duration.get('min', '?')}-{duration.get('max', '?')} min : deux pauses courtes.",
            "- Une seule personne parle a la fois, signal sonore pour les transitions.",
            "- Indices gardes sous enveloppe jusqu'a validation du chef de piste.",
        ],
    )
    lines += section(
        "Interventions",
        [
            "- Proposer un mini-defi express si ca deraille.",
            "- Donner un role d'observateur aux enfants en retrait.",
        ],
    )
    return "\n".join(lines).strip() + "\n"


def build_roles_doc() -> str:
    lines = ["# Repartition des roles", ""]
    lines += section(
        "Roles principaux",
        [
            "- Chef de piste : centralise les indices et relit la timeline.",
            "- Chronometreur : marque les phases (exploration, synthese, accusation).",
            "- Analyste technique : gere les codes et les puzzles logiques.",
            "- Archiviste : organise les preuves.",
            "- Temoin narrateur : reformule les consignes anti-chaos.",
            "- Gardien des objets : protege enveloppes, badges et fiches.",
        ],
    )
    lines += section(
        "Roles optionnels",
        [
            "- Detective des emotions : remet le calme si besoin.",
            "- Cartographe : trace le parcours des stations.",
            "- Ambassadeur : valide que rien n'a ete oublie.",
            "- Journaliste : consigne la conclusion finale.",
        ],
    )
    return "\n".join(lines).strip() + "\n"


def build_stations_doc(model: dict) -> str:
    stations = model.get("stations", [])
    puzzles = model.get("puzzles", [])
    lines = ["# Stations detaillees", ""]
    if stations:
        for station in stations:
            lines += [
                f"## {station.get('name', 'Station')}",
                "",
                f"- Focus : {normalize(station.get('focus'))}",
                f"- Clue : {normalize(station.get('clue'))}",
                "",
            ]
    else:
        lines += ["- Aucune station definie dans le scenario."]

    if puzzles:
        lines += ["## Puzzles", ""]
        for puzzle in puzzles:
            lines += [
                f"- {puzzle.get('id')} ({puzzle.get('type')}) : {normalize(puzzle.get('clue'))}",
                f"  Effet : {normalize(puzzle.get('effect'))}",
                "",
            ]
    return "\n".join(lines).strip() + "\n"


def build_brief_doc(model: dict) -> str:
    title = model.get("title") or model.get("id")
    players = model.get("players", {})
    duration = model.get("duration_minutes", {})
    canon = model.get("canon", {})
    timeline = canon.get("timeline", [])
    stations = model.get("stations", [])
    puzzles = model.get("puzzles", [])
    solution = model.get("solution", {})

    lines = [f"# SCENARIO BRIEF - {title}", ""]
    lines += section(
        "Stats",
        [
            f"- Joueurs : {players.get('min', '?')}-{players.get('max', '?')}",
            f"- Duree : {duration.get('min', '?')}-{duration.get('max', '?')} min",
            f"- Solution unique : {model.get('solution_unique')}",
        ],
    )
    lines += section("Introduction", [normalize(canon.get("introduction")) or "(a completer)"])

    if timeline:
        lines += section(
            "Chronologie",
            [f"- **{entry.get('label')}** - {normalize(entry.get('note'))}" for entry in timeline],
        )
    else:
        lines += section("Chronologie", ["- A completer."])

    if stations:
        lines += section(
            "Stations",
            [
                f"- **{station.get('name')}** - {normalize(station.get('focus'))} | Indice : {normalize(station.get('clue'))}"
                for station in stations
            ],
        )
    else:
        lines += section("Stations", ["- A completer."])

    if puzzles:
        lines += section(
            "Puzzles",
            [
                f"- {puzzle.get('id')} ({puzzle.get('type')}) - {normalize(puzzle.get('clue'))}\n  Effet : {normalize(puzzle.get('effect'))}"
                for puzzle in puzzles
            ],
        )
    else:
        lines += section("Puzzles", ["- A completer."])

    solution_lines = [
        f"- Coupable : {solution.get('culprit', 'a definir')}",
        f"- Mobile : {normalize(solution.get('motive'))}",
        f"- Methode : {normalize(solution.get('method'))}",
    ]
    proof = solution.get("proof") or []
    if proof:
        solution_lines += ["- Preuves:"] + [f"  - {normalize(item)}" for item in proof]
    lines += section("Solution", solution_lines)

    notes = model.get("notes")
    if notes:
        lines += section("Notes", [normalize(notes)])
    return "\n".join(lines).strip() + "\n"


def write_file(path: Path, content: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content)


def export_files(scenario_path: Path, out_path: Path | None = None) -> None:
    if not scenario_path.exists():
        print(f"Scenario file not found: {scenario_path}")
        sys.exit(1)

    scenario = yaml.safe_load(scenario_path.read_text())
    if not isinstance(scenario, dict):
        print(f"Invalid scenario format: {scenario_path}")
        sys.exit(1)

    model = to_export_model(scenario)
    brief_doc = build_brief_doc(model)

    files: dict[Path, str] = {
        KIT_DIR / "script.md": build_script_doc(model),
        KIT_DIR / "checklist.md": build_checklist_doc(model, scenario_path),
        KIT_DIR / "anti-chaos.md": build_anti_chaos_doc(model),
        KIT_DIR / "roles.md": build_roles_doc(),
        KIT_DIR / "stations.md": build_stations_doc(model),
        DOCS_DIR / "SCENARIO_BRIEF.md": brief_doc,
    }

    if out_path is not None:
        files[out_path] = brief_doc

    for path, content in files.items():
        write_file(path, content)

    print("[export] generated Markdown briefs for", scenario_path.name)


def main() -> int:
    parser = argparse.ArgumentParser(description="Export Zacus scenario briefs as Markdown.")
    parser.add_argument("scenario", type=Path, help="Path to the scenario YAML file")
    parser.add_argument(
        "-o",
        "--out",
        type=Path,
        default=None,
        help="Optional extra output path for SCENARIO_BRIEF markdown copy.",
    )
    args = parser.parse_args()

    export_files(args.scenario, args.out)
    return 0


if __name__ == "__main__":
    sys.exit(main())
