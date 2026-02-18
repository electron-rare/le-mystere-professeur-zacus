from __future__ import annotations

import hashlib
import json
import tarfile
from dataclasses import dataclass
from pathlib import Path
from typing import Any

try:
    import yaml
except Exception:  # pragma: no cover - dependency error surfaced at runtime
    yaml = None

try:
    import yamale
    from yamale.yamale_error import YamaleError
except Exception:  # pragma: no cover - dependency error surfaced at runtime
    yamale = None
    YamaleError = Exception

try:
    from jinja2 import Environment, FileSystemLoader
except Exception:  # pragma: no cover - dependency error surfaced at runtime
    Environment = None
    FileSystemLoader = None


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


class StoryGenerationError(RuntimeError):
    """Raised when validation or generation fails."""


@dataclass
class StoryPaths:
    fw_root: Path
    repo_root: Path
    game_scenarios_dir: Path
    story_specs_dir: Path
    generated_cpp_dir: Path
    bundle_root: Path


@dataclass
class ValidationIssue:
    file: str
    field: str
    reason: str

    def format(self) -> str:
        return f"{self.file}: {self.field}: {self.reason}"


def _require_deps() -> None:
    missing = []
    if yaml is None:
        missing.append("PyYAML")
    if yamale is None:
        missing.append("yamale")
    if Environment is None or FileSystemLoader is None:
        missing.append("Jinja2")
    if missing:
        joined = ", ".join(missing)
        raise StoryGenerationError(
            f"Missing dependencies: {joined}. Install with: pip install pyyaml yamale Jinja2"
        )


def find_fw_root() -> Path:
    start = Path(__file__).resolve()
    for candidate in [start, *start.parents]:
        if (candidate / "platformio.ini").exists():
            return candidate
    raise StoryGenerationError("Cannot resolve firmware root (platformio.ini not found)")


def default_paths() -> StoryPaths:
    fw_root = find_fw_root()
    repo_root = fw_root.parents[1]
    return StoryPaths(
        fw_root=fw_root,
        repo_root=repo_root,
        game_scenarios_dir=repo_root / "game" / "scenarios",
        story_specs_dir=fw_root / "docs" / "protocols" / "story_specs" / "scenarios",
        generated_cpp_dir=fw_root / "hardware" / "libs" / "story" / "src" / "generated",
        bundle_root=fw_root / "artifacts" / "story_fs" / "deploy",
    )


def _schema_path(name: str) -> Path:
    return Path(__file__).resolve().parent / "schemas" / name


def _template_dir() -> Path:
    return Path(__file__).resolve().parent / "templates"


def _list_yaml_files(path: Path) -> list[Path]:
    if path.is_file() and path.suffix.lower() in {".yml", ".yaml"}:
        return [path]
    if not path.exists():
        return []
    return sorted([p for p in path.glob("*.y*ml") if p.is_file()])


def _validate_yamale(schema_path: Path, files: list[Path]) -> None:
    if not files:
        raise StoryGenerationError(f"No YAML files found for schema: {schema_path.name}")
    schema = yamale.make_schema(str(schema_path))
    errors: list[str] = []
    for file_path in files:
        data = yamale.make_data(str(file_path))
        try:
            yamale.validate(schema, data, strict=True)
        except YamaleError as exc:
            for result in exc.results:
                for msg in result.errors:
                    errors.append(f"{file_path}: {msg}")
    if errors:
        raise StoryGenerationError("Yamale validation failed:\n" + "\n".join(errors))


def _load_yaml(path: Path) -> dict[str, Any]:
    obj = yaml.safe_load(path.read_text(encoding="utf-8"))
    if not isinstance(obj, dict):
        raise StoryGenerationError(f"YAML root must be a mapping: {path}")
    return obj


def _normalize_story_specs(files: list[Path]) -> list[dict[str, Any]]:
    issues: list[ValidationIssue] = []
    scenarios: list[dict[str, Any]] = []
    ids: set[str] = set()

    for file_path in files:
        raw = _load_yaml(file_path)
        sid = str(raw.get("id", "")).strip()
        if not sid:
            issues.append(ValidationIssue(str(file_path), "id", "missing id"))
            continue
        if sid in ids:
            issues.append(ValidationIssue(str(file_path), "id", f"duplicate id '{sid}'"))
            continue
        ids.add(sid)

        version = raw.get("version", 0)
        if not isinstance(version, int):
            issues.append(ValidationIssue(str(file_path), "version", "must be int"))
            continue

        initial_step = str(raw.get("initial_step", "")).strip()
        bindings_raw = raw.get("app_bindings", [])
        steps_raw = raw.get("steps", [])
        if not isinstance(bindings_raw, list) or not isinstance(steps_raw, list):
            issues.append(ValidationIssue(str(file_path), "root", "app_bindings and steps must be lists"))
            continue

        bindings: list[dict[str, Any]] = []
        binding_ids: set[str] = set()
        for idx, binding in enumerate(bindings_raw):
            if not isinstance(binding, dict):
                issues.append(ValidationIssue(str(file_path), f"app_bindings[{idx}]", "must be mapping"))
                continue
            bid = str(binding.get("id", "")).strip()
            app = str(binding.get("app", "")).strip()
            if not bid:
                issues.append(ValidationIssue(str(file_path), f"app_bindings[{idx}].id", "missing id"))
                continue
            if app not in ALLOWED_APP:
                issues.append(
                    ValidationIssue(str(file_path), f"app_bindings[{idx}].app", f"invalid app '{app}'")
                )
                continue
            if bid in binding_ids:
                issues.append(ValidationIssue(str(file_path), f"app_bindings[{idx}].id", f"duplicate '{bid}'"))
                continue
            binding_ids.add(bid)

            config = binding.get("config") if isinstance(binding.get("config"), dict) else None
            if app == "LA_DETECTOR":
                hold_ms = 3000
                unlock_event = "UNLOCK"
                require_listening = True
                if config is not None:
                    hold_raw = config.get("hold_ms", hold_ms)
                    if isinstance(hold_raw, int):
                        hold_ms = hold_raw
                    unlock_raw = config.get("unlock_event", unlock_event)
                    if isinstance(unlock_raw, str) and unlock_raw.strip():
                        unlock_event = unlock_raw.strip()
                    listen_raw = config.get("require_listening", require_listening)
                    if isinstance(listen_raw, bool):
                        require_listening = listen_raw
                config = {
                    "hold_ms": hold_ms,
                    "unlock_event": unlock_event,
                    "require_listening": require_listening,
                }
            else:
                config = None

            bindings.append({"id": bid, "app": app, "config": config})

        steps: list[dict[str, Any]] = []
        step_ids: set[str] = set()
        for sidx, step in enumerate(steps_raw):
            if not isinstance(step, dict):
                issues.append(ValidationIssue(str(file_path), f"steps[{sidx}]", "must be mapping"))
                continue
            step_id = str(step.get("step_id", "")).strip()
            if not step_id:
                issues.append(ValidationIssue(str(file_path), f"steps[{sidx}].step_id", "missing step_id"))
                continue
            if step_id in step_ids:
                issues.append(ValidationIssue(str(file_path), f"steps[{sidx}].step_id", f"duplicate '{step_id}'"))
                continue
            step_ids.add(step_id)

            apps = [str(v).strip() for v in (step.get("apps") or []) if str(v).strip()]
            for app_id in apps:
                if app_id not in binding_ids:
                    issues.append(
                        ValidationIssue(
                            str(file_path),
                            f"steps[{sidx}].apps",
                            f"unknown app binding '{app_id}'",
                        )
                    )

            transitions_norm: list[dict[str, Any]] = []
            for tidx, transition in enumerate(step.get("transitions") or []):
                if not isinstance(transition, dict):
                    issues.append(
                        ValidationIssue(str(file_path), f"steps[{sidx}].transitions[{tidx}]", "must be mapping")
                    )
                    continue
                trigger = str(transition.get("trigger", "on_event")).strip()
                event_type = str(transition.get("event_type", "none")).strip()
                event_name = str(transition.get("event_name", "")).strip()
                target_step_id = str(transition.get("target_step_id", "")).strip()
                after_ms = transition.get("after_ms", 0)
                priority = transition.get("priority", 0)
                tr_id = str(transition.get("id", "")).strip()
                if not tr_id:
                    tr_id = f"TR_{step_id}_{tidx + 1}"
                if trigger not in ALLOWED_TRIGGER:
                    issues.append(
                        ValidationIssue(
                            str(file_path),
                            f"steps[{sidx}].transitions[{tidx}].trigger",
                            f"invalid trigger '{trigger}'",
                        )
                    )
                if event_type not in ALLOWED_EVENT:
                    issues.append(
                        ValidationIssue(
                            str(file_path),
                            f"steps[{sidx}].transitions[{tidx}].event_type",
                            f"invalid event_type '{event_type}'",
                        )
                    )
                if not isinstance(after_ms, int):
                    issues.append(
                        ValidationIssue(
                            str(file_path),
                            f"steps[{sidx}].transitions[{tidx}].after_ms",
                            "must be int",
                        )
                    )
                    after_ms = 0
                if not isinstance(priority, int):
                    issues.append(
                        ValidationIssue(
                            str(file_path),
                            f"steps[{sidx}].transitions[{tidx}].priority",
                            "must be int",
                        )
                    )
                    priority = 0

                transitions_norm.append(
                    {
                        "id": tr_id,
                        "trigger": trigger,
                        "event_type": event_type,
                        "event_name": event_name,
                        "target_step_id": target_step_id,
                        "after_ms": after_ms,
                        "priority": priority,
                    }
                )

            actions = [str(v).strip() for v in (step.get("actions") or []) if str(v).strip()]
            steps.append(
                {
                    "step_id": step_id,
                    "screen_scene_id": str(step.get("screen_scene_id", "")).strip(),
                    "audio_pack_id": str(step.get("audio_pack_id", "")).strip(),
                    "actions": actions,
                    "apps": apps,
                    "mp3_gate_open": bool(step.get("mp3_gate_open", False)),
                    "transitions": transitions_norm,
                }
            )

        if initial_step and initial_step not in step_ids:
            issues.append(ValidationIssue(str(file_path), "initial_step", f"unknown step '{initial_step}'"))

        for step in steps:
            for transition in step["transitions"]:
                if transition["target_step_id"] and transition["target_step_id"] not in step_ids:
                    issues.append(
                        ValidationIssue(
                            str(file_path),
                            f"steps[{step['step_id']}].transitions[{transition['id']}].target_step_id",
                            f"unknown target '{transition['target_step_id']}'",
                        )
                    )

        scenarios.append(
            {
                "id": sid,
                "version": version,
                "estimated_duration_s": int(raw.get("estimated_duration_s", 0) or 0),
                "initial_step": initial_step,
                "app_bindings": bindings,
                "steps": steps,
                "source": str(file_path),
            }
        )

    if issues:
        formatted = "\n".join(issue.format() for issue in issues)
        raise StoryGenerationError("Story semantic validation failed:\n" + formatted)

    scenarios.sort(key=lambda item: item["id"])
    return scenarios


def _validate_game_scenarios(game_scenario_files: list[Path]) -> list[str]:
    ids: list[str] = []
    for file_path in game_scenario_files:
        doc = _load_yaml(file_path)
        sid = str(doc.get("id", "")).strip()
        if sid:
            ids.append(sid)
    return sorted(ids)


def _sha_hex(payload: bytes) -> str:
    return hashlib.sha256(payload).hexdigest()


def _story_spec_hash(scenarios: list[dict[str, Any]]) -> str:
    canonical = json.dumps(scenarios, sort_keys=True, separators=(",", ":"))
    return _sha_hex(canonical.encode("utf-8"))[:12]


def _create_cpp_context(scenarios: list[dict[str, Any]], spec_hash: str) -> dict[str, Any]:
    normalized: list[dict[str, Any]] = []
    for sidx, scenario in enumerate(scenarios):
        step_entries: list[dict[str, Any]] = []
        for tidx, step in enumerate(scenario["steps"]):
            prefix = f"kSc{sidx}St{tidx}"
            transitions: list[dict[str, Any]] = []
            for transition in step["transitions"]:
                transitions.append(
                    {
                        "id": transition["id"],
                        "trigger_cpp": TRIGGER_CPP.get(transition["trigger"], "StoryTransitionTrigger::kOnEvent"),
                        "event_cpp": EVENT_CPP.get(transition["event_type"], "StoryEventType::kNone"),
                        "event_name": transition["event_name"],
                        "after_ms": max(0, int(transition["after_ms"])),
                        "target_step_id": transition["target_step_id"],
                        "priority": max(0, int(transition["priority"])),
                    }
                )
            step_entries.append(
                {
                    "prefix": prefix,
                    "id": step["step_id"],
                    "screen_scene_id": step["screen_scene_id"] or "nullptr",
                    "audio_pack_id": step["audio_pack_id"] or "nullptr",
                    "actions": step["actions"],
                    "apps": step["apps"],
                    "transitions": transitions,
                    "mp3_gate_open": "true" if step["mp3_gate_open"] else "false",
                }
            )
        normalized.append(
            {
                "index": sidx,
                "id": scenario["id"],
                "version": scenario["version"],
                "initial_step": scenario["initial_step"],
                "steps": step_entries,
            }
        )

    app_bindings: dict[str, dict[str, Any]] = {}
    for scenario in scenarios:
        for binding in scenario["app_bindings"]:
            app_bindings[binding["id"]] = binding
    app_entries = [
        {
            "id": app_id,
            "app_cpp": APP_CPP[binding["app"]],
            "la_config": binding.get("config") if binding["app"] == "LA_DETECTOR" else None,
        }
        for app_id, binding in sorted(app_bindings.items(), key=lambda item: item[0])
    ]

    return {
        "spec_hash": spec_hash,
        "scenario_count": len(scenarios),
        "scenarios": normalized,
        "app_entries": app_entries,
    }


def _render_template(template_name: str, context: dict[str, Any]) -> str:
    env = Environment(loader=FileSystemLoader(str(_template_dir())), trim_blocks=True, lstrip_blocks=True)
    template = env.get_template(template_name)
    return template.render(**context).rstrip() + "\n"


def _write_text(path: Path, content: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding="utf-8")


def generate_cpp_files(scenarios: list[dict[str, Any]], out_dir: Path) -> str:
    spec_hash = _story_spec_hash(scenarios)
    context = _create_cpp_context(scenarios, spec_hash)
    _write_text(out_dir / "scenarios_gen.h", _render_template("scenarios_gen.h.j2", context))
    _write_text(out_dir / "scenarios_gen.cpp", _render_template("scenarios_gen.cpp.j2", context))
    _write_text(out_dir / "apps_gen.h", _render_template("apps_gen.h.j2", context))
    _write_text(out_dir / "apps_gen.cpp", _render_template("apps_gen.cpp.j2", context))
    return spec_hash


def _json_compact(payload: dict[str, Any]) -> bytes:
    return json.dumps(payload, sort_keys=True, separators=(",", ":")).encode("utf-8")


def _write_json_with_checksum(root: Path, rel_path: str, payload: dict[str, Any]) -> None:
    blob = _json_compact(payload)
    out_path = root / rel_path
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_bytes(blob)
    (out_path.with_suffix(".sha256")).write_text(_sha_hex(blob) + "\n", encoding="utf-8")


def generate_bundle_files(scenarios: list[dict[str, Any]], out_dir: Path, spec_hash: str) -> None:
    resources: dict[str, set[str]] = {
        "screens": set(),
        "audio": set(),
        "actions": set(),
        "apps": set(),
    }
    bindings: dict[str, dict[str, Any]] = {}

    for scenario in scenarios:
        scenario_payload = {
            "id": scenario["id"],
            "version": scenario["version"],
            "estimated_duration_s": scenario.get("estimated_duration_s", 0),
            "initial_step": scenario["initial_step"],
            "app_bindings": scenario["app_bindings"],
            "steps": scenario["steps"],
        }
        _write_json_with_checksum(out_dir, f"story/scenarios/{scenario['id']}.json", scenario_payload)

        for binding in scenario["app_bindings"]:
            resources["apps"].add(binding["id"])
            bindings[binding["id"]] = {
                "id": binding["id"],
                "app": binding["app"],
                "config": binding.get("config"),
            }

        for step in scenario["steps"]:
            if step["screen_scene_id"]:
                resources["screens"].add(step["screen_scene_id"])
            if step["audio_pack_id"]:
                resources["audio"].add(step["audio_pack_id"])
            for action in step["actions"]:
                resources["actions"].add(action)

    for app_id in sorted(resources["apps"]):
        _write_json_with_checksum(out_dir, f"story/apps/{app_id}.json", bindings[app_id])
    for screen_id in sorted(resources["screens"]):
        _write_json_with_checksum(out_dir, f"story/screens/{screen_id}.json", {"id": screen_id})
    for audio_id in sorted(resources["audio"]):
        _write_json_with_checksum(out_dir, f"story/audio/{audio_id}.json", {"id": audio_id})
    for action_id in sorted(resources["actions"]):
        _write_json_with_checksum(out_dir, f"story/actions/{action_id}.json", {"id": action_id})

    manifest = {
        "spec_hash": spec_hash,
        "scenarios": [scenario["id"] for scenario in scenarios],
        "resource_counts": {key: len(values) for key, values in resources.items()},
    }
    _write_json_with_checksum(out_dir, "story/manifest.json", manifest)


def create_archive(root: Path, archive_path: Path) -> None:
    archive_path.parent.mkdir(parents=True, exist_ok=True)
    with tarfile.open(archive_path, "w:gz") as tar:
        for path in sorted(root.rglob("*")):
            if path.is_file():
                tar.add(path, arcname=path.relative_to(root))


def load_and_validate(paths: StoryPaths, spec_dir: Path | None = None, game_dir: Path | None = None) -> tuple[list[dict[str, Any]], list[str]]:
    _require_deps()
    actual_spec_dir = spec_dir or paths.story_specs_dir
    actual_game_dir = game_dir or paths.game_scenarios_dir

    spec_files = _list_yaml_files(actual_spec_dir)
    game_files = _list_yaml_files(actual_game_dir)

    _validate_yamale(_schema_path("story_spec_schema.yamale"), spec_files)
    _validate_yamale(_schema_path("game_scenario_schema.yamale"), game_files)

    scenarios = _normalize_story_specs(spec_files)
    game_ids = _validate_game_scenarios(game_files)
    return scenarios, game_ids


def run_validate(paths: StoryPaths, spec_dir: Path | None = None, game_dir: Path | None = None) -> dict[str, Any]:
    scenarios, game_ids = load_and_validate(paths, spec_dir=spec_dir, game_dir=game_dir)
    return {
        "scenarios": [item["id"] for item in scenarios],
        "scenario_count": len(scenarios),
        "game_scenarios": game_ids,
        "game_scenario_count": len(game_ids),
    }


def run_generate_cpp(
    paths: StoryPaths,
    out_dir: Path | None = None,
    spec_dir: Path | None = None,
    game_dir: Path | None = None,
) -> dict[str, Any]:
    scenarios, game_ids = load_and_validate(paths, spec_dir=spec_dir, game_dir=game_dir)
    cpp_out = out_dir or paths.generated_cpp_dir
    spec_hash = generate_cpp_files(scenarios, cpp_out)
    return {
        "spec_hash": spec_hash,
        "out_dir": cpp_out,
        "scenario_count": len(scenarios),
        "game_scenario_count": len(game_ids),
    }


def run_generate_bundle(
    paths: StoryPaths,
    out_dir: Path | None = None,
    archive: Path | None = None,
    spec_dir: Path | None = None,
    game_dir: Path | None = None,
) -> dict[str, Any]:
    scenarios, game_ids = load_and_validate(paths, spec_dir=spec_dir, game_dir=game_dir)
    bundle_out = out_dir or paths.bundle_root
    spec_hash = _story_spec_hash(scenarios)

    if bundle_out.exists():
        for file_path in sorted(bundle_out.rglob("*"), reverse=True):
            if file_path.is_file():
                file_path.unlink()

    generate_bundle_files(scenarios, bundle_out, spec_hash)

    archive_path = archive
    if archive_path is not None:
        create_archive(bundle_out, archive_path)

    return {
        "spec_hash": spec_hash,
        "out_dir": bundle_out,
        "archive": archive_path,
        "scenario_count": len(scenarios),
        "game_scenario_count": len(game_ids),
    }
