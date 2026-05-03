#!/usr/bin/env python3
"""Shared helpers for Zacus Runtime 3 scaffolding."""

from __future__ import annotations

import json
import logging
import re
from dataclasses import dataclass
from pathlib import Path
from typing import Any

logger = logging.getLogger(__name__)

import yaml


EVENT_TYPES = {
    "button",
    "serial",
    "timer",
    "audio_done",
    "unlock",
    "espnow",
    "action",
}


def normalize_event_type(raw: str) -> str:
    token = str(raw or "").strip().lower()
    return token if token in EVENT_TYPES else "serial"


def normalize_token(raw: str, fallback: str) -> str:
    cleaned = re.sub(r"[^A-Za-z0-9_]+", "_", (raw or "").strip()).strip("_").upper()
    if not cleaned:
        logger.warning("normalize_token: empty result from raw=%r, using fallback=%r", raw, fallback)
        return fallback
    return cleaned


def read_yaml(path: Path) -> dict[str, Any]:
    data = yaml.safe_load(path.read_text(encoding="utf-8"))
    if not isinstance(data, dict):
        raise ValueError(f"{path} does not contain a mapping at the root")
    return data


def _linear_transition(next_step_id: str) -> dict[str, Any]:
    return {
        "event_type": "serial",
        "event_name": "BTN_NEXT",
        "target_step_id": next_step_id,
        "priority": 0,
        "after_ms": 0,
    }


def _normalize_transition(transition: dict[str, Any], fallback_target: str) -> dict[str, Any]:
    target_step_id = normalize_token(str(transition.get("target_step_id", "")), fallback_target)
    return {
        "event_type": normalize_event_type(str(transition.get("event_type", ""))),
        "event_name": normalize_token(str(transition.get("event_name", "")), "BTN_NEXT"),
        "target_step_id": target_step_id,
        "priority": int(transition.get("priority", 0)),
        "after_ms": int(transition.get("after_ms", 0)),
    }


def _compile_runtime3_steps_from_firmware(
    data: dict[str, Any],
    narrative_by_id: dict[str, dict[str, Any]],
) -> tuple[list[dict[str, Any]], str] | None:
    firmware = data.get("firmware")
    if not isinstance(firmware, dict):
        return None
    firmware_steps = firmware.get("steps")
    if not isinstance(firmware_steps, list) or not firmware_steps:
        return None

    compiled_steps: list[dict[str, Any]] = []
    normalized_ids: list[str] = []
    for index, step in enumerate(firmware_steps):
        if not isinstance(step, dict):
            continue
        step_id = normalize_token(str(step.get("step_id", "")), f"STEP_{index + 1}")
        normalized_ids.append(step_id)

    if not normalized_ids:
        return None

    for index, step in enumerate(firmware_steps):
        if not isinstance(step, dict):
            continue
        step_id = normalized_ids[index]
        narrative = narrative_by_id.get(step_id, {})
        raw_transitions = step.get("transitions")
        transitions: list[dict[str, Any]] = []
        if isinstance(raw_transitions, list) and raw_transitions:
            for transition in raw_transitions:
                if not isinstance(transition, dict):
                    continue
                transitions.append(
                    _normalize_transition(
                        transition,
                        normalized_ids[index + 1] if index < len(normalized_ids) - 1 else step_id,
                    )
                )
        elif index < len(normalized_ids) - 1:
            transitions.append(_linear_transition(normalized_ids[index + 1]))

        compiled_steps.append(
            {
                "id": step_id,
                "scene_id": normalize_token(str(step.get("screen_scene_id", "")), f"SCENE_{index + 1}"),
                "audio_pack_id": normalize_token(str(step.get("audio_pack_id", "")), ""),
                "actions": [str(action) for action in step.get("actions", []) if str(action).strip()],
                "apps": [str(app) for app in step.get("apps", []) if str(app).strip()],
                "transitions": transitions,
                "narrative": str(narrative.get("narrative", "")),
            }
        )

    entry_step_id = normalize_token(str(firmware.get("initial_step", "")), compiled_steps[0]["id"])
    return compiled_steps, entry_step_id


def _parse_major_version(raw: Any) -> int:
    """Tolerate both int versions (1, 2, 3) and semantic strings ("3.1").

    Returns the major component as an int. Falls back to 1 if unparseable.
    """
    if isinstance(raw, int):
        return raw
    text = str(raw).strip()
    if not text:
        return 1
    try:
        return int(text)
    except ValueError:
        head = text.split(".", 1)[0]
        try:
            return int(head)
        except ValueError:
            return 1


def compile_runtime3_document(data: dict[str, Any], source_kind: str = "yaml") -> dict[str, Any]:
    scenario_id = normalize_token(str(data.get("id", "")), "ZACUS_RUNTIME3")
    version = _parse_major_version(data.get("version", 1))
    title = str(data.get("title", scenario_id))
    firmware = data.get("firmware") if isinstance(data.get("firmware"), dict) else {}
    steps_reference_order = (
        firmware.get("steps_reference_order")
        or data.get("steps_reference_order")
        or []
    )
    steps_narrative = data.get("steps_narrative") or []

    narrative_by_id: dict[str, dict[str, Any]] = {}
    for entry in steps_narrative:
        if isinstance(entry, dict):
            step_id = normalize_token(str(entry.get("step_id", "")), "")
            if step_id:
                narrative_by_id[step_id] = entry

    normalized_order: list[str] = [
        normalize_token(str(step_id), f"STEP_{index + 1}")
        for index, step_id in enumerate(steps_reference_order)
    ]

    if not normalized_order and narrative_by_id:
        normalized_order = list(narrative_by_id.keys())

    if not normalized_order:
        normalized_order = ["STEP_BOOT"]

    compiled_from_firmware = _compile_runtime3_steps_from_firmware(data, narrative_by_id)
    if compiled_from_firmware is not None:
        steps, entry_step_id = compiled_from_firmware
        migration_mode = "firmware_import"
    else:
        steps = []
        for index, step_id in enumerate(normalized_order):
            narrative = narrative_by_id.get(step_id, {})
            scene_id = normalize_token(str(narrative.get("scene", "")), f"SCENE_{index + 1}")
            transitions: list[dict[str, Any]] = []
            if index < len(normalized_order) - 1:
                transitions.append(_linear_transition(normalized_order[index + 1]))
            steps.append(
                {
                    "id": step_id,
                    "scene_id": scene_id,
                    "audio_pack_id": "",
                    "actions": [],
                    "apps": [],
                    "transitions": transitions,
                    "narrative": str(narrative.get("narrative", "")),
                }
            )
        entry_step_id = normalized_order[0]
        migration_mode = "linear_import"

    return {
        "schema_version": "zacus.runtime3.v1",
        "scenario": {
            "id": scenario_id,
            "version": version,
            "title": title,
            "entry_step_id": entry_step_id,
            "source_kind": source_kind,
        },
        "steps": steps,
        "metadata": {
            "migration_mode": migration_mode,
            "generated_by": "tools/scenario/compile_runtime3.py",
        },
    }


def validate_runtime3_document(document: dict[str, Any]) -> None:
    if document.get("schema_version") != "zacus.runtime3.v1":
        raise ValueError("schema_version must be zacus.runtime3.v1")
    scenario = document.get("scenario")
    steps = document.get("steps")
    if not isinstance(scenario, dict):
        raise ValueError("scenario must be an object")
    if not isinstance(steps, list) or not steps:
        raise ValueError("steps must be a non-empty list")
    entry = scenario.get("entry_step_id")
    step_ids = set()
    for step in steps:
        if not isinstance(step, dict):
            raise ValueError("each step must be an object")
        step_id = step.get("id")
        if not isinstance(step_id, str) or not step_id:
            raise ValueError("each step requires a non-empty id")
        if step_id in step_ids:
            raise ValueError(f"duplicate step id: {step_id}")
        step_ids.add(step_id)
        transitions = step.get("transitions", [])
        if not isinstance(transitions, list):
            raise ValueError(f"step {step_id} transitions must be a list")
        for transition in transitions:
            if not isinstance(transition, dict):
                raise ValueError(f"step {step_id} has an invalid transition")
            event_type = transition.get("event_type")
            if event_type not in EVENT_TYPES:
                raise ValueError(f"step {step_id} has unsupported event_type {event_type}")
            target = transition.get("target_step_id")
            if not isinstance(target, str) or not target:
                raise ValueError(f"step {step_id} has a transition without target_step_id")
    if entry not in step_ids:
        raise ValueError("entry_step_id must point to a declared step")
    for step in steps:
        for transition in step.get("transitions", []):
            target = transition["target_step_id"]
            if target not in step_ids:
                raise ValueError(f"transition target does not exist: {target}")

    # Cycle detection via DFS
    adjacency: dict[str, list[str]] = {sid: [] for sid in step_ids}
    for step in steps:
        for transition in step.get("transitions", []):
            adjacency[step["id"]].append(transition["target_step_id"])

    WHITE, GRAY, BLACK = 0, 1, 2
    color: dict[str, int] = {sid: WHITE for sid in step_ids}
    parent: dict[str, str | None] = {sid: None for sid in step_ids}
    errors: list[str] = []

    def _dfs(node: str) -> None:
        color[node] = GRAY
        for neighbor in adjacency[node]:
            if color[neighbor] == GRAY:
                # Reconstruct cycle path
                cycle = [neighbor, node]
                cursor = node
                while cursor != neighbor:
                    cursor = parent[cursor]  # type: ignore[assignment]
                    if cursor is None or cursor == neighbor:
                        break
                    cycle.append(cursor)
                cycle.reverse()
                errors.append(f"circular transition detected: {' -> '.join(cycle)}")
            elif color[neighbor] == WHITE:
                parent[neighbor] = node
                _dfs(neighbor)
        color[node] = BLACK

    for sid in step_ids:
        if color[sid] == WHITE:
            _dfs(sid)

    if errors:
        raise ValueError("; ".join(errors))


def dump_json(document: dict[str, Any], out_path: Path | None = None) -> None:
    payload = json.dumps(document, ensure_ascii=False, indent=2) + "\n"
    if out_path is None:
        print(payload, end="")
        return
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(payload, encoding="utf-8")


@dataclass
class SimulationResult:
    history: list[str]


def simulate_runtime3_document(document: dict[str, Any], max_steps: int = 16) -> SimulationResult:
    validate_runtime3_document(document)
    steps_by_id = {step["id"]: step for step in document["steps"]}
    current = document["scenario"]["entry_step_id"]
    history = [current]
    for _ in range(max_steps - 1):
        transitions = steps_by_id[current].get("transitions", [])
        if not transitions:
            break
        transitions = sorted(transitions, key=lambda item: item.get("priority", 0), reverse=True)
        current = transitions[0]["target_step_id"]
        history.append(current)
    return SimulationResult(history=history)
