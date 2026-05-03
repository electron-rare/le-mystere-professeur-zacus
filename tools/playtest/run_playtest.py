#!/usr/bin/env python3
"""Replay a Zacus scenario playtest against the Runtime 3 IR.

Phase P1 of the Playtest Harness (see
docs/superpowers/specs/2026-05-03-playtest-harness-design.md).

Reads:
  * a Runtime 3 source scenario YAML (compiled via runtime3_common)
  * a playtest YAML (scripted events + optional critical asserts)

Replays each event by selecting the matching transition from the current
step, captures a transcript (initial step + step taken at each event),
and either compares it to a committed snapshot JSON or writes the
snapshot when ``--update`` is passed.

Exit codes:
  0 = pass (snapshot match + critical asserts ok, or snapshot written)
  1 = diff (snapshot mismatch or critical assert failure)
  2 = error (bad input, IR validation failure, missing transition, ...)
"""

from __future__ import annotations

import argparse
import json
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any

REPO_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO_ROOT / "tools" / "scenario"))

from runtime3_common import (  # noqa: E402  (sys.path manipulated above)
    compile_runtime3_document,
    normalize_event_type,
    normalize_token,
    read_yaml,
    validate_runtime3_document,
)


SNAPSHOT_VERSION = "zacus.playtest.snapshot.v1"


@dataclass
class TranscriptEntry:
    """One observable state captured during replay."""

    at: int
    event_type: str
    event_name: str
    step_id: str
    scene_id: str
    audio_pack_id: str
    narrative: str

    def to_dict(self) -> dict[str, Any]:
        return {
            "at": self.at,
            "event_type": self.event_type,
            "event_name": self.event_name,
            "step_id": self.step_id,
            "scene_id": self.scene_id,
            "audio_pack_id": self.audio_pack_id,
            "narrative": self.narrative,
        }


@dataclass
class ReplayResult:
    scenario_id: str
    playtest_name: str
    initial_step_id: str
    final_step_id: str
    history: list[str]
    transcript: list[TranscriptEntry] = field(default_factory=list)

    def to_snapshot(self) -> dict[str, Any]:
        return {
            "snapshot_version": SNAPSHOT_VERSION,
            "scenario_id": self.scenario_id,
            "playtest_name": self.playtest_name,
            "initial_step_id": self.initial_step_id,
            "final_step_id": self.final_step_id,
            "history": list(self.history),
            "transcript": [entry.to_dict() for entry in self.transcript],
        }


class PlaytestError(RuntimeError):
    """Hard failure during replay (missing transition, malformed input)."""


def _resolve_path(base: Path, candidate: str) -> Path:
    path = Path(candidate)
    return path if path.is_absolute() else (base / candidate).resolve()


def _load_playtest(playtest_path: Path) -> dict[str, Any]:
    data = read_yaml(playtest_path)
    if "events" not in data or not isinstance(data["events"], list):
        raise PlaytestError(f"{playtest_path}: missing or invalid 'events' list")
    return data


def _select_transition(
    transitions: list[dict[str, Any]],
    event_type: str,
    event_name: str,
) -> dict[str, Any] | None:
    """Pick the highest-priority transition matching (event_type, event_name)."""
    matches = [
        transition
        for transition in transitions
        if transition.get("event_type") == event_type
        and transition.get("event_name") == event_name
    ]
    if not matches:
        return None
    matches.sort(key=lambda item: item.get("priority", 0), reverse=True)
    return matches[0]


def replay(
    scenario_document: dict[str, Any],
    playtest: dict[str, Any],
) -> ReplayResult:
    validate_runtime3_document(scenario_document)
    steps_by_id = {step["id"]: step for step in scenario_document["steps"]}
    scenario_id = scenario_document["scenario"]["id"]
    entry_step_id = scenario_document["scenario"]["entry_step_id"]
    playtest_name = str(playtest.get("name") or playtest.get("id") or "unnamed_playtest")

    current = entry_step_id
    history = [current]
    transcript: list[TranscriptEntry] = []

    for index, raw_event in enumerate(playtest["events"]):
        if not isinstance(raw_event, dict):
            raise PlaytestError(f"event #{index}: must be a mapping")
        at = int(raw_event.get("at", 0))
        event_type = normalize_event_type(str(raw_event.get("type", "")))
        event_name = normalize_token(str(raw_event.get("name", "")), "")
        if not event_name:
            raise PlaytestError(
                f"event #{index} (at={at}): 'name' is required (token form)"
            )
        step = steps_by_id[current]
        transition = _select_transition(step.get("transitions", []), event_type, event_name)
        if transition is None:
            raise PlaytestError(
                f"event #{index} (at={at}): no transition for "
                f"({event_type}, {event_name}) from step {current}"
            )
        current = transition["target_step_id"]
        history.append(current)
        target_step = steps_by_id[current]
        transcript.append(
            TranscriptEntry(
                at=at,
                event_type=event_type,
                event_name=event_name,
                step_id=current,
                scene_id=str(target_step.get("scene_id", "")),
                audio_pack_id=str(target_step.get("audio_pack_id", "")),
                narrative=str(target_step.get("narrative", "")),
            )
        )

    return ReplayResult(
        scenario_id=scenario_id,
        playtest_name=playtest_name,
        initial_step_id=entry_step_id,
        final_step_id=current,
        history=history,
        transcript=transcript,
    )


def _diff_snapshots(actual: dict[str, Any], expected: dict[str, Any]) -> list[str]:
    actual_text = json.dumps(actual, ensure_ascii=False, indent=2, sort_keys=True)
    expected_text = json.dumps(expected, ensure_ascii=False, indent=2, sort_keys=True)
    if actual_text == expected_text:
        return []
    import difflib

    return list(
        difflib.unified_diff(
            expected_text.splitlines(),
            actual_text.splitlines(),
            fromfile="snapshot.expected",
            tofile="snapshot.actual",
            lineterm="",
        )
    )


def _write_snapshot(snapshot_path: Path, payload: dict[str, Any]) -> None:
    snapshot_path.parent.mkdir(parents=True, exist_ok=True)
    snapshot_path.write_text(
        json.dumps(payload, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description=(
            "Replay a Zacus playtest YAML against the Runtime 3 IR and "
            "compare to (or update) a snapshot JSON."
        ),
    )
    parser.add_argument(
        "--scenario",
        required=True,
        help="Path to the Runtime 3 source scenario YAML (e.g. game/scenarios/zacus_v2.yaml)",
    )
    parser.add_argument(
        "--playtest",
        required=True,
        help="Path to the playtest YAML (events + asserts)",
    )
    parser.add_argument(
        "--snapshot",
        default=None,
        help="Path to the snapshot JSON (defaults next to the playtest)",
    )
    parser.add_argument(
        "--update",
        action="store_true",
        help="Regenerate the snapshot instead of comparing",
    )
    args = parser.parse_args(argv)

    scenario_path = Path(args.scenario).resolve()
    playtest_path = Path(args.playtest).resolve()
    snapshot_path = (
        Path(args.snapshot).resolve()
        if args.snapshot
        else playtest_path.with_suffix(".snapshot.json")
    )

    try:
        scenario_doc = compile_runtime3_document(read_yaml(scenario_path))
    except Exception as exc:  # noqa: BLE001 — surface compile errors as exit 2
        print(f"[playtest] ERROR compiling scenario: {exc}", file=sys.stderr)
        return 2

    try:
        playtest = _load_playtest(playtest_path)
    except Exception as exc:  # noqa: BLE001
        print(f"[playtest] ERROR loading playtest: {exc}", file=sys.stderr)
        return 2

    try:
        result = replay(scenario_doc, playtest)
    except PlaytestError as exc:
        print(f"[playtest] ERROR during replay: {exc}", file=sys.stderr)
        return 2
    except Exception as exc:  # noqa: BLE001
        print(f"[playtest] ERROR during replay: {exc}", file=sys.stderr)
        return 2

    snapshot_payload = result.to_snapshot()

    if args.update:
        _write_snapshot(snapshot_path, snapshot_payload)
        print(f"[playtest] snapshot written: {snapshot_path}")
        return 0

    if not snapshot_path.exists():
        print(
            f"[playtest] ERROR: snapshot not found ({snapshot_path}). "
            "Run with --update to create it.",
            file=sys.stderr,
        )
        return 1

    expected = json.loads(snapshot_path.read_text(encoding="utf-8"))
    diff = _diff_snapshots(snapshot_payload, expected)
    if diff:
        print(f"[playtest] FAIL: snapshot mismatch for {playtest_path.name}")
        for line in diff:
            print(line)
        return 1

    # P1: critical_asserts evaluator deferred to a later phase. We only
    # gate on snapshot equality here; the spec keeps the asserts block
    # forward-compatible by ignoring unknown keys at this stage.
    print(
        f"[playtest] PASS: {result.playtest_name} "
        f"({len(result.transcript)} events, final={result.final_step_id})"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
