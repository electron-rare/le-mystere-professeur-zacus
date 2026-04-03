#!/usr/bin/env python3
"""
collect_metrics.py — Collecte et analyse des métriques de playtest.

Lit les logs série ESP-NOW de BOX-3, parse les événements de résolution,
demandes d'indices et décisions NPC. Génère un rapport Markdown de session
et l'ajoute au fichier metrics_template.csv.

Usage:
    # Lire depuis port série (BOX-3 connecté en USB)
    python3 tools/playtest/collect_metrics.py --port /dev/cu.usbserial-XXXX

    # Lire depuis un fichier log pré-capturé
    python3 tools/playtest/collect_metrics.py --log-file session_001.log

    # Spécifier un identifiant de session
    python3 tools/playtest/collect_metrics.py --log-file session_001.log --session beta_001

    # Écrire le rapport dans un dossier spécifique
    python3 tools/playtest/collect_metrics.py --log-file session_001.log --output-dir docs/playtest/reports/
"""

import argparse
import csv
import re
import sys
from datetime import datetime, timedelta
from pathlib import Path

# ---------------------------------------------------------------------------
# Constantes
# ---------------------------------------------------------------------------

PUZZLES = ["P1_SON", "P2_CIRCUIT", "P3_QR", "P4_RADIO", "P5_MORSE", "P6_SYMBOLES", "P7_COFFRE"]

PUZZLE_LABELS = {
    "P1_SON": "Séquence Sonore",
    "P2_CIRCUIT": "Circuit LED",
    "P3_QR": "QR Treasure",
    "P4_RADIO": "Fréquence Radio",
    "P5_MORSE": "Code Morse",
    "P6_SYMBOLES": "Symboles Alchimiques",
    "P7_COFFRE": "Coffre Final",
}

EXPECTED_DURATIONS_S = {
    "P1_SON": 300,
    "P2_CIRCUIT": 360,
    "P3_QR": 240,
    "P4_RADIO": 300,
    "P5_MORSE": 420,
    "P6_SYMBOLES": 360,
    "P7_COFFRE": 300,
}

METRICS_CSV = Path(__file__).parent.parent.parent / "docs" / "playtest" / "metrics_template.csv"

# ---------------------------------------------------------------------------
# Patterns de log ESP-NOW / BOX-3
# ---------------------------------------------------------------------------

# Exemple : [12:05:33] [ESPNOW] P1_SON: SOLVED in 287s (attempts: 2)
RE_SOLVED = re.compile(
    r"\[(?P<ts>\d{2}:\d{2}:\d{2})\]\s+\[ESPNOW\]\s+(?P<puzzle>P\d+_\w+):\s+SOLVED\s+in\s+(?P<duration>\d+)s"
    r"(?:\s+\(attempts:\s*(?P<attempts>\d+)\))?"
)

# Exemple : [12:08:01] [ESPNOW] P1_SON: SKIPPED
RE_SKIPPED = re.compile(
    r"\[(?P<ts>\d{2}:\d{2}:\d{2})\]\s+\[ESPNOW\]\s+(?P<puzzle>P\d+_\w+):\s+SKIPPED"
)

# Exemple : [12:06:12] [NPC] HINT requested: P1_SON level 2
RE_HINT = re.compile(
    r"\[(?P<ts>\d{2}:\d{2}:\d{2})\]\s+\[NPC\]\s+HINT requested:\s+(?P<puzzle>P\d+_\w+)\s+level\s+(?P<level>\d+)"
)

# Exemple : [11:55:00] [GAME] START target=60min group_size=5
RE_GAME_START = re.compile(
    r"\[(?P<ts>\d{2}:\d{2}:\d{2})\]\s+\[GAME\]\s+START\s+target=(?P<target>\d+)min\s+group_size=(?P<size>\d+)"
)

# Exemple : [12:55:12] [GAME] END score=1150 profile=TECH profile_correct=true
RE_GAME_END = re.compile(
    r"\[(?P<ts>\d{2}:\d{2}:\d{2})\]\s+\[GAME\]\s+END\s+score=(?P<score>\d+)"
    r"\s+profile=(?P<profile>\w+)\s+profile_correct=(?P<correct>\w+)"
)

# Exemple : [11:56:10] [NPC] PROFILE_DETECTED: TECH
RE_PROFILE = re.compile(
    r"\[(?P<ts>\d{2}:\d{2}:\d{2})\]\s+\[NPC\]\s+PROFILE_DETECTED:\s+(?P<profile>\w+)"
)

# Exemple : [11:57:00] [NPC] PHRASE: adaptation.group_tech_detected.0 via XTTS
RE_PHRASE = re.compile(
    r"\[(?P<ts>\d{2}:\d{2}:\d{2})\]\s+\[NPC\]\s+PHRASE:\s+(?P<key>\S+)\s+via\s+(?P<backend>\w+)"
)

# Exemple : [12:10:00] [HW] FAILURE: P4_RADIO ESP-NOW timeout
RE_HW_FAILURE = re.compile(
    r"\[(?P<ts>\d{2}:\d{2}:\d{2})\]\s+\[HW\]\s+FAILURE:\s+(?P<description>.+)"
)

# ---------------------------------------------------------------------------
# Parseur de logs
# ---------------------------------------------------------------------------

def parse_log(lines: list[str]) -> dict:
    """Parse les lignes de log BOX-3 et retourne les données de session."""
    session: dict = {
        "start_ts": None,
        "end_ts": None,
        "target_minutes": 0,
        "group_size": 0,
        "profile_detected": "UNKNOWN",
        "profile_correct": None,
        "final_score": 0,
        "puzzles": {},
        "hints": [],
        "phrases": [],
        "hw_failures": [],
    }

    for line in lines:
        line = line.rstrip()

        m = RE_GAME_START.search(line)
        if m:
            session["start_ts"] = m.group("ts")
            session["target_minutes"] = int(m.group("target"))
            session["group_size"] = int(m.group("size"))
            continue

        m = RE_GAME_END.search(line)
        if m:
            session["end_ts"] = m.group("ts")
            session["final_score"] = int(m.group("score"))
            session["profile_detected"] = m.group("profile")
            session["profile_correct"] = m.group("correct").lower() == "true"
            continue

        m = RE_PROFILE.search(line)
        if m:
            session["profile_detected"] = m.group("profile")
            continue

        m = RE_SOLVED.search(line)
        if m:
            puzzle = m.group("puzzle")
            session["puzzles"][puzzle] = {
                "status": "SOLVED",
                "ts": m.group("ts"),
                "duration_s": int(m.group("duration")),
                "attempts": int(m.group("attempts")) if m.group("attempts") else 1,
            }
            continue

        m = RE_SKIPPED.search(line)
        if m:
            puzzle = m.group("puzzle")
            session["puzzles"][puzzle] = {
                "status": "SKIPPED",
                "ts": m.group("ts"),
                "duration_s": None,
                "attempts": 0,
            }
            continue

        m = RE_HINT.search(line)
        if m:
            session["hints"].append({
                "ts": m.group("ts"),
                "puzzle": m.group("puzzle"),
                "level": int(m.group("level")),
            })
            continue

        m = RE_PHRASE.search(line)
        if m:
            session["phrases"].append({
                "ts": m.group("ts"),
                "key": m.group("key"),
                "backend": m.group("backend"),
            })
            continue

        m = RE_HW_FAILURE.search(line)
        if m:
            session["hw_failures"].append({
                "ts": m.group("ts"),
                "description": m.group("description"),
            })

    return session


def _ts_to_minutes(ts_start: str | None, ts_end: str | None) -> float:
    """Calcule la durée en minutes entre deux timestamps HH:MM:SS."""
    if not ts_start or not ts_end:
        return 0.0
    fmt = "%H:%M:%S"
    try:
        t0 = datetime.strptime(ts_start, fmt)
        t1 = datetime.strptime(ts_end, fmt)
        delta = t1 - t0
        if delta.total_seconds() < 0:
            # Passage minuit
            delta += timedelta(hours=24)
        return delta.total_seconds() / 60
    except ValueError:
        return 0.0

# ---------------------------------------------------------------------------
# Génération du rapport Markdown
# ---------------------------------------------------------------------------

def generate_report(session_id: str, session: dict) -> str:
    """Génère un rapport Markdown complet pour la session."""
    actual_minutes = _ts_to_minutes(session["start_ts"], session["end_ts"])
    target_minutes = session["target_minutes"]
    duration_accuracy_pct = (
        abs(actual_minutes - target_minutes) / target_minutes * 100
        if target_minutes > 0 else 0.0
    )

    solved_puzzles = [p for p, d in session["puzzles"].items() if d["status"] == "SOLVED"]
    skipped_puzzles = [p for p, d in session["puzzles"].items() if d["status"] == "SKIPPED"]
    hints_total = len(session["hints"])
    hw_failures = session["hw_failures"]

    # Backend TTS stats
    backends_used = [p["backend"] for p in session["phrases"]]
    xtts_count = backends_used.count("XTTS")
    piper_count = backends_used.count("PIPER")
    sdcard_count = backends_used.count("SDCARD")

    lines = [
        f"# Rapport de Session — {session_id}",
        f"",
        f"Généré le : {datetime.now().strftime('%Y-%m-%d %H:%M')}",
        f"",
        f"---",
        f"",
        f"## Résumé",
        f"",
        f"| Métrique | Valeur |",
        f"|----------|--------|",
        f"| Session ID | `{session_id}` |",
        f"| Taille du groupe | {session['group_size']} joueurs |",
        f"| Durée cible | {target_minutes} min |",
        f"| Durée réelle | {actual_minutes:.1f} min |",
        f"| Précision durée | ±{duration_accuracy_pct:.1f}% |",
        f"| Profil NPC détecté | {session['profile_detected']} |",
        f"| Profil correct | {'OUI' if session['profile_correct'] else 'NON' if session['profile_correct'] is not None else 'N/A'} |",
        f"| Puzzles résolus | {len(solved_puzzles)}/{len(PUZZLES)} |",
        f"| Puzzles sautés | {len(skipped_puzzles)} |",
        f"| Indices utilisés | {hints_total} |",
        f"| Score final | {session['final_score']} |",
        f"| Pannes matériel | {len(hw_failures)} |",
        f"",
    ]

    # Critères de succès
    lines += [
        f"## Critères de Succès",
        f"",
        f"| Critère | Objectif | Résultat | Statut |",
        f"|---------|----------|----------|--------|",
        f"| Temps de setup | ≤ 15 min | — | — |",
        f"| Précision durée | ≤ ±10% | ±{duration_accuracy_pct:.1f}% | {'PASS' if duration_accuracy_pct <= 10 else 'FAIL'} |",
        f"| Profil NPC correct | OUI | {'OUI' if session['profile_correct'] else 'NON'} | {'PASS' if session['profile_correct'] else 'FAIL'} |",
        f"| Pannes matériel | 0 | {len(hw_failures)} | {'PASS' if not hw_failures else 'FAIL'} |",
        f"| Jeu terminé | OUI | {'OUI' if 'P7_COFFRE' in solved_puzzles else 'NON'} | {'PASS' if 'P7_COFFRE' in solved_puzzles else 'FAIL'} |",
        f"",
    ]

    # Détail puzzles
    lines += [
        f"## Détail par Puzzle",
        f"",
        f"| Puzzle | Statut | Durée réelle | Durée attendue | Écart | Tentatives | Indices |",
        f"|--------|--------|-------------|----------------|-------|-----------|---------|",
    ]
    for puzzle_id in PUZZLES:
        label = PUZZLE_LABELS.get(puzzle_id, puzzle_id)
        data = session["puzzles"].get(puzzle_id, {"status": "NOT_REACHED", "duration_s": None, "attempts": 0})
        expected = EXPECTED_DURATIONS_S.get(puzzle_id, 0)
        puzzle_hints = [h for h in session["hints"] if h["puzzle"] == puzzle_id]

        status = data["status"]
        duration_s = data.get("duration_s")
        duration_str = f"{duration_s}s" if duration_s is not None else "—"
        expected_str = f"{expected}s"
        ecart_str = f"{duration_s - expected:+d}s" if duration_s is not None else "—"
        attempts = data.get("attempts", 0)

        lines.append(
            f"| {label} | {status} | {duration_str} | {expected_str} | {ecart_str} | {attempts} | {len(puzzle_hints)} |"
        )

    lines += [""]

    # Indices demandés
    if session["hints"]:
        lines += [
            f"## Indices Demandés",
            f"",
            f"| Heure | Puzzle | Niveau |",
            f"|-------|--------|--------|",
        ]
        for hint in session["hints"]:
            puzzle_label = PUZZLE_LABELS.get(hint["puzzle"], hint["puzzle"])
            lines.append(f"| {hint['ts']} | {puzzle_label} | {hint['level']} |")
        lines += [""]

    # NPC / voix
    if session["phrases"]:
        lines += [
            f"## Interventions NPC",
            f"",
            f"- Phrases jouées : {len(session['phrases'])}",
            f"- XTTS-v2 (GPU) : {xtts_count}",
            f"- Piper TTS : {piper_count}",
            f"- SD Card fallback : {sdcard_count}",
            f"",
        ]

    # Pannes matériel
    if hw_failures:
        lines += [
            f"## Pannes Matériel",
            f"",
            f"| Heure | Description |",
            f"|-------|-------------|",
        ]
        for failure in hw_failures:
            lines.append(f"| {failure['ts']} | {failure['description']} |")
        lines += [""]

    lines += [
        f"---",
        f"",
        f"*Rapport généré automatiquement par `tools/playtest/collect_metrics.py`*",
    ]

    return "\n".join(lines)


# ---------------------------------------------------------------------------
# Écriture CSV
# ---------------------------------------------------------------------------

def append_to_csv(session_id: str, session: dict, csv_path: Path, nps_score: float = 0.0) -> None:
    """Ajoute une ligne de métriques au fichier CSV de référence."""
    actual_minutes = _ts_to_minutes(session["start_ts"], session["end_ts"])
    solved = [p for p, d in session["puzzles"].items() if d["status"] == "SOLVED"]
    skipped = [p for p, d in session["puzzles"].items() if d["status"] == "SKIPPED"]

    row = {
        "session_id": session_id,
        "date": datetime.now().strftime("%Y-%m-%d"),
        "group_size": session["group_size"],
        "target_duration": session["target_minutes"],
        "actual_duration": round(actual_minutes, 1),
        "group_profile_detected": session["profile_detected"],
        "group_profile_actual": "",
        "puzzles_solved": len(solved),
        "puzzles_skipped": len(skipped),
        "hints_total": len(session["hints"]),
        "nps_score": nps_score,
        "notes": "",
    }

    fieldnames = [
        "session_id", "date", "group_size", "target_duration", "actual_duration",
        "group_profile_detected", "group_profile_actual", "puzzles_solved",
        "puzzles_skipped", "hints_total", "nps_score", "notes",
    ]

    # Lire les lignes existantes
    existing_rows: list[dict] = []
    if csv_path.exists():
        with open(csv_path, newline="", encoding="utf-8") as f:
            reader = csv.DictReader(f)
            for r in reader:
                if r.get("session_id") != session_id:
                    existing_rows.append(r)

    # Ré-écrire avec la nouvelle ligne
    with open(csv_path, "w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()
        for r in existing_rows:
            writer.writerow(r)
        writer.writerow(row)

    print(f"CSV mis à jour : {csv_path}")


# ---------------------------------------------------------------------------
# Lecture des sources de données
# ---------------------------------------------------------------------------

def read_from_file(path: str) -> list[str]:
    """Lit les lignes depuis un fichier log."""
    with open(path, encoding="utf-8") as f:
        return f.readlines()


def read_from_serial(port: str, timeout_seconds: int = 7200) -> list[str]:
    """Lit les lignes depuis un port série (BOX-3 connecté en USB)."""
    try:
        import serial  # type: ignore
    except ImportError:
        print("Erreur : pyserial non installé. Lancer : pip install pyserial", file=sys.stderr)
        sys.exit(1)

    lines: list[str] = []
    print(f"Lecture du port série {port} (Ctrl+C pour arrêter)...")
    with serial.Serial(port, baudrate=115200, timeout=1) as ser:
        start = datetime.now()
        game_ended = False
        while True:
            line = ser.readline().decode("utf-8", errors="replace")
            if line:
                lines.append(line)
                print(line, end="", flush=True)
                if "[GAME] END" in line:
                    game_ended = True
                    print("\n[collect_metrics] Fin de session détectée.")
                    break
            elapsed = (datetime.now() - start).total_seconds()
            if elapsed > timeout_seconds:
                print(f"\n[collect_metrics] Timeout {timeout_seconds}s atteint.")
                break
    return lines


# ---------------------------------------------------------------------------
# Point d'entrée
# ---------------------------------------------------------------------------

def main() -> None:
    parser = argparse.ArgumentParser(
        description="Collecte et analyse des métriques de playtest Zacus V3"
    )
    source = parser.add_mutually_exclusive_group(required=True)
    source.add_argument("--port", help="Port série BOX-3 (ex: /dev/cu.usbserial-XXXX)")
    source.add_argument("--log-file", help="Fichier log pré-capturé")

    parser.add_argument("--session", default=None, help="Identifiant de session (ex: beta_001)")
    parser.add_argument("--output-dir", default=None, help="Dossier de sortie pour le rapport Markdown")
    parser.add_argument("--nps-score", type=float, default=0.0, help="Score NPS moyen du groupe (Q1, 0-10)")
    parser.add_argument("--no-csv", action="store_true", help="Ne pas mettre à jour le fichier CSV")
    args = parser.parse_args()

    # Identifiant de session
    session_id = args.session or f"session_{datetime.now().strftime('%Y%m%d_%H%M%S')}"

    # Lecture des données
    if args.port:
        lines = read_from_serial(args.port)
    else:
        lines = read_from_file(args.log_file)

    if not lines:
        print("Erreur : aucune donnée à analyser.", file=sys.stderr)
        sys.exit(1)

    # Analyse
    session = parse_log(lines)

    # Rapport Markdown
    report_md = generate_report(session_id, session)

    output_dir = Path(args.output_dir) if args.output_dir else Path(__file__).parent.parent.parent / "docs" / "playtest" / "reports"
    output_dir.mkdir(parents=True, exist_ok=True)
    report_path = output_dir / f"{session_id}_report.md"
    report_path.write_text(report_md, encoding="utf-8")
    print(f"Rapport Markdown : {report_path}")

    # Affichage rapide
    actual_minutes = _ts_to_minutes(session["start_ts"], session["end_ts"])
    solved = [p for p, d in session["puzzles"].items() if d["status"] == "SOLVED"]
    print(f"\nRésumé session {session_id}:")
    print(f"  Groupe     : {session['group_size']} joueurs")
    print(f"  Durée      : {actual_minutes:.1f} min / {session['target_minutes']} min cible")
    print(f"  Profil NPC : {session['profile_detected']} ({'correct' if session['profile_correct'] else 'incorrect'})")
    print(f"  Puzzles    : {len(solved)}/{len(PUZZLES)} résolus, {len(session['hints'])} indices")
    print(f"  Score      : {session['final_score']}")
    print(f"  NPS        : {args.nps_score}/10")
    print(f"  HW pannes  : {len(session['hw_failures'])}")

    # Mise à jour CSV
    if not args.no_csv:
        append_to_csv(session_id, session, METRICS_CSV, nps_score=args.nps_score)


if __name__ == "__main__":
    main()
