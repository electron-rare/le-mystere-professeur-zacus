from __future__ import annotations

import copy
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
ALLOWED_APP = {"LA_DETECTOR", "AUDIO_PACK", "SCREEN_SCENE", "MP3_GATE", "WIFI_STACK", "ESPNOW_STACK"}

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
    "WIFI_STACK": "StoryAppType::kWifiStack",
    "ESPNOW_STACK": "StoryAppType::kEspNowStack",
}

DEFAULT_SCENE_PROFILE: dict[str, Any] = {
    "title": "MISSION",
    "subtitle": "",
    "symbol": "RUN",
    "effect": "pulse",
    "effect_speed_ms": 680,
    "theme": {"bg": "#07132A", "accent": "#2A76FF", "text": "#E8F1FF"},
    "transition": {"effect": "fade", "duration_ms": 240},
    "timeline": [
        {
            "at_ms": 0,
            "effect": "pulse",
            "speed_ms": 680,
            "theme": {"bg": "#07132A", "accent": "#2A76FF", "text": "#E8F1FF"},
        },
        {
            "at_ms": 1400,
            "effect": "blink",
            "speed_ms": 360,
            "theme": {"bg": "#0B1F3C", "accent": "#5A99FF", "text": "#F1F7FF"},
        },
    ],
}

SCENE_PROFILES: dict[str, dict[str, Any]] = {
    "SCENE_LOCKED": {
        "title": "VERROUILLE",
        "subtitle": "Attente de debloquage",
        "symbol": "LOCK",
        "effect": "pulse",
        "effect_speed_ms": 680,
        "theme": {"bg": "#08152D", "accent": "#3E8DFF", "text": "#F5F8FF"},
        "transition": {"effect": "fade", "duration_ms": 260},
        "timeline": [
            {
                "at_ms": 0,
                "effect": "pulse",
                "speed_ms": 680,
                "theme": {"bg": "#08152D", "accent": "#3E8DFF", "text": "#F5F8FF"},
            },
            {
                "at_ms": 1400,
                "effect": "blink",
                "speed_ms": 420,
                "theme": {"bg": "#0A1E3A", "accent": "#74B0FF", "text": "#F8FBFF"},
            },
        ],
    },
    "SCENE_BROKEN": {
        "title": "PROTO U-SON",
        "subtitle": "Signal brouille",
        "symbol": "ALERT",
        "effect": "blink",
        "effect_speed_ms": 180,
        "theme": {"bg": "#2A0508", "accent": "#FF4A45", "text": "#FFF1F1"},
        "transition": {"effect": "glitch", "duration_ms": 160},
        "timeline": [
            {
                "at_ms": 0,
                "effect": "blink",
                "speed_ms": 180,
                "theme": {"bg": "#2A0508", "accent": "#FF4A45", "text": "#FFF1F1"},
            },
            {
                "at_ms": 760,
                "effect": "blink",
                "speed_ms": 140,
                "theme": {"bg": "#33070C", "accent": "#FF7A75", "text": "#FFF6F5"},
            },
        ],
    },
    "SCENE_LA_DETECTOR": {
        "title": "DETECTION",
        "subtitle": "Balayage en cours",
        "symbol": "SCAN",
        "effect": "scan",
        "effect_speed_ms": 960,
        "theme": {"bg": "#041F1B", "accent": "#2CE5A6", "text": "#EFFFF8"},
        "transition": {"effect": "slide_up", "duration_ms": 220},
        "timeline": [
            {
                "at_ms": 0,
                "effect": "scan",
                "speed_ms": 960,
                "theme": {"bg": "#041F1B", "accent": "#2CE5A6", "text": "#EFFFF8"},
            },
            {
                "at_ms": 1500,
                "effect": "pulse",
                "speed_ms": 620,
                "theme": {"bg": "#062923", "accent": "#63F0C3", "text": "#F3FFFB"},
            },
        ],
    },
    "SCENE_SEARCH": {
        "title": "RECHERCHE",
        "subtitle": "Analyse des indices",
        "symbol": "SCAN",
        "effect": "scan",
        "effect_speed_ms": 920,
        "theme": {"bg": "#05261F", "accent": "#35E7B0", "text": "#EFFFF8"},
        "transition": {"effect": "glitch", "duration_ms": 230},
        "timeline": [
            {
                "at_ms": 0,
                "effect": "scan",
                "speed_ms": 920,
                "theme": {"bg": "#05261F", "accent": "#35E7B0", "text": "#EFFFF8"},
            },
            {
                "at_ms": 1600,
                "effect": "wave",
                "speed_ms": 520,
                "theme": {"bg": "#07322A", "accent": "#67F0C4", "text": "#F2FFF9"},
            },
            {
                "at_ms": 3000,
                "effect": "scan",
                "speed_ms": 820,
                "theme": {"bg": "#05261F", "accent": "#35E7B0", "text": "#EFFFF8"},
            },
        ],
    },
    "SCENE_CAMERA_SCAN": {
        "title": "CAMERA SCAN",
        "subtitle": "Capture des indices visuels",
        "symbol": "SCAN",
        "effect": "radar",
        "effect_speed_ms": 840,
        "theme": {"bg": "#041A24", "accent": "#5CE6FF", "text": "#E9FBFF"},
        "transition": {"effect": "slide_left", "duration_ms": 230},
        "timeline": [
            {
                "at_ms": 0,
                "effect": "radar",
                "speed_ms": 840,
                "theme": {"bg": "#041A24", "accent": "#5CE6FF", "text": "#E9FBFF"},
            },
            {
                "at_ms": 1200,
                "effect": "wave",
                "speed_ms": 620,
                "theme": {"bg": "#072838", "accent": "#8AF1FF", "text": "#F5FEFF"},
            },
            {
                "at_ms": 2200,
                "effect": "radar",
                "speed_ms": 760,
                "theme": {"bg": "#041A24", "accent": "#5CE6FF", "text": "#E9FBFF"},
            },
        ],
    },
    "SCENE_SIGNAL_SPIKE": {
        "title": "PIC DE SIGNAL",
        "subtitle": "Interference soudaine detectee",
        "symbol": "ALERT",
        "effect": "wave",
        "effect_speed_ms": 260,
        "theme": {"bg": "#24090C", "accent": "#FF6A52", "text": "#FFF2EB"},
        "transition": {"effect": "glitch", "duration_ms": 170},
        "timeline": [
            {
                "at_ms": 0,
                "effect": "wave",
                "speed_ms": 260,
                "theme": {"bg": "#24090C", "accent": "#FF6A52", "text": "#FFF2EB"},
            },
            {
                "at_ms": 700,
                "effect": "blink",
                "speed_ms": 180,
                "theme": {"bg": "#2F1014", "accent": "#FF8C73", "text": "#FFF8F5"},
            },
            {
                "at_ms": 1400,
                "effect": "wave",
                "speed_ms": 320,
                "theme": {"bg": "#24090C", "accent": "#FF6A52", "text": "#FFF2EB"},
            },
        ],
    },
    "SCENE_MEDIA_ARCHIVE": {
        "title": "ARCHIVES MEDIA",
        "subtitle": "Photos et enregistrements sauvegardes",
        "symbol": "READY",
        "effect": "radar",
        "effect_speed_ms": 760,
        "theme": {"bg": "#0D1A34", "accent": "#7CB1FF", "text": "#EEF4FF"},
        "transition": {"effect": "fade", "duration_ms": 240},
        "timeline": [
            {
                "at_ms": 0,
                "effect": "radar",
                "speed_ms": 760,
                "theme": {"bg": "#0D1A34", "accent": "#7CB1FF", "text": "#EEF4FF"},
            },
            {
                "at_ms": 1000,
                "effect": "pulse",
                "speed_ms": 620,
                "theme": {"bg": "#132245", "accent": "#9CC7FF", "text": "#F7FAFF"},
            },
            {
                "at_ms": 2000,
                "effect": "radar",
                "speed_ms": 760,
                "theme": {"bg": "#0D1A34", "accent": "#7CB1FF", "text": "#EEF4FF"},
            },
        ],
    },
    "SCENE_WIN": {
        "title": "VICTOIRE",
        "subtitle": "Etape validee",
        "symbol": "WIN",
        "effect": "celebrate",
        "effect_speed_ms": 420,
        "theme": {"bg": "#231038", "accent": "#F4CB4A", "text": "#FFF8E2"},
        "transition": {"effect": "zoom", "duration_ms": 280},
        "timeline": [
            {
                "at_ms": 0,
                "effect": "celebrate",
                "speed_ms": 420,
                "theme": {"bg": "#231038", "accent": "#F4CB4A", "text": "#FFF8E2"},
            },
            {
                "at_ms": 1000,
                "effect": "blink",
                "speed_ms": 240,
                "theme": {"bg": "#341A4D", "accent": "#FFE083", "text": "#FFFDF3"},
            },
        ],
    },
    "SCENE_WIN_ETAPE": {
        "title": "VICTOIRE",
        "subtitle": "Etape validee",
        "symbol": "WIN",
        "effect": "celebrate",
        "effect_speed_ms": 420,
        "theme": {"bg": "#231038", "accent": "#F4CB4A", "text": "#FFF8E2"},
        "transition": {"effect": "zoom", "duration_ms": 280},
        "timeline": [
            {
                "at_ms": 0,
                "effect": "celebrate",
                "speed_ms": 420,
                "theme": {"bg": "#231038", "accent": "#F4CB4A", "text": "#FFF8E2"},
            },
            {
                "at_ms": 1000,
                "effect": "blink",
                "speed_ms": 240,
                "theme": {"bg": "#341A4D", "accent": "#FFE083", "text": "#FFFDF3"},
            },
        ],
    },
    "SCENE_REWARD": {
        "title": "RECOMPENSE",
        "subtitle": "Indice debloque",
        "symbol": "WIN",
        "effect": "celebrate",
        "effect_speed_ms": 420,
        "theme": {"bg": "#2A103E", "accent": "#F9D860", "text": "#FFF9E6"},
        "transition": {"effect": "zoom", "duration_ms": 300},
        "timeline": [
            {
                "at_ms": 0,
                "effect": "celebrate",
                "speed_ms": 420,
                "theme": {"bg": "#2A103E", "accent": "#F9D860", "text": "#FFF9E6"},
            },
            {
                "at_ms": 1200,
                "effect": "pulse",
                "speed_ms": 280,
                "theme": {"bg": "#3E1A52", "accent": "#FFD97D", "text": "#FFFDF2"},
            },
        ],
    },
    "SCENE_READY": {
        "title": "PRET",
        "subtitle": "Scenario termine",
        "symbol": "READY",
        "effect": "wave",
        "effect_speed_ms": 560,
        "theme": {"bg": "#0F2A12", "accent": "#6CD96B", "text": "#EDFFED"},
        "transition": {"effect": "fade", "duration_ms": 220},
        "timeline": [
            {
                "at_ms": 0,
                "effect": "wave",
                "speed_ms": 560,
                "theme": {"bg": "#0F2A12", "accent": "#6CD96B", "text": "#EDFFED"},
            },
            {
                "at_ms": 1500,
                "effect": "radar",
                "speed_ms": 740,
                "theme": {"bg": "#133517", "accent": "#9EE49D", "text": "#F4FFF4"},
            },
        ],
    },
}

SCENE_ALIASES: dict[str, str] = {
    "SCENE_LA_DETECT": "SCENE_LA_DETECTOR",
}


def _canonical_scene_id(scene_id: str) -> str:
    return SCENE_ALIASES.get(scene_id, scene_id)

DEFAULT_TEXT_OPTIONS: dict[str, Any] = {
    "show_title": False,
    "show_subtitle": True,
    "show_symbol": True,
    "title_case": "upper",
    "subtitle_case": "raw",
    "title_align": "top",
    "subtitle_align": "bottom",
}

DEFAULT_FRAMING_OPTIONS: dict[str, Any] = {
    "preset": "center",
    "x_offset": 0,
    "y_offset": 0,
    "scale_pct": 100,
}

DEFAULT_SCROLL_OPTIONS: dict[str, Any] = {
    "mode": "none",
    "speed_ms": 4200,
    "pause_ms": 900,
    "loop": True,
}

DEFAULT_DEMO_OPTIONS: dict[str, Any] = {
    "mode": "standard",
    "particle_count": 4,
    "strobe_level": 65,
}

SCREEN_EFFECT_CHOICES = {"none", "pulse", "scan", "radar", "wave", "blink", "celebrate"}
SCREEN_EFFECT_ALIASES = {
    "steady": "none",
    "glitch": "blink",
    "reward": "celebrate",
    "sonar": "radar",
}
TRANSITION_EFFECT_CHOICES = {"none", "fade", "slide_left", "slide_right", "slide_up", "slide_down", "zoom", "glitch"}
TRANSITION_EFFECT_ALIASES = {
    "crossfade": "fade",
    "left": "slide_left",
    "right": "slide_right",
    "up": "slide_up",
    "down": "slide_down",
    "zoom_in": "zoom",
    "flash": "glitch",
    "wipe": "slide_left",
    "camera_flash": "glitch",
}
TEXT_CASE_CHOICES = {"raw", "upper", "lower"}
TEXT_ALIGN_CHOICES = {"top", "center", "bottom"}
FRAMING_PRESET_CHOICES = {"center", "focus_top", "focus_bottom", "split"}
SCROLL_MODE_CHOICES = {"none", "marquee"}
SCROLL_MODE_ALIASES = {"ticker": "marquee", "crawl": "marquee"}
DEMO_MODE_CHOICES = {"standard", "cinematic", "arcade"}


class StoryGenerationError(RuntimeError):
    """Raised when validation or generation fails."""


@dataclass
class StoryPaths:
    fw_root: Path
    repo_root: Path
    game_scenarios_dir: Path
    story_specs_dir: Path
    story_data_dir: Path
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
        story_data_dir=fw_root / "data" / "story",
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


def _normalize_scene_id(value: Any, issues: list[ValidationIssue], source: str) -> str:
    scene_id = str(value).strip() if isinstance(value, str) else ""
    if not scene_id:
        return ""
    scene_id = _canonical_scene_id(scene_id)
    if scene_id not in SCENE_PROFILES:
        issues.append(ValidationIssue(source, "screen_scene_id", f"unknown scene id '{scene_id}'"))
        return ""
    return scene_id


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

            raw_scene_id = str(step.get("screen_scene_id", "")).strip()
            normalized_scene_id = _normalize_scene_id(raw_scene_id, issues, str(file_path))
            actions = [str(v).strip() for v in (step.get("actions") or []) if str(v).strip()]
            steps.append(
                {
                    "step_id": step_id,
                    "screen_scene_id": normalized_scene_id,
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


def _resource_slug(resource_id: str, prefix: str) -> str:
    slug = resource_id
    if slug.startswith(prefix):
        slug = slug[len(prefix) :]
    return slug.lower()


def _resource_candidates(resource_root: Path, resource_type: str, resource_id: str) -> list[Path]:
    candidates: list[Path] = []
    base = resource_root / resource_type
    candidates.append(base / f"{resource_id}.json")
    candidates.append(base / f"{resource_id.lower()}.json")

    if resource_type == "screens":
        slug = _resource_slug(resource_id, "SCENE_")
        candidates.append(base / f"{slug}.json")
    elif resource_type == "audio":
        slug = _resource_slug(resource_id, "PACK_")
        candidates.append(base / f"{slug}.json")

    # de-duplicate while preserving order
    dedup: list[Path] = []
    seen: set[str] = set()
    for path in candidates:
        marker = str(path)
        if marker in seen:
            continue
        seen.add(marker)
        dedup.append(path)
    return dedup


def _load_resource_payload(
    resource_root: Path | None, resource_type: str, resource_id: str, required: bool = False
) -> dict[str, Any] | None:
    if resource_root is None:
        if required:
            raise StoryGenerationError(f"Missing required {resource_type} resource '{resource_id}'")
        return None
    for path in _resource_candidates(resource_root, resource_type, resource_id):
        if not path.exists():
            continue
        try:
            payload = json.loads(path.read_text(encoding="utf-8"))
        except json.JSONDecodeError as exc:
            raise StoryGenerationError(f"Invalid JSON resource {path}: {exc}") from exc
        if not isinstance(payload, dict):
            raise StoryGenerationError(f"Invalid resource type in {path}: expected object")
        return payload
    if required:
        raise StoryGenerationError(f"Missing required {resource_type} resource '{resource_id}'")
    return None


def _merge_app_payload(source_payload: dict[str, Any] | None, binding: dict[str, Any]) -> dict[str, Any]:
    merged: dict[str, Any] = dict(source_payload or {})
    merged["id"] = binding["id"]
    merged["app"] = binding["app"]

    binding_config = binding.get("config")
    if binding_config is not None:
        existing_config = merged.get("config")
        if isinstance(existing_config, dict):
            config = dict(existing_config)
        else:
            config = {}
        config.update(binding_config)
        merged["config"] = config
    return merged


def _with_resource_id(payload: dict[str, Any] | None, resource_id: str) -> dict[str, Any]:
    result = dict(payload or {})
    result["id"] = resource_id
    return result


def _scene_profile(scene_id: str) -> dict[str, Any]:
    profile = SCENE_PROFILES.get(scene_id, DEFAULT_SCENE_PROFILE)
    return copy.deepcopy(profile)


def _as_positive_int(value: Any, default_value: int) -> int:
    try:
        parsed = int(value)
    except (TypeError, ValueError):
        return default_value
    if parsed < 0:
        return 0
    return parsed


def _as_int_in_range(value: Any, default_value: int, min_value: int, max_value: int) -> int:
    try:
        parsed = int(value)
    except (TypeError, ValueError):
        parsed = default_value
    if parsed < min_value:
        return min_value
    if parsed > max_value:
        return max_value
    return parsed


def _as_bool(value: Any, default_value: bool) -> bool:
    if isinstance(value, bool):
        return value
    if isinstance(value, str):
        normalized = value.strip().lower()
        if normalized in {"1", "true", "yes", "on"}:
            return True
        if normalized in {"0", "false", "no", "off"}:
            return False
    return default_value


def _as_choice(value: Any, allowed: set[str], default_value: str) -> str:
    if isinstance(value, str):
        normalized = value.strip().lower().replace("-", "_")
        if normalized in allowed:
            return normalized
    return default_value


def _normalize_screen_effect(value: Any, default_value: str) -> str:
    if isinstance(value, str):
        normalized = value.strip().lower().replace("-", "_")
        normalized = SCREEN_EFFECT_ALIASES.get(normalized, normalized)
        if normalized in SCREEN_EFFECT_CHOICES:
            return normalized
        if any(token in normalized for token in ("scan", "radar", "wave", "sonar")):
            return "scan"
        return "pulse"
    return _as_choice(default_value, SCREEN_EFFECT_CHOICES, "pulse")


def _normalize_transition_effect(value: Any, default_value: str) -> str:
    if isinstance(value, str):
        normalized = value.strip().lower().replace("-", "_")
        normalized = TRANSITION_EFFECT_ALIASES.get(normalized, normalized)
        if normalized in TRANSITION_EFFECT_CHOICES:
            return normalized
    return _as_choice(default_value, TRANSITION_EFFECT_CHOICES, "fade")


def _normalize_theme(theme_payload: Any, fallback_theme: dict[str, str]) -> dict[str, str]:
    theme: dict[str, str] = {
        "bg": fallback_theme["bg"],
        "accent": fallback_theme["accent"],
        "text": fallback_theme["text"],
    }
    if isinstance(theme_payload, dict):
        for key in ("bg", "accent", "text"):
            value = theme_payload.get(key)
            if isinstance(value, str) and value.strip():
                theme[key] = value.strip()
    return theme


def _normalize_text_options(payload: Any, profile: dict[str, Any]) -> dict[str, Any]:
    base = dict(DEFAULT_TEXT_OPTIONS)
    profile_options = profile.get("text")
    if isinstance(profile_options, dict):
        base.update(profile_options)
    src = payload if isinstance(payload, dict) else {}

    return {
        "show_title": _as_bool(src.get("show_title"), _as_bool(base.get("show_title"), False)),
        "show_subtitle": _as_bool(src.get("show_subtitle"), _as_bool(base.get("show_subtitle"), True)),
        "show_symbol": _as_bool(src.get("show_symbol"), _as_bool(base.get("show_symbol"), True)),
        "title_case": _as_choice(
            src.get("title_case"),
            TEXT_CASE_CHOICES,
            _as_choice(base.get("title_case"), TEXT_CASE_CHOICES, "upper"),
        ),
        "subtitle_case": _as_choice(
            src.get("subtitle_case"),
            TEXT_CASE_CHOICES,
            _as_choice(base.get("subtitle_case"), TEXT_CASE_CHOICES, "raw"),
        ),
        "title_align": _as_choice(
            src.get("title_align"),
            TEXT_ALIGN_CHOICES,
            _as_choice(base.get("title_align"), TEXT_ALIGN_CHOICES, "top"),
        ),
        "subtitle_align": _as_choice(
            src.get("subtitle_align"),
            TEXT_ALIGN_CHOICES,
            _as_choice(base.get("subtitle_align"), TEXT_ALIGN_CHOICES, "bottom"),
        ),
    }


def _normalize_framing_options(payload: Any, profile: dict[str, Any]) -> dict[str, Any]:
    base = dict(DEFAULT_FRAMING_OPTIONS)
    profile_options = profile.get("framing")
    if isinstance(profile_options, dict):
        base.update(profile_options)
    src = payload if isinstance(payload, dict) else {}

    return {
        "preset": _as_choice(
            src.get("preset"),
            FRAMING_PRESET_CHOICES,
            _as_choice(base.get("preset"), FRAMING_PRESET_CHOICES, "center"),
        ),
        "x_offset": _as_int_in_range(src.get("x_offset"), int(base.get("x_offset", 0)), -80, 80),
        "y_offset": _as_int_in_range(src.get("y_offset"), int(base.get("y_offset", 0)), -80, 80),
        "scale_pct": _as_int_in_range(src.get("scale_pct"), int(base.get("scale_pct", 100)), 60, 140),
    }


def _normalize_scroll_options(payload: Any, profile: dict[str, Any]) -> dict[str, Any]:
    base = dict(DEFAULT_SCROLL_OPTIONS)
    profile_options = profile.get("scroll")
    if isinstance(profile_options, dict):
        base.update(profile_options)
    src = payload if isinstance(payload, dict) else {}

    mode_value = src.get("mode")
    if isinstance(mode_value, str):
        normalized_mode = mode_value.strip().lower().replace("-", "_")
        mode_value = SCROLL_MODE_ALIASES.get(normalized_mode, normalized_mode)
    mode = _as_choice(mode_value, SCROLL_MODE_CHOICES, _as_choice(base.get("mode"), SCROLL_MODE_CHOICES, "none"))
    return {
        "mode": mode,
        "speed_ms": _as_int_in_range(src.get("speed_ms"), int(base.get("speed_ms", 4200)), 600, 20000),
        "pause_ms": _as_int_in_range(src.get("pause_ms"), int(base.get("pause_ms", 900)), 0, 10000),
        "loop": _as_bool(src.get("loop"), _as_bool(base.get("loop"), True)),
    }


def _normalize_demo_options(payload: Any, profile: dict[str, Any]) -> dict[str, Any]:
    base = dict(DEFAULT_DEMO_OPTIONS)
    profile_options = profile.get("demo")
    if isinstance(profile_options, dict):
        base.update(profile_options)
    src = payload if isinstance(payload, dict) else {}

    return {
        "mode": _as_choice(
            src.get("mode"),
            DEMO_MODE_CHOICES,
            _as_choice(base.get("mode"), DEMO_MODE_CHOICES, "standard"),
        ),
        "particle_count": _as_int_in_range(src.get("particle_count"), int(base.get("particle_count", 4)), 0, 4),
        "strobe_level": _as_int_in_range(src.get("strobe_level"), int(base.get("strobe_level", 65)), 0, 100),
    }


def _normalize_screen_timeline(payload: dict[str, Any], profile: dict[str, Any]) -> dict[str, Any]:
    timeline_source = payload.get("timeline")
    visual_source = payload.get("visual")
    timeline_obj: dict[str, Any] | None = None
    if isinstance(timeline_source, dict):
        timeline_obj = timeline_source
    elif isinstance(visual_source, dict) and isinstance(visual_source.get("timeline"), dict):
        timeline_obj = visual_source.get("timeline")

    timeline_nodes: list[Any] = []
    if isinstance(timeline_source, list):
        timeline_nodes = list(timeline_source)
    elif isinstance(timeline_obj, dict):
        keyframes = timeline_obj.get("keyframes")
        if isinstance(keyframes, list):
            timeline_nodes = list(keyframes)
        elif isinstance(timeline_obj.get("frames"), list):
            timeline_nodes = list(timeline_obj.get("frames"))
    elif isinstance(visual_source, dict) and isinstance(visual_source.get("timeline"), list):
        timeline_nodes = list(visual_source.get("timeline"))

    default_frames = profile["timeline"]
    base_theme = _normalize_theme(payload.get("theme"), profile["theme"])
    base_effect = _normalize_screen_effect(payload.get("effect"), str(profile["effect"]))
    visual = payload.get("visual")
    if isinstance(visual, dict):
        base_speed = _as_positive_int(visual.get("effect_speed_ms"), profile["effect_speed_ms"])
    else:
        base_speed = profile["effect_speed_ms"]
    base_speed = _as_positive_int(payload.get("effect_speed_ms"), base_speed)
    if base_speed <= 0:
        base_speed = profile["effect_speed_ms"]

    normalized_frames: list[dict[str, Any]] = []
    if not timeline_nodes:
        timeline_nodes = copy.deepcopy(default_frames)

    prev_at = 0
    prev_effect = base_effect
    prev_speed = base_speed
    prev_theme = dict(base_theme)
    for frame in timeline_nodes:
        if not isinstance(frame, dict):
            continue
        at_value = frame.get("at_ms", frame.get("time_ms", frame.get("t", prev_at)))
        at_ms = _as_positive_int(at_value, prev_at)
        if at_ms < prev_at:
            at_ms = prev_at

        effect = _normalize_screen_effect(frame.get("effect", frame.get("fx", prev_effect)), prev_effect)

        speed_value = frame.get("speed_ms", frame.get("effect_speed_ms", frame.get("speed", prev_speed)))
        speed_ms = _as_positive_int(speed_value, prev_speed)
        if speed_ms <= 0:
            speed_ms = prev_speed

        theme_payload = frame.get("theme")
        if not isinstance(theme_payload, dict):
            theme_payload = {
                "bg": frame.get("bg"),
                "accent": frame.get("accent"),
                "text": frame.get("text"),
            }
        theme = _normalize_theme(theme_payload, prev_theme)

        normalized_frames.append(
            {
                "at_ms": at_ms,
                "effect": effect,
                "speed_ms": speed_ms,
                "theme": theme,
            }
        )
        prev_at = at_ms
        prev_effect = effect
        prev_speed = speed_ms
        prev_theme = dict(theme)

    if not normalized_frames:
        normalized_frames = copy.deepcopy(default_frames)

    if normalized_frames[0]["at_ms"] != 0:
        first = copy.deepcopy(normalized_frames[0])
        first["at_ms"] = 0
        normalized_frames.insert(0, first)

    if len(normalized_frames) == 1:
        extra = copy.deepcopy(normalized_frames[0])
        extra["at_ms"] = max(1000, normalized_frames[0]["at_ms"] + 1000)
        normalized_frames.append(extra)

    duration_ms = normalized_frames[-1]["at_ms"]
    if timeline_obj is not None:
        duration_ms = max(duration_ms, _as_positive_int(timeline_obj.get("duration_ms"), duration_ms))
    if duration_ms < 100:
        duration_ms = 100

    loop = True
    if timeline_obj is not None and isinstance(timeline_obj.get("loop"), bool):
        loop = bool(timeline_obj.get("loop"))

    return {
        "loop": loop,
        "duration_ms": duration_ms,
        "keyframes": normalized_frames,
    }


def _normalize_screen_payload(source_payload: dict[str, Any] | None, scene_id: str) -> dict[str, Any]:
    payload: dict[str, Any] = dict(source_payload or {})
    payload["id"] = scene_id

    profile = _scene_profile(scene_id)

    for text_field in ("title", "subtitle", "symbol", "effect"):
        value = payload.get(text_field)
        if not isinstance(value, str) or not value.strip():
            payload[text_field] = profile[text_field]
    payload["effect"] = _normalize_screen_effect(payload.get("effect"), str(profile["effect"]))

    text_options = _normalize_text_options(payload.get("text"), profile)
    payload["text"] = text_options

    visual = payload.get("visual")
    if not isinstance(visual, dict):
        visual = {}
    visual["show_title"] = text_options["show_title"]
    visual["show_subtitle"] = text_options["show_subtitle"]
    visual["show_symbol"] = text_options["show_symbol"]
    visual["effect_speed_ms"] = _as_positive_int(visual.get("effect_speed_ms"), profile["effect_speed_ms"])
    if visual["effect_speed_ms"] <= 0:
        visual["effect_speed_ms"] = profile["effect_speed_ms"]
    payload["visual"] = visual

    payload["theme"] = _normalize_theme(payload.get("theme"), profile["theme"])
    payload["framing"] = _normalize_framing_options(payload.get("framing"), profile)
    payload["scroll"] = _normalize_scroll_options(payload.get("scroll"), profile)
    payload["demo"] = _normalize_demo_options(payload.get("demo"), profile)

    transition = payload.get("transition")
    if isinstance(transition, str):
        transition = {"effect": transition}
    if not isinstance(transition, dict):
        transition = {}
    default_transition = profile["transition"]
    effect = transition.get("effect", transition.get("type", default_transition["effect"]))
    transition["effect"] = _normalize_transition_effect(effect, str(default_transition["effect"]))
    transition["duration_ms"] = _as_positive_int(transition.get("duration_ms"), default_transition["duration_ms"])
    if transition["duration_ms"] <= 0:
        transition["duration_ms"] = default_transition["duration_ms"]
    payload["transition"] = transition

    payload["timeline"] = _normalize_screen_timeline(payload, profile)
    return payload


def generate_bundle_files(
    scenarios: list[dict[str, Any]],
    out_dir: Path,
    spec_hash: str,
    resource_root: Path | None = None,
) -> None:
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
        binding = bindings[app_id]
        source_payload = _load_resource_payload(resource_root, "apps", app_id)
        app_payload = _merge_app_payload(source_payload, binding)
        _write_json_with_checksum(out_dir, f"story/apps/{app_id}.json", app_payload)

    for screen_id in sorted(resources["screens"]):
        source_payload = _load_resource_payload(resource_root, "screens", screen_id, required=True)
        payload = _normalize_screen_payload(source_payload, screen_id)
        _write_json_with_checksum(out_dir, f"story/screens/{screen_id}.json", payload)

    for audio_id in sorted(resources["audio"]):
        source_payload = _load_resource_payload(resource_root, "audio", audio_id)
        payload = _with_resource_id(source_payload, audio_id)
        _write_json_with_checksum(out_dir, f"story/audio/{audio_id}.json", payload)

    for action_id in sorted(resources["actions"]):
        source_payload = _load_resource_payload(resource_root, "actions", action_id)
        payload = _with_resource_id(source_payload, action_id)
        _write_json_with_checksum(out_dir, f"story/actions/{action_id}.json", payload)

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

    resource_root = paths.story_data_dir if paths.story_data_dir.exists() else None
    generate_bundle_files(scenarios, bundle_out, spec_hash, resource_root=resource_root)

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
