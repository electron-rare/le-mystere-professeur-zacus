#!/usr/bin/env python3
"""Validate coherence of Zacus conversation bundle artifacts."""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

try:
    import yaml
except ImportError:  # pragma: no cover
    sys.exit("Missing dependency: install PyYAML (pip install pyyaml) to validate bundle YAML files.")


class ValidationError(Exception):
    pass


def read_json(path: Path) -> dict:
    try:
        return json.loads(path.read_text())
    except OSError as exc:
        raise ValidationError(f"Cannot read JSON file {path}: {exc}")
    except json.JSONDecodeError as exc:
        raise ValidationError(f"Invalid JSON in {path}: {exc}")


def read_yaml(path: Path) -> dict:
    try:
        return yaml.safe_load(path.read_text())
    except OSError as exc:
        raise ValidationError(f"Cannot read YAML file {path}: {exc}")
    except yaml.YAMLError as exc:
        raise ValidationError(f"Invalid YAML in {path}: {exc}")


def extract_template_scenario(template: dict) -> tuple[str, int]:
    prompt_input = template.get("prompt_input")
    if not isinstance(prompt_input, dict):
        raise ValidationError("template missing `prompt_input` mapping")

    scenario = prompt_input.get("scenario")
    if not isinstance(scenario, dict):
        raise ValidationError("template missing `prompt_input.scenario` mapping")

    scenario_id = scenario.get("id")
    version = scenario.get("version")
    if not isinstance(scenario_id, str) or not scenario_id:
        raise ValidationError("template `prompt_input.scenario.id` must be a non-empty string")
    if not isinstance(version, int):
        raise ValidationError("template `prompt_input.scenario.version` must be an integer")
    return scenario_id, version


def extract_canonical(canonical: dict) -> tuple[str, int, list[str], str]:
    scenario = canonical.get("scenario")
    if not isinstance(scenario, dict):
        raise ValidationError("canonical missing `scenario` mapping")

    scenario_id = scenario.get("id")
    version = scenario.get("version")
    initial_step = scenario.get("initial_step")
    steps = canonical.get("steps_runtime_order")

    if not isinstance(scenario_id, str) or not scenario_id:
        raise ValidationError("canonical `scenario.id` must be a non-empty string")
    if not isinstance(version, int):
        raise ValidationError("canonical `scenario.version` must be an integer")
    if not isinstance(initial_step, str) or not initial_step:
        raise ValidationError("canonical `scenario.initial_step` must be a non-empty string")
    if not isinstance(steps, list) or not steps:
        raise ValidationError("canonical `steps_runtime_order` must be a non-empty list")

    step_ids: list[str] = []
    for idx, step in enumerate(steps, start=1):
        if not isinstance(step, dict):
            raise ValidationError(f"canonical step #{idx} must be a mapping")
        step_id = step.get("step_id")
        if not isinstance(step_id, str) or not step_id:
            raise ValidationError(f"canonical step #{idx} missing non-empty `step_id`")
        step_ids.append(step_id)

    if initial_step not in step_ids:
        raise ValidationError("canonical initial_step not found in canonical steps_runtime_order")

    return scenario_id, version, step_ids, initial_step


def extract_runtime(runtime: dict) -> tuple[str, int, list[str], str]:
    scenario = runtime.get("scenario")
    if not isinstance(scenario, dict):
        raise ValidationError("runtime missing `scenario` mapping")

    scenario_id = scenario.get("id")
    version = scenario.get("version")
    initial_step = scenario.get("initial_step")
    steps = runtime.get("steps_runtime_order")

    if not isinstance(scenario_id, str) or not scenario_id:
        raise ValidationError("runtime `scenario.id` must be a non-empty string")
    if not isinstance(version, int):
        raise ValidationError("runtime `scenario.version` must be an integer")
    if not isinstance(initial_step, str) or not initial_step:
        raise ValidationError("runtime `scenario.initial_step` must be a non-empty string")
    if not isinstance(steps, list) or not steps:
        raise ValidationError("runtime `steps_runtime_order` must be a non-empty list")

    step_ids: list[str] = []
    for idx, step in enumerate(steps, start=1):
        if not isinstance(step, dict):
            raise ValidationError(f"runtime step #{idx} must be a mapping")
        step_id = step.get("step_id")
        if not isinstance(step_id, str) or not step_id:
            raise ValidationError(f"runtime step #{idx} missing non-empty `step_id`")
        step_ids.append(step_id)

    if initial_step not in step_ids:
        raise ValidationError("runtime initial_step not found in runtime steps_runtime_order")

    return scenario_id, version, step_ids, initial_step


def validate_bundle(runtime_path: Path, canonical_path: Path, template_path: Path) -> None:
    runtime = read_json(runtime_path)
    canonical = read_yaml(canonical_path)
    template = read_yaml(template_path)

    canonical_id, canonical_version, canonical_steps, canonical_initial = extract_canonical(canonical)
    runtime_id, runtime_version, runtime_steps, runtime_initial = extract_runtime(runtime)
    template_id, template_version = extract_template_scenario(template)

    errors: list[str] = []

    if canonical_id != runtime_id:
        errors.append(f"scenario id mismatch: canonical={canonical_id} runtime={runtime_id}")
    if canonical_version != runtime_version:
        errors.append(f"scenario version mismatch: canonical={canonical_version} runtime={runtime_version}")
    if canonical_initial != runtime_initial:
        errors.append(f"initial_step mismatch: canonical={canonical_initial} runtime={runtime_initial}")
    if canonical_steps != runtime_steps:
        errors.append("steps_runtime_order mismatch between canonical and runtime")

    if template_id != canonical_id:
        errors.append(f"template id mismatch: template={template_id} canonical={canonical_id}")
    if template_version != canonical_version:
        errors.append(f"template version mismatch: template={template_version} canonical={canonical_version}")

    if errors:
        raise ValidationError("; ".join(errors))

    print(
        "[runtime-bundle-validate] ok "
        f"id={canonical_id} version={canonical_version} steps={len(canonical_steps)} initial={canonical_initial}"
    )


def main() -> int:
    parser = argparse.ArgumentParser(description="Validate conversation bundle runtime/canonical/template coherence.")
    parser.add_argument(
        "--runtime",
        default="scenario-ai-coherence/zacus_conversation_bundle_v3/scenario_runtime.json",
        help="Path to scenario_runtime.json",
    )
    parser.add_argument(
        "--canonical",
        default="scenario-ai-coherence/zacus_conversation_bundle_v3/scenario_canonical.yaml",
        help="Path to scenario_canonical.yaml",
    )
    parser.add_argument(
        "--template",
        default="scenario-ai-coherence/zacus_conversation_bundle_v3/scenario_promptable_template.yaml",
        help="Path to scenario_promptable_template.yaml",
    )
    args = parser.parse_args()

    try:
        validate_bundle(Path(args.runtime), Path(args.canonical), Path(args.template))
    except ValidationError as exc:
        print(f"[runtime-bundle-validate] error -> {exc}")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
