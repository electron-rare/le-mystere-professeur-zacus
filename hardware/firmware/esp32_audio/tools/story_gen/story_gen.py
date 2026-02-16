#!/usr/bin/env python3
"""StorySpec YAML validator + C++ generator (strict/idempotent)."""

from __future__ import annotations

import argparse
import hashlib
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

ROOT_FIELDS = {"id", "version", "initial_step", "app_bindings", "steps"}
APP_BINDING_FIELDS = {"id", "app", "config"}
LA_APP_CONFIG_FIELDS = {"hold_ms", "unlock_event", "require_listening"}
STEP_FIELDS = {
    "step_id",
    "screen_scene_id",
    "audio_pack_id",
    "actions",
    "apps",
    "mp3_gate_open",
    "transitions",
}
TRANSITION_FIELDS = {"id", "trigger", "event_type", "event_name", "target_step_id", "after_ms", "priority"}

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
    code: str

    def format(self) -> str:
        return f"{self.file}: line=n/a code={self.code} field={self.field}: {self.reason}"


def add_issue(issues: list[ValidationIssue], file: str, field: str, reason: str, code: str) -> None:
    issues.append(ValidationIssue(file=file, field=field, reason=reason, code=code))


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
        raise RuntimeError("ruby introuvable (requis pour parser YAML)") from exc

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


def expect_type(
    obj: Any,
    expected_type: type,
    file: str,
    field: str,
    issues: list[ValidationIssue],
    code: str,
) -> bool:
    if isinstance(obj, expected_type):
        return True
    add_issue(
        issues,
        file,
        field,
        f"type attendu {expected_type.__name__}, recu {type(obj).__name__}",
        code,
    )
    return False


def expect_non_empty_string(
    obj: Any,
    file: str,
    field: str,
    issues: list[ValidationIssue],
    code: str,
) -> str:
    if not isinstance(obj, str) or obj.strip() == "":
        add_issue(issues, file, field, "chaine non vide requise", code)
        return ""
    return obj.strip()


def check_unknown_keys(
    raw: dict[str, Any],
    allowed: set[str],
    file: str,
    field_prefix: str,
    issues: list[ValidationIssue],
    strict: bool,
    code_prefix: str,
) -> None:
    if not strict:
        return
    extra = sorted(set(raw.keys()) - allowed)
    for key in extra:
        add_issue(
            issues,
            file,
            f"{field_prefix}.{key}" if field_prefix else key,
            f"champ inconnu '{key}'",
            f"{code_prefix}_UNKNOWN_FIELD",
        )


def normalize_scenario(path: Path, strict: bool) -> tuple[dict[str, Any] | None, list[ValidationIssue]]:
    issues: list[ValidationIssue] = []
    file = path.as_posix()

    try:
        doc = run_ruby_yaml_to_json(path)
    except RuntimeError as exc:
        return None, [ValidationIssue(file=file, field="root", reason=str(exc), code="YAML_PARSE_ERROR")]

    if not expect_type(doc, dict, file, "root", issues, "ROOT_TYPE"):
        return None, issues

    check_unknown_keys(doc, ROOT_FIELDS, file, "", issues, strict, "ROOT")

    scenario_id = expect_non_empty_string(doc.get("id"), file, "id", issues, "SCENARIO_ID")

    version_raw = doc.get("version")
    version = 0
    if isinstance(version_raw, int):
        version = version_raw
    else:
        add_issue(issues, file, "version", "entier requis", "SCENARIO_VERSION")

    initial_step = expect_non_empty_string(doc.get("initial_step"), file, "initial_step", issues, "SCENARIO_INITIAL")

    app_bindings_raw = doc.get("app_bindings")
    if not expect_type(app_bindings_raw, list, file, "app_bindings", issues, "APP_BINDINGS_TYPE"):
        app_bindings_raw = []

    app_bindings: list[dict[str, Any]] = []
    app_ids_seen: set[str] = set()
    for idx, item in enumerate(app_bindings_raw):
        field_prefix = f"app_bindings[{idx}]"
        if not expect_type(item, dict, file, field_prefix, issues, "APP_BINDING_TYPE"):
            continue
        check_unknown_keys(item, APP_BINDING_FIELDS, file, field_prefix, issues, strict, "APP_BINDING")

        app_id = expect_non_empty_string(item.get("id"), file, f"{field_prefix}.id", issues, "APP_BINDING_ID")
        app_name = expect_non_empty_string(item.get("app"), file, f"{field_prefix}.app", issues, "APP_BINDING_APP")

        if app_name and app_name not in ALLOWED_APP:
            add_issue(
                issues,
                file,
                f"{field_prefix}.app",
                f"valeur invalide '{app_name}', attendu {sorted(ALLOWED_APP)}",
                "APP_BINDING_APP_INVALID",
            )

        config_obj: dict[str, Any] | None = None
        config_raw = item.get("config")
        if config_raw is not None:
            if not isinstance(config_raw, dict):
                add_issue(issues, file, f"{field_prefix}.config", "objet requis", "APP_BINDING_CONFIG_TYPE")
            elif app_name != "LA_DETECTOR":
                add_issue(
                    issues,
                    file,
                    f"{field_prefix}.config",
                    "config supporte uniquement pour LA_DETECTOR",
                    "APP_BINDING_CONFIG_UNSUPPORTED",
                )
            else:
                check_unknown_keys(
                    config_raw,
                    LA_APP_CONFIG_FIELDS,
                    file,
                    f"{field_prefix}.config",
                    issues,
                    strict,
                    "APP_BINDING_CONFIG",
                )
                hold_ms = config_raw.get("hold_ms", 3000)
                if not isinstance(hold_ms, int) or hold_ms < 100 or hold_ms > 60000:
                    add_issue(
                        issues,
                        file,
                        f"{field_prefix}.config.hold_ms",
                        "entier 100..60000 requis",
                        "APP_BINDING_CONFIG_HOLD_MS",
                    )
                    hold_ms = 3000

                unlock_event = config_raw.get("unlock_event", "UNLOCK")
                if not isinstance(unlock_event, str) or unlock_event.strip() == "":
                    add_issue(
                        issues,
                        file,
                        f"{field_prefix}.config.unlock_event",
                        "chaine non vide requise",
                        "APP_BINDING_CONFIG_UNLOCK_EVENT",
                    )
                    unlock_event = "UNLOCK"
                unlock_event = unlock_event.strip()

                require_listening = config_raw.get("require_listening", True)
                if not isinstance(require_listening, bool):
                    add_issue(
                        issues,
                        file,
                        f"{field_prefix}.config.require_listening",
                        "bool requis",
                        "APP_BINDING_CONFIG_REQUIRE_LISTENING",
                    )
                    require_listening = True

                config_obj = {
                    "hold_ms": int(hold_ms),
                    "unlock_event": unlock_event,
                    "require_listening": require_listening,
                }

        if app_name == "LA_DETECTOR" and config_obj is None:
            config_obj = {
                "hold_ms": 3000,
                "unlock_event": "UNLOCK",
                "require_listening": True,
            }

        if app_id:
            if app_id in app_ids_seen:
                add_issue(issues, file, f"{field_prefix}.id", "duplicata", "APP_BINDING_ID_DUP")
            app_ids_seen.add(app_id)

        app_bindings.append({"id": app_id, "app": app_name, "config": config_obj})

    steps_raw = doc.get("steps")
    if not expect_type(steps_raw, list, file, "steps", issues, "STEPS_TYPE"):
        steps_raw = []

    steps: list[dict[str, Any]] = []
    step_ids: set[str] = set()

    for sidx, raw_step in enumerate(steps_raw):
        step_prefix = f"steps[{sidx}]"
        if not expect_type(raw_step, dict, file, step_prefix, issues, "STEP_TYPE"):
            continue
        check_unknown_keys(raw_step, STEP_FIELDS, file, step_prefix, issues, strict, "STEP")

        step_id = expect_non_empty_string(raw_step.get("step_id"), file, f"{step_prefix}.step_id", issues, "STEP_ID")

        screen_scene_id = raw_step.get("screen_scene_id", "")
        if not isinstance(screen_scene_id, str):
            add_issue(issues, file, f"{step_prefix}.screen_scene_id", "chaine requise", "STEP_SCREEN_SCENE")
            screen_scene_id = ""

        audio_pack_id = raw_step.get("audio_pack_id", "")
        if not isinstance(audio_pack_id, str):
            add_issue(issues, file, f"{step_prefix}.audio_pack_id", "chaine requise", "STEP_AUDIO_PACK")
            audio_pack_id = ""

        mp3_gate_open = raw_step.get("mp3_gate_open")
        if not isinstance(mp3_gate_open, bool):
            add_issue(issues, file, f"{step_prefix}.mp3_gate_open", "bool requis", "STEP_GATE")
            mp3_gate_open = False

        if step_id:
            if step_id in step_ids:
                add_issue(issues, file, f"{step_prefix}.step_id", "duplicata", "STEP_ID_DUP")
            step_ids.add(step_id)

        actions_raw = raw_step.get("actions", [])
        if actions_raw is None:
            actions_raw = []
        if not isinstance(actions_raw, list):
            add_issue(issues, file, f"{step_prefix}.actions", "liste requise", "STEP_ACTIONS_TYPE")
            actions_raw = []
        actions: list[str] = []
        for aidx, action in enumerate(actions_raw):
            action_id = expect_non_empty_string(action, file, f"{step_prefix}.actions[{aidx}]", issues, "STEP_ACTION_ID")
            if action_id:
                actions.append(action_id)

        apps_raw = raw_step.get("apps")
        if not expect_type(apps_raw, list, file, f"{step_prefix}.apps", issues, "STEP_APPS_TYPE"):
            apps_raw = []
        apps: list[str] = []
        for aidx, app in enumerate(apps_raw):
            app_id = expect_non_empty_string(app, file, f"{step_prefix}.apps[{aidx}]", issues, "STEP_APP_ID")
            if app_id:
                if app_id not in app_ids_seen:
                    add_issue(
                        issues,
                        file,
                        f"{step_prefix}.apps[{aidx}]",
                        f"binding inconnu '{app_id}'",
                        "STEP_APP_BINDING_UNKNOWN",
                    )
                apps.append(app_id)

        transitions_raw = raw_step.get("transitions")
        if not expect_type(transitions_raw, list, file, f"{step_prefix}.transitions", issues, "STEP_TRANSITIONS_TYPE"):
            transitions_raw = []

        transitions: list[dict[str, Any]] = []
        for tidx, raw_tr in enumerate(transitions_raw):
            tr_prefix = f"{step_prefix}.transitions[{tidx}]"
            if not expect_type(raw_tr, dict, file, tr_prefix, issues, "TRANSITION_TYPE"):
                continue
            check_unknown_keys(raw_tr, TRANSITION_FIELDS, file, tr_prefix, issues, strict, "TRANSITION")

            trigger = expect_non_empty_string(raw_tr.get("trigger"), file, f"{tr_prefix}.trigger", issues, "TRANSITION_TRIGGER")
            if trigger and trigger not in ALLOWED_TRIGGER:
                add_issue(
                    issues,
                    file,
                    f"{tr_prefix}.trigger",
                    f"valeur invalide '{trigger}', attendu {sorted(ALLOWED_TRIGGER)}",
                    "TRANSITION_TRIGGER_INVALID",
                )

            event_type = expect_non_empty_string(raw_tr.get("event_type"), file, f"{tr_prefix}.event_type", issues, "TRANSITION_EVENT_TYPE")
            if event_type and event_type not in ALLOWED_EVENT:
                add_issue(
                    issues,
                    file,
                    f"{tr_prefix}.event_type",
                    f"valeur invalide '{event_type}', attendu {sorted(ALLOWED_EVENT)}",
                    "TRANSITION_EVENT_TYPE_INVALID",
                )

            event_name_raw = raw_tr.get("event_name", "")
            if event_name_raw is None:
                event_name_raw = ""
            if not isinstance(event_name_raw, str):
                add_issue(issues, file, f"{tr_prefix}.event_name", "chaine requise", "TRANSITION_EVENT_NAME")
                event_name_raw = ""
            event_name = event_name_raw.strip()

            target_step_id = expect_non_empty_string(
                raw_tr.get("target_step_id"),
                file,
                f"{tr_prefix}.target_step_id",
                issues,
                "TRANSITION_TARGET",
            )

            after_ms = raw_tr.get("after_ms", 0)
            if not isinstance(after_ms, int) or after_ms < 0:
                add_issue(issues, file, f"{tr_prefix}.after_ms", "entier >= 0 requis", "TRANSITION_AFTER")
                after_ms = 0

            priority = raw_tr.get("priority", 0)
            if not isinstance(priority, int) or priority < 0 or priority > 255:
                add_issue(issues, file, f"{tr_prefix}.priority", "entier 0..255 requis", "TRANSITION_PRIORITY")
                priority = 0

            if trigger == "on_event" and event_type == "none":
                add_issue(
                    issues,
                    file,
                    f"{tr_prefix}.event_type",
                    "on_event requiert event_type != none",
                    "TRANSITION_EVENT_TYPE_NONE",
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
        add_issue(issues, file, "initial_step", f"step inconnu '{initial_step}'", "SCENARIO_INITIAL_UNKNOWN")

    for step in steps:
        for tidx, tr in enumerate(step["transitions"]):
            if tr["target_step_id"] not in step_ids:
                add_issue(
                    issues,
                    file,
                    f"steps[{step['step_id']}].transitions[{tidx}].target_step_id",
                    f"step cible inconnu '{tr['target_step_id']}'",
                    "TRANSITION_TARGET_UNKNOWN",
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


def cstr(value: str | None) -> str:
    if value is None or value == "":
        return "nullptr"
    escaped = value.replace("\\", "\\\\").replace('"', '\\"')
    return f'"{escaped}"'


def scenario_spec_hash(scenarios: list[dict[str, Any]]) -> str:
    payload = []
    for scenario in scenarios:
        payload.append(
            {
                "id": scenario["id"],
                "version": scenario["version"],
                "initial_step": scenario["initial_step"],
                "app_bindings": scenario["app_bindings"],
                "steps": scenario["steps"],
            }
        )
    blob = json.dumps(payload, sort_keys=True, separators=(",", ":"))
    return hashlib.sha256(blob.encode("utf-8")).hexdigest()[:12]


def generated_banner(spec_hash: str, scenario_count: int) -> list[str]:
    return [
        "// AUTO-GENERATED FILE - DO NOT EDIT",
        "// Generated by tools/story_gen/story_gen.py",
        f"// spec_hash: {spec_hash}",
        f"// scenarios: {scenario_count}",
        "",
    ]


def generate_scenarios_h(spec_hash: str, scenarios: list[dict[str, Any]]) -> str:
    lines = generated_banner(spec_hash, len(scenarios))
    lines.extend(
        [
            "#pragma once",
            "",
            "#include <Arduino.h>",
            "",
            "#include \"../core/scenario_def.h\"",
            "",
            "const ScenarioDef* generatedScenarioById(const char* id);",
            "const ScenarioDef* generatedScenarioDefault();",
            "uint8_t generatedScenarioCount();",
            "const char* generatedScenarioIdAt(uint8_t index);",
            "const char* generatedScenarioSpecHash();",
            "",
        ]
    )
    return "\n".join(lines)


def generate_apps_h(spec_hash: str, scenarios: list[dict[str, Any]]) -> str:
    lines = generated_banner(spec_hash, len(scenarios))
    lines.extend(
        [
            "#pragma once",
            "",
            "#include <Arduino.h>",
            "",
            "#include \"../core/scenario_def.h\"",
            "",
            "struct LaDetectorAppConfigDef {",
            "  const char* bindingId;",
            "  bool hasConfig;",
            "  uint32_t holdMs;",
            "  const char* unlockEvent;",
            "  bool requireListening;",
            "};",
            "",
            "const AppBindingDef* generatedAppBindingById(const char* id);",
            "uint8_t generatedAppBindingCount();",
            "const char* generatedAppBindingIdAt(uint8_t index);",
            "const LaDetectorAppConfigDef* generatedLaDetectorConfigByBindingId(const char* id);",
            "",
        ]
    )
    return "\n".join(lines)


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
                lines.append(
                    "    {"
                    + ", ".join(
                        [
                            cstr(tr["id"]),
                            TRIGGER_CPP.get(tr["trigger"], "StoryTransitionTrigger::kOnEvent"),
                            EVENT_CPP.get(tr["event_type"], "StoryEventType::kNone"),
                            cstr(tr["event_name"]),
                            f"{int(tr['after_ms'])}U",
                            cstr(tr["target_step_id"]),
                            f"{int(tr['priority'])}U",
                        ]
                    )
                    + "},"
                )
            lines.append("};")


def generate_scenarios_cpp(spec_hash: str, scenarios: list[dict[str, Any]]) -> str:
    lines: list[str] = []
    lines.extend(generated_banner(spec_hash, len(scenarios)))
    lines.extend([
        '#include "scenarios_gen.h"',
        "",
        "#include <cstring>",
        "",
        "namespace {",
    ])

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
    lines.append("  return (generatedScenarioCount() == 0U) ? nullptr : kGeneratedScenarios[0];")
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

    lines.append("const char* generatedScenarioSpecHash() {")
    lines.append(f"  return \"{spec_hash}\";")
    lines.append("}")
    lines.append("")

    return "\n".join(lines)


def generate_apps_cpp(spec_hash: str, scenarios: list[dict[str, Any]]) -> str:
    binding_map: dict[str, dict[str, Any]] = {}
    for scenario in scenarios:
        for binding in scenario["app_bindings"]:
            bid = binding["id"]
            app = binding["app"]
            config = binding.get("config")
            normalized = {"app": app, "config": config}
            if bid in binding_map and binding_map[bid] != normalized:
                raise RuntimeError(
                    f"binding '{bid}' declare avec 2 configurations differentes"
                )
            binding_map[bid] = normalized

    sorted_bindings = sorted(binding_map.items(), key=lambda x: x[0])

    lines: list[str] = []
    lines.extend(generated_banner(spec_hash, len(scenarios)))
    lines.extend([
        '#include "apps_gen.h"',
        "",
        "#include <cstring>",
        "",
        "namespace {",
        "constexpr AppBindingDef kGeneratedAppBindings[] = {",
    ])

    for bid, payload in sorted_bindings:
        app = payload["app"]
        app_cpp = APP_CPP.get(app, "StoryAppType::kNone")
        lines.append(f"    {{{cstr(bid)}, {app_cpp}}},")

    lines.append("};")
    lines.append("constexpr LaDetectorAppConfigDef kGeneratedLaConfigs[] = {")
    for bid, payload in sorted_bindings:
        app = payload["app"]
        config = payload.get("config")
        if app != "LA_DETECTOR":
            continue
        hold_ms = 3000
        unlock_event = "UNLOCK"
        require_listening = True
        has_config = False
        if isinstance(config, dict):
            hold_ms = int(config.get("hold_ms", hold_ms))
            unlock_event = str(config.get("unlock_event", unlock_event))
            require_listening = bool(config.get("require_listening", require_listening))
            has_config = True
        lines.append(
            "    {"
            + ", ".join(
                [
                    cstr(bid),
                    "true" if has_config else "false",
                    f"{hold_ms}U",
                    cstr(unlock_event),
                    "true" if require_listening else "false",
                ]
            )
            + "},"
        )

    lines.extend([
        "};",
        "}  // namespace",
        "",
        "const AppBindingDef* generatedAppBindingById(const char* id) {",
        "  if (id == nullptr || id[0] == '\\0') {",
        "    return nullptr;",
        "  }",
        "  for (const AppBindingDef& binding : kGeneratedAppBindings) {",
        "    if (binding.id != nullptr && strcmp(binding.id, id) == 0) {",
        "      return &binding;",
        "    }",
        "  }",
        "  return nullptr;",
        "}",
        "",
        "uint8_t generatedAppBindingCount() {",
        "  return static_cast<uint8_t>(sizeof(kGeneratedAppBindings) / sizeof(kGeneratedAppBindings[0]));",
        "}",
        "",
        "const char* generatedAppBindingIdAt(uint8_t index) {",
        "  if (index >= generatedAppBindingCount()) {",
        "    return nullptr;",
        "  }",
        "  return kGeneratedAppBindings[index].id;",
        "}",
        "",
        "const LaDetectorAppConfigDef* generatedLaDetectorConfigByBindingId(const char* id) {",
        "  if (id == nullptr || id[0] == '\\0') {",
        "    return nullptr;",
        "  }",
        "  for (const LaDetectorAppConfigDef& cfg : kGeneratedLaConfigs) {",
        "    if (cfg.bindingId != nullptr && strcmp(cfg.bindingId, id) == 0) {",
        "      return &cfg;",
        "    }",
        "  }",
        "  return nullptr;",
        "}",
        "",
    ])

    return "\n".join(lines)


def write_file(path: Path, content: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content + "\n", encoding="utf-8")


def collect_scenarios(spec_dir: Path, strict: bool) -> tuple[list[dict[str, Any]], list[ValidationIssue]]:
    scenarios: list[dict[str, Any]] = []
    issues: list[ValidationIssue] = []

    files = sorted(spec_dir.glob("*.yaml"))
    if not files:
        issues.append(
            ValidationIssue(
                file=spec_dir.as_posix(),
                field="*.yaml",
                reason="aucun scenario trouve",
                code="SCENARIO_NOT_FOUND",
            )
        )
        return [], issues

    scenario_ids: set[str] = set()
    for path in files:
        scenario, scenario_issues = normalize_scenario(path, strict)
        if scenario_issues:
            issues.extend(scenario_issues)
            continue
        assert scenario is not None

        scenario_id = scenario["id"]
        if scenario_id in scenario_ids:
            issues.append(
                ValidationIssue(
                    file=path.as_posix(),
                    field="id",
                    reason=f"duplicata global '{scenario_id}'",
                    code="SCENARIO_ID_DUP",
                )
            )
            continue

        scenario_ids.add(scenario_id)
        scenarios.append(scenario)

    return scenarios, issues


def cmd_validate(args: argparse.Namespace) -> int:
    spec_dir = Path(args.spec_dir)
    scenarios, issues = collect_scenarios(spec_dir, args.strict)
    if issues:
        for issue in issues:
            print(f"[story-validate] ERR {issue.format()}")
        return 1

    for scenario in scenarios:
        print(
            f"[story-validate] OK file={scenario['source']} id={scenario['id']} "
            f"steps={len(scenario['steps'])} bindings={len(scenario['app_bindings'])}"
        )
    print(f"[story-validate] OK total={len(scenarios)} strict={1 if args.strict else 0}")
    return 0


def cmd_generate(args: argparse.Namespace) -> int:
    spec_dir = Path(args.spec_dir)
    out_dir = Path(args.out_dir)

    scenarios, issues = collect_scenarios(spec_dir, args.strict)
    if issues:
        for issue in issues:
            print(f"[story-gen] ERR {issue.format()}")
        return 1

    scenarios.sort(key=lambda sc: sc["id"])
    spec_hash = scenario_spec_hash(scenarios)

    scenarios_h = generate_scenarios_h(spec_hash, scenarios)
    scenarios_cpp = generate_scenarios_cpp(spec_hash, scenarios)
    apps_h = generate_apps_h(spec_hash, scenarios)
    apps_cpp = generate_apps_cpp(spec_hash, scenarios)

    write_file(out_dir / "scenarios_gen.h", scenarios_h)
    write_file(out_dir / "scenarios_gen.cpp", scenarios_cpp)
    write_file(out_dir / "apps_gen.h", apps_h)
    write_file(out_dir / "apps_gen.cpp", apps_cpp)

    print(
        f"[story-gen] OK scenarios={len(scenarios)} out={out_dir.as_posix()} "
        f"strict={1 if args.strict else 0} spec_hash={spec_hash}"
    )
    return 0


def build_arg_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="StorySpec YAML validator/generator")
    sub = parser.add_subparsers(dest="command", required=True)

    validate_p = sub.add_parser("validate", help="validate YAML scenarios")
    validate_p.add_argument("--spec-dir", default="../docs/protocols/story_specs/scenarios")
    validate_p.add_argument("--strict", action="store_true", help="reject unknown fields")
    validate_p.set_defaults(func=cmd_validate)

    generate_p = sub.add_parser("generate", help="generate C++ from YAML scenarios")
    generate_p.add_argument("--spec-dir", default="../docs/protocols/story_specs/scenarios")
    generate_p.add_argument("--out-dir", default="src/story/generated")
    generate_p.add_argument("--strict", action="store_true", help="validate in strict mode before generate")
    generate_p.set_defaults(func=cmd_generate)

    return parser


def main(argv: list[str]) -> int:
    parser = build_arg_parser()
    args = parser.parse_args(argv)
    return args.func(args)


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
