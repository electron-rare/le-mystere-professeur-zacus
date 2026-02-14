#!/usr/bin/env python3
"""Export a human-readable scenario summary."""
from __future__ import annotations

import argparse
from pathlib import Path
import sys

try:
    import yaml
except ImportError as exc:
    print("Missing dependency: install PyYAML", exc)
    sys.exit(1)


def format_transitions(transitions: list[dict[str, str]]) -> list[str]:
    lines: list[str] = []
    for transition in transitions:
        trigger = transition.get("trigger")
        event = transition.get("event_type")
        target = transition.get("target_step_id")
        desc = f"{trigger} → {event} → {target}"
        lines.append(desc)
    return lines


def load_yaml(path: Path) -> dict:
    if not path.exists():
        raise FileNotFoundError(path)
    data = yaml.safe_load(path.read_text())
    if not isinstance(data, dict):
        raise ValueError("scenario must be a mapping")
    return data


def main() -> int:
    parser = argparse.ArgumentParser(description="Render scenario summary to Markdown")
    parser.add_argument("scenario", type=Path, help="scenario file path")
    parser.add_argument("--output", type=Path, default=Path("docs/scenario-zacus_v1.md"))
    args = parser.parse_args()

    scenario = load_yaml(args.scenario)
    scenario_id = scenario.get("id", "unknown")
    version = scenario.get("version", "n/a")
    initial = scenario.get("initial_step", "n/a")

    steps = scenario.get("steps") or []
    if not isinstance(steps, list):
        raise ValueError("steps must be a list")

    lines: list[str] = []
    lines.append(f"# Scenario {scenario_id}")
    lines.append("")
    lines.append(f"- Version: {version}")
    lines.append(f"- Initial step: {initial}")
    lines.append("")
    lines.append("## Steps")

    for step in steps:
        if not isinstance(step, dict):
            continue
        step_id = step.get("step_id", "unknown")
        scene = step.get("screen_scene_id", "?")
        audio_pack = step.get("audio_pack_id", "none")
        apps = step.get("apps") or []
        transitions = step.get("transitions") or []
        app_list = ", ".join(str(app) for app in apps)
        lines.append(f"### {step_id}")
        lines.append(f"- screen scene: {scene}")
        lines.append(f"- audio pack: {audio_pack or 'none'}")
        lines.append(f"- apps: {app_list or 'none'}")
        if transitions:
            lines.append("- transitions:")
            for line in format_transitions(transitions):
                lines.append(f"  - {line}")
        lines.append("")

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text("\n".join(lines).rstrip() + "\n")
    print(f"[scenario-export] wrote {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
