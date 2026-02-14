#!/usr/bin/env python3
"""Minimal validator for Zacus scenario YAML."""
from __future__ import annotations

import argparse
import sys
from pathlib import Path
from typing import Any

try:
    import yaml
except ImportError as exc:
    print("Missing dependency: install PyYAML", exc)
    sys.exit(1)

ALLOWED_APPS = {"LA_DETECTOR", "AUDIO_PACK", "SCREEN_SCENE", "MP3_GATE"}
ALLOWED_TRIGGERS = {"on_event", "after_ms", "immediate"}
ALLOWED_EVENTS = {"unlock", "audio_done", "serial", "timer", "none"}


def load_yaml(path: Path) -> Any:
    if not path.exists():
        raise FileNotFoundError(path)
    return yaml.safe_load(path.read_text())


def collect_pack_ids(manifest_path: Path) -> set[str]:
    manifest = load_yaml(manifest_path)
    packs = manifest.get("packs") or []
    if not isinstance(packs, list):
        raise ValueError("audio manifest packs must be a list")
    ids: set[str] = set()
    for entry in packs:
        entry_id = entry.get("id")
        if isinstance(entry_id, str):
            ids.add(entry_id)
    return ids


def report_issue(message: str) -> None:
    print("[scenario-validate]", message)


def main() -> int:
    parser = argparse.ArgumentParser(description="Validate scenario YAML for Zacus")
    parser.add_argument("scenario", type=Path, help="path to scenario YAML")
    parser.add_argument("--audio-manifest", type=Path, default=Path("audio/manifests/zacus_v1_audio.yaml"))
    args = parser.parse_args()

    try:
        scenario = load_yaml(args.scenario)
    except Exception as exc:  # pragma: no cover
        print(f"Failed to load scenario: {exc}")
        return 1

    issues = 0
    if not isinstance(scenario, dict):
        report_issue("top-level YAML must be a mapping")
        return 1

    scenario_id = scenario.get("id")
    if not scenario_id or not isinstance(scenario_id, str):
        report_issue("missing or invalid id")
        issues += 1

    version = scenario.get("version")
    if not isinstance(version, int) or version <= 0:
        report_issue("version must be a positive integer")
        issues += 1

    initial = scenario.get("initial_step")
    if not isinstance(initial, str) or not initial:
        report_issue("initial_step must be a non-empty string")
        issues += 1

    steps = scenario.get("steps")
    if not isinstance(steps, list) or not steps:
        report_issue("steps must be a non-empty list")
        issues += 1

    app_bindings = scenario.get("app_bindings") or []
    if not isinstance(app_bindings, list) or not app_bindings:
        report_issue("app_bindings must be a list of bindings")
        issues += 1

    bindings_by_id: dict[str, str] = {}
    for binding in app_bindings:
        if not isinstance(binding, dict):
            continue
        binding_id = binding.get("id")
        binding_app = binding.get("app")
        if not binding_id or not isinstance(binding_id, str):
            report_issue("binding id must be a string")
            issues += 1
            continue
        if not isinstance(binding_app, str) or binding_app not in ALLOWED_APPS:
            report_issue(f"binding {binding_id} uses unsupported app {binding_app}")
            issues += 1
        bindings_by_id[binding_id] = binding_app

    if initial and initial not in {step.get("step_id") for step in steps if isinstance(step, dict)}:
        report_issue(f"initial_step {initial} not defined in steps")
        issues += 1

    pack_ids = set()
    try:
        pack_ids = collect_pack_ids(args.audio_manifest)
    except Exception as exc:
        report_issue(f"audio manifest error: {exc}")
        issues += 1

    seen_steps: set[str] = set()
    for step in steps:
        if not isinstance(step, dict):
            report_issue("each step must be a mapping")
            issues += 1
            continue
        step_id = step.get("step_id")
        if not step_id or not isinstance(step_id, str):
            report_issue("step missing step_id")
            issues += 1
            continue
        if step_id in seen_steps:
            report_issue(f"duplicate step_id {step_id}")
            issues += 1
            continue
        seen_steps.add(step_id)

        apps = step.get("apps") or []
        if not isinstance(apps, list) or not apps:
            report_issue(f"step {step_id} apps must be a non-empty list")
            issues += 1
        else:
            for app in apps:
                if app not in bindings_by_id:
                    report_issue(f"step {step_id} references unknown app {app}")
                    issues += 1

        transitions = step.get("transitions") or []
        if not isinstance(transitions, list):
            report_issue(f"step {step_id} transitions must be a list")
            issues += 1
            continue
        for transition in transitions:
            if not isinstance(transition, dict):
                report_issue(f"step {step_id} has invalid transition")
                issues += 1
                continue
            trigger = transition.get("trigger")
            if trigger not in ALLOWED_TRIGGERS:
                report_issue(f"step {step_id} transition trigger {trigger} unsupported")
                issues += 1
            event_type = transition.get("event_type")
            if event_type not in ALLOWED_EVENTS:
                report_issue(f"step {step_id} transition event {event_type} unsupported")
                issues += 1
            target = transition.get("target_step_id")
            if not target or not isinstance(target, str):
                report_issue(f"step {step_id} transition missing target_step_id")
                issues += 1

        audio_pack = step.get("audio_pack_id")
        if audio_pack and isinstance(audio_pack, str) and audio_pack not in pack_ids:
            report_issue(f"step {step_id} references unknown audio_pack_id {audio_pack}")
            issues += 1

    if issues:
        return 1
    print(f"[scenario-validate] {scenario_id or 'scenario'} OK ({len(seen_steps)} steps)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
