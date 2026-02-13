#!/usr/bin/env python3
"""StorySpec YAML validator + C++ generator (PR1)."""

from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Any

ALLOWED_TRIGGER = {"on_event", "after_ms", "immediate"}
ALLOWED_EVENT = {"none", "unlock", "audio_done", "timer", "serial", "action"}
ALLOWED_APP = {"LA_DETECTOR", "AUDIO_PACK", "SCREEN_SCENE", "MP3_GATE"}

EVENT_CPP = {
    "none": "StoryEventType::kNone",
    "unlock": "StoryEventType::kUnlock",
    "audio_done": "StoryEventType::kAudioDone",
    "timer": "StoryEventType::kTimer",
    "serial": "StoryEventType::kSerial",
    "action": "StoryEventType::kAction",
}
TRIGGER_CPP = {
    "on_event": "StoryTransitionTrigger::kOnEvent",
    "after_ms": "StoryTransitionTrigger::kAfterMs",
    "immediate": "StoryTransitionTrigger::kImmediate",
}
APP_CPP = {
    "LA_DETECTOR": "StoryAppType::kLaDetector",
    "AUDIO_PACK": "StoryAppType::kAudioPack",
    "SCREEN_SCENE": "StoryAppType::kScreenScene",
    "MP3_GATE": "StoryAppType::kMp3Gate",
}


@dataclass
class ValidationIssue:
    file: str
    field: str
    reason: str

    def format(self) -> str:
        return f"{self.file}: line=n/a field={self.field}: {self.reason}"


def run_ruby_yaml_to_json(path: Path) -> Any:
    ruby_program = (
        "require 'yaml'; require 'json'; "
        "obj = YAML.load_file(ARGV[0]); "
        "puts JSON.generate(obj)"
    )
    try:
        result = subprocess.run(
            ["ruby", "-e", ruby_program, str(path)],
            check=False,
            capture_output=True,
            text=True,
        )
    except FileNotFoundError as exc:
        raise RuntimeError("ruby introuvable (requis pour parser YAML en PR1)") from exc

    if result.returncode != 0:
        stderr = (result.stderr or "").strip()
        raise RuntimeError(f"erreur parser YAML ruby: {stderr}")

    stdout = (result.stdout or "").strip()
    if not stdout:
        raise RuntimeError("parser YAML ruby a retourne une sortie vide")

    try:
        return json.loads(stdout)
    except json.JSONDecodeError as exc:
        raise RuntimeError(f"JSON invalide depuis parser YAML ruby: {exc}") from exc


def expect_type(obj: Any, t: type, file: str, field: str, issues: list[ValidationIssue]) -> bool:
    if isinstance(obj, t):
        return True
    issues.append(ValidationIssue(file=file, field=field, reason=f"type attendu {t.__name__}, recu {type(obj).__name__}"))
    return False


def expect_non_empty_string(obj: Any, file: str, field: str, issues: list[ValidationIssue]) -> str:
    if not isinstance(obj, str) or obj.strip() == "":
        issues.append(ValidationIssue(file=file, field=field, reason="chaine non vide requise"))
        return ""
    return obj.strip()


def normalize_scenario(path: Path) -> tuple[dict[str, Any] | None, list[ValidationIssue]]:
    issues: list[ValidationIssue] = []
    file = path.as_posix()
    try:
        doc = run_ruby_yaml_to_json(path)
    except RuntimeError as exc:
        return None, [ValidationIssue(file=file, field="root", reason=str(exc))]

    if not expect_type(doc, dict, file, "root", issues):
        return None, issues

    scenario_id = expect_non_empty_string(doc.get("id"), file, "id", issues)
    version = doc.get("version")
    if not isinstance(version, int):
        issues.append(ValidationIssue(file=file, field="version", reason="entier requis"))
        version = 0
    initial_step = expect_non_empty_string(doc.get("initial_step"), file, "initial_step", issues)

    app_bindings_raw = doc.get("app_bindings")
    if not expect_type(app_bindings_raw, list, file, "app_bindings", issues):
        app_bindings_raw = []

    app_bindings: list[dict[str, str]] = []
    app_ids_seen: set[str] = set()
    for idx, item in enumerate(app_bindings_raw):
        field_prefix = f"app_bindings[{idx}]"
        if not expect_type(item, dict, file, field_prefix, issues):
            continue
        app_id = expect_non_empty_string(item.get("id"), file, f"{field_prefix}.id", issues)
        app_name = expect_non_empty_string(item.get("app"), file, f"{field_prefix}.app", issues)
        if app_name and app_name not in ALLOWED_APP:
            issues.append(
                ValidationIssue(
                    file=file,
                    field=f"{field_prefix}.app",
                    reason=f"valeur invalide '{app_name}', attendu {sorted(ALLOWED_APP)}",
                )
            )
        if app_id:
            if app_id in app_ids_seen:
                issues.append(ValidationIssue(file=file, field=f"{field_prefix}.id", reason="duplicata"))
            app_ids_seen.add(app_id)
        app_bindings.append({"id": app_id, "app": app_name})

    steps_raw = doc.get("steps")
    if not expect_type(steps_raw, list, file, "steps", issues):
        steps_raw = []

    steps: list[dict[str, Any]] = []
    step_ids: set[str] = set()

    for sidx, raw_step in enumerate(steps_raw):
        step_prefix = f"steps[{sidx}]"
        if not expect_type(raw_step, dict, file, step_prefix, issues):
            continue

        step_id = expect_non_empty_string(raw_step.get("step_id"), file, f"{step_prefix}.step_id", issues)
        screen_scene_id = raw_step.get("screen_scene_id", "")
        if not isinstance(screen_scene_id, str):
            issues.append(ValidationIssue(file=file, field=f"{step_prefix}.screen_scene_id", reason="chaine requise"))
            screen_scene_id = ""
        audio_pack_id = raw_step.get("audio_pack_id", "")
        if not isinstance(audio_pack_id, str):
            issues.append(ValidationIssue(file=file, field=f"{step_prefix}.audio_pack_id", reason="chaine requise"))
            audio_pack_id = ""

        mp3_gate_open = raw_step.get("mp3_gate_open")
        if not isinstance(mp3_gate_open, bool):
            issues.append(ValidationIssue(file=file, field=f"{step_prefix}.mp3_gate_open", reason="bool requis"))
            mp3_gate_open = False

        if step_id:
            if step_id in step_ids:
                issues.append(ValidationIssue(file=file, field=f"{step_prefix}.step_id", reason="duplicata"))
            step_ids.add(step_id)

        actions_raw = raw_step.get("actions", [])
        if actions_raw is None:
            actions_raw = []
        if not isinstance(actions_raw, list):
            issues.append(ValidationIssue(file=file, field=f"{step_prefix}.actions", reason="liste requise"))
            actions_raw = []
        actions: list[str] = []
        for aidx, action in enumerate(actions_raw):
            action_id = expect_non_empty_string(action, file, f"{step_prefix}.actions[{aidx}]", issues)
            if action_id:
                actions.append(action_id)

        apps_raw = raw_step.get("apps")
        if not expect_type(apps_raw, list, file, f"{step_prefix}.apps", issues):
            apps_raw = []
        apps: list[str] = []
        for aidx, app in enumerate(apps_raw):
            app_id = expect_non_empty_string(app, file, f"{step_prefix}.apps[{aidx}]", issues)
            if app_id:
                if app_id not in app_ids_seen:
                    issues.append(
                        ValidationIssue(
                            file=file,
                            field=f"{step_prefix}.apps[{aidx}]",
                            reason=f"binding inconnu '{app_id}'",
                        )
                    )
                apps.append(app_id)

        transitions_raw = raw_step.get("transitions")
        if not expect_type(transitions_raw, list, file, f"{step_prefix}.transitions", issues):
            transitions_raw = []

        transitions: list[dict[str, Any]] = []
        for tidx, raw_tr in enumerate(transitions_raw):
            tr_prefix = f"{step_prefix}.transitions[{tidx}]"
            if not expect_type(raw_tr, dict, file, tr_prefix, issues):
                continue

            trigger = expect_non_empty_string(raw_tr.get("trigger"), file, f"{tr_prefix}.trigger", issues)
            if trigger and trigger not in ALLOWED_TRIGGER:
                issues.append(
                    ValidationIssue(
                        file=file,
                        field=f"{tr_prefix}.trigger",
                        reason=f"valeur invalide '{trigger}', attendu {sorted(ALLOWED_TRIGGER)}",
                    )
                )

            event_type = expect_non_empty_string(raw_tr.get("event_type"), file, f"{tr_prefix}.event_type", issues)
            if event_type and event_type not in ALLOWED_EVENT:
                issues.append(
                    ValidationIssue(
                        file=file,
                        field=f"{tr_prefix}.event_type",
                        reason=f"valeur invalide '{event_type}', attendu {sorted(ALLOWED_EVENT)}",
                    )
                )

            event_name_value = raw_tr.get("event_name", "")
            if event_name_value is None:
                event_name_value = ""
            if not isinstance(event_name_value, str):
                issues.append(ValidationIssue(file=file, field=f"{tr_prefix}.event_name", reason="chaine requise"))
                event_name_value = ""
            event_name = event_name_value.strip()

            target_step_id = expect_non_empty_string(raw_tr.get("target_step_id"), file, f"{tr_prefix}.target_step_id", issues)

            after_ms = raw_tr.get("after_ms", 0)
            if not isinstance(after_ms, int) or after_ms < 0:
                issues.append(ValidationIssue(file=file, field=f"{tr_prefix}.after_ms", reason="entier >= 0 requis"))
                after_ms = 0

            priority = raw_tr.get("priority", 0)
            if not isinstance(priority, int) or priority < 0 or priority > 255:
                issues.append(ValidationIssue(file=file, field=f"{tr_prefix}.priority", reason="entier 0..255 requis"))
                priority = 0

            if trigger == "on_event" and event_type == "none":
                issues.append(
                    ValidationIssue(file=file, field=f"{tr_prefix}.event_type", reason="on_event requiert event_type != none")
                )

            tr_id = raw_tr.get("id")
            if not isinstance(tr_id, str) or tr_id.strip() == "":
                tr_id = f"TR_{step_id}_{tidx + 1}"

            transitions.append(
                {
                    "id": str(tr_id),
                    "trigger": trigger,
                    "event_type": event_type,
                    "event_name": event_name,
                    "target_step_id": target_step_id,
                    "after_ms": after_ms,
                    "priority": priority,
                }
            )

        steps.append(
            {
                "step_id": step_id,
                "screen_scene_id": screen_scene_id,
                "audio_pack_id": audio_pack_id,
                "actions": actions,
                "apps": apps,
                "mp3_gate_open": mp3_gate_open,
                "transitions": transitions,
            }
        )

    if initial_step and initial_step not in step_ids:
        issues.append(ValidationIssue(file=file, field="initial_step", reason=f"step inconnu '{initial_step}'"))

    for step in steps:
        for idx, tr in enumerate(step["transitions"]):
            if tr["target_step_id"] not in step_ids:
                issues.append(
                    ValidationIssue(
                        file=file,
                        field=f"steps[{step['step_id']}].transitions[{idx}].target_step_id",
                        reason=f"step cible inconnu '{tr['target_step_id']}'",
                    )
                )

    scenario = {
        "id": scenario_id,
        "version": version,
        "initial_step": initial_step,
        "app_bindings": app_bindings,
        "steps": steps,
        "source": file,
    }
    return (scenario if not issues else None), issues


def sanitize_ident(text: str) -> str:
    base = re.sub(r"[^A-Za-z0-9_]", "_", text)
    if not base:
        base = "ID"
    if base[0].isdigit():
        base = f"_{base}"
    return base


def cstr(value: str | None) -> str:
    if value is None or value == "":
        return "nullptr"
    escaped = value.replace("\\", "\\\\").replace('"', '\\"')
    return f'"{escaped}"'


def generate_scenarios_h(scenarios: list[dict[str, Any]]) -> str:
    return """#pragma once

#include <Arduino.h>

#include "../core/scenario_def.h"

const ScenarioDef* generatedScenarioById(const char* id);
const ScenarioDef* generatedScenarioDefault();
uint8_t generatedScenarioCount();
const char* generatedScenarioIdAt(uint8_t index);
"""


def generate_apps_h() -> str:
    return """#pragma once

#include "../core/scenario_def.h"

const AppBindingDef* generatedAppBindingById(const char* id);
uint8_t generatedAppBindingCount();
const char* generatedAppBindingIdAt(uint8_t index);
"""


def emit_step_arrays(lines: list[str], scenario_idx: int, steps: list[dict[str, Any]]) -> None:
    for step_idx, step in enumerate(steps):
        prefix = f"kSc{scenario_idx}St{step_idx}"

        actions = step["actions"]
        if actions:
            lines.append(f"constexpr const char* {prefix}Actions[] = {{")
            for action in actions:
                lines.append(f"    {cstr(action)},")
            lines.append("};")
        apps = step["apps"]
        if apps:
            lines.append(f"constexpr const char* {prefix}Apps[] = {{")
            for app_id in apps:
                lines.append(f"    {cstr(app_id)},")
            lines.append("};")

        transitions = step["transitions"]
        if transitions:
            lines.append(f"constexpr TransitionDef {prefix}Transitions[] = {{")
            for tr in transitions:
                trigger_cpp = TRIGGER_CPP.get(tr["trigger"], "StoryTransitionTrigger::kOnEvent")
                event_cpp = EVENT_CPP.get(tr["event_type"], "StoryEventType::kNone")
                lines.append(
                    "    {"
                    + ", ".join(
                        [
                            cstr(tr["id"]),
                            trigger_cpp,
                            event_cpp,
                            cstr(tr["event_name"]),
                            f"{int(tr['after_ms'])}U",
                            cstr(tr["target_step_id"]),
                            f"{int(tr['priority'])}U",
                        ]
                    )
                    + "},"
                )
            lines.append("};")


def generate_scenarios_cpp(scenarios: list[dict[str, Any]]) -> str:
    lines: list[str] = []
    lines.append('#include "scenarios_gen.h"')
    lines.append("")
    lines.append("#include <cstring>")
    lines.append("")
    lines.append("namespace {")

    for scenario_idx, scenario in enumerate(scenarios):
        emit_step_arrays(lines, scenario_idx, scenario["steps"])
        lines.append("")
        lines.append(f"constexpr StepDef kScenario{scenario_idx}Steps[] = {{")
        for step_idx, step in enumerate(scenario["steps"]):
            prefix = f"kSc{scenario_idx}St{step_idx}"
            actions_ptr = f"{prefix}Actions" if step["actions"] else "nullptr"
            actions_count = len(step["actions"])
            apps_ptr = f"{prefix}Apps" if step["apps"] else "nullptr"
            apps_count = len(step["apps"])
            transitions_ptr = f"{prefix}Transitions" if step["transitions"] else "nullptr"
            transitions_count = len(step["transitions"])
            lines.append(
                "    {"
                + ", ".join(
                    [
                        cstr(step["step_id"]),
                        "{" + ", ".join(
                            [
                                cstr(step["screen_scene_id"]),
                                cstr(step["audio_pack_id"]),
                                actions_ptr,
                                f"{actions_count}U",
                                apps_ptr,
                                f"{apps_count}U",
                            ]
                        ) + "}",
                        transitions_ptr,
                        f"{transitions_count}U",
                        "true" if step["mp3_gate_open"] else "false",
                    ]
                )
                + "},"
            )
        lines.append("};")
        lines.append("")
        lines.append(f"constexpr ScenarioDef kScenario{scenario_idx} = {{")
        lines.append(f"    {cstr(scenario['id'])},")
        lines.append(f"    {int(scenario['version'])}U,")
        lines.append(f"    kScenario{scenario_idx}Steps,")
        lines.append(f"    {len(scenario['steps'])}U,")
        lines.append(f"    {cstr(scenario['initial_step'])},")
        lines.append("};")
        lines.append("")

    lines.append("constexpr const ScenarioDef* kGeneratedScenarios[] = {")
    for idx in range(len(scenarios)):
        lines.append(f"    &kScenario{idx},")
    lines.append("};")
    lines.append("")
    lines.append("}  // namespace")
    lines.append("")
    lines.append("const ScenarioDef* generatedScenarioById(const char* id) {")
    lines.append("  if (id == nullptr || id[0] == '\\0') {")
    lines.append("    return generatedScenarioDefault();")
    lines.append("  }")
    lines.append("  for (const ScenarioDef* scenario : kGeneratedScenarios) {")
    lines.append("    if (scenario != nullptr && scenario->id != nullptr && strcmp(scenario->id, id) == 0) {")
    lines.append("      return scenario;")
    lines.append("    }")
    lines.append("  }")
    lines.append("  return nullptr;")
    lines.append("}")
    lines.append("")
    lines.append("const ScenarioDef* generatedScenarioDefault() {")
    lines.append("  if (generatedScenarioCount() == 0U) {")
    lines.append("    return nullptr;")
    lines.append("  }")
    lines.append("  return kGeneratedScenarios[0];")
    lines.append("}")
    lines.append("")
    lines.append("uint8_t generatedScenarioCount() {")
    lines.append("  return static_cast<uint8_t>(sizeof(kGeneratedScenarios) / sizeof(kGeneratedScenarios[0]));")
    lines.append("}")
    lines.append("")
    lines.append("const char* generatedScenarioIdAt(uint8_t index) {")
    lines.append("  if (index >= generatedScenarioCount()) {")
    lines.append("    return nullptr;")
    lines.append("  }")
    lines.append("  const ScenarioDef* scenario = kGeneratedScenarios[index];")
    lines.append("  return (scenario != nullptr) ? scenario->id : nullptr;")
    lines.append("}")
    lines.append("")
    return "\n".join(lines)


def generate_apps_cpp(scenarios: list[dict[str, Any]]) -> str:
    binding_map: dict[str, str] = {}
    for scenario in scenarios:
        for binding in scenario["app_bindings"]:
            bid = binding["id"]
            app = binding["app"]
            if bid in binding_map and binding_map[bid] != app:
                raise RuntimeError(f"binding '{bid}' declare avec 2 apps differents: {binding_map[bid]} vs {app}")
            binding_map[bid] = app

    sorted_bindings = sorted(binding_map.items(), key=lambda x: x[0])

    lines: list[str] = []
    lines.append('#include "apps_gen.h"')
    lines.append("")
    lines.append("#include <cstring>")
    lines.append("")
    lines.append("namespace {")
    lines.append("constexpr AppBindingDef kGeneratedAppBindings[] = {")
    for bid, app in sorted_bindings:
        app_cpp = APP_CPP.get(app, "StoryAppType::kNone")
        lines.append(f"    {{{cstr(bid)}, {app_cpp}}},")
    lines.append("};")
    lines.append("}  // namespace")
    lines.append("")
    lines.append("const AppBindingDef* generatedAppBindingById(const char* id) {")
    lines.append("  if (id == nullptr || id[0] == '\\0') {")
    lines.append("    return nullptr;")
    lines.append("  }")
    lines.append("  for (const AppBindingDef& binding : kGeneratedAppBindings) {")
    lines.append("    if (binding.id != nullptr && strcmp(binding.id, id) == 0) {")
    lines.append("      return &binding;")
    lines.append("    }")
    lines.append("  }")
    lines.append("  return nullptr;")
    lines.append("}")
    lines.append("")
    lines.append("uint8_t generatedAppBindingCount() {")
    lines.append("  return static_cast<uint8_t>(sizeof(kGeneratedAppBindings) / sizeof(kGeneratedAppBindings[0]));")
    lines.append("}")
    lines.append("")
    lines.append("const char* generatedAppBindingIdAt(uint8_t index) {")
    lines.append("  if (index >= generatedAppBindingCount()) {")
    lines.append("    return nullptr;")
    lines.append("  }")
    lines.append("  return kGeneratedAppBindings[index].id;")
    lines.append("}")
    lines.append("")
    return "\n".join(lines)


def write_file(path: Path, content: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content + "\n", encoding="utf-8")


def collect_scenarios(spec_dir: Path) -> tuple[list[dict[str, Any]], list[ValidationIssue]]:
    scenarios: list[dict[str, Any]] = []
    issues: list[ValidationIssue] = []

    files = sorted(spec_dir.glob("*.yaml"))
    if not files:
        issues.append(ValidationIssue(file=spec_dir.as_posix(), field="*.yaml", reason="aucun scenario trouve"))
        return [], issues

    scenario_ids: set[str] = set()
    for path in files:
        scenario, scenario_issues = normalize_scenario(path)
        if scenario_issues:
            issues.extend(scenario_issues)
            continue
        assert scenario is not None
        if scenario["id"] in scenario_ids:
            issues.append(ValidationIssue(file=path.as_posix(), field="id", reason=f"duplicata global '{scenario['id']}'"))
            continue
        scenario_ids.add(scenario["id"])
        scenarios.append(scenario)

    return scenarios, issues


def cmd_validate(args: argparse.Namespace) -> int:
    spec_dir = Path(args.spec_dir)
    scenarios, issues = collect_scenarios(spec_dir)
    if issues:
        for issue in issues:
            print(f"[story-validate] ERR {issue.format()}")
        return 1
    for scenario in scenarios:
        print(
            f"[story-validate] OK file={scenario['source']} id={scenario['id']} "
            f"steps={len(scenario['steps'])} bindings={len(scenario['app_bindings'])}"
        )
    print(f"[story-validate] OK total={len(scenarios)}")
    return 0


def cmd_generate(args: argparse.Namespace) -> int:
    spec_dir = Path(args.spec_dir)
    out_dir = Path(args.out_dir)

    scenarios, issues = collect_scenarios(spec_dir)
    if issues:
        for issue in issues:
            print(f"[story-gen] ERR {issue.format()}")
        return 1

    scenarios.sort(key=lambda sc: sc["id"])

    scenarios_h = generate_scenarios_h(scenarios)
    scenarios_cpp = generate_scenarios_cpp(scenarios)
    apps_h = generate_apps_h()
    apps_cpp = generate_apps_cpp(scenarios)

    write_file(out_dir / "scenarios_gen.h", scenarios_h)
    write_file(out_dir / "scenarios_gen.cpp", scenarios_cpp)
    write_file(out_dir / "apps_gen.h", apps_h)
    write_file(out_dir / "apps_gen.cpp", apps_cpp)

    print(f"[story-gen] OK scenarios={len(scenarios)} out={out_dir.as_posix()}")
    return 0


def build_arg_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="StorySpec YAML validator/generator")
    sub = parser.add_subparsers(dest="command", required=True)

    validate_p = sub.add_parser("validate", help="validate YAML scenarios")
    validate_p.add_argument("--spec-dir", default="story_specs/scenarios")
    validate_p.set_defaults(func=cmd_validate)

    generate_p = sub.add_parser("generate", help="generate C++ from YAML scenarios")
    generate_p.add_argument("--spec-dir", default="story_specs/scenarios")
    generate_p.add_argument("--out-dir", default="src/story/generated")
    generate_p.set_defaults(func=cmd_generate)

    return parser


def main(argv: list[str]) -> int:
    parser = build_arg_parser()
    args = parser.parse_args(argv)
    return args.func(args)


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
