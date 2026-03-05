#!/usr/bin/env python3
"""Generate WAV assets from macOS TTS prompts.

Features:
- Supports global files + per-scene files (`SCENE_ID/1.wav`, etc.)
- Quality profiles (`gentle` / `aggressive`) defined in JSON
- Batch mode (`--batch`) that skips unchanged files by manifest
- Optional FFmpeg post-processing: trim + RMS normalization
- Optional strict post-processing validation (`--validate-ffmpeg`)
- Channel override (`--channels`) and advanced post-processing overrides (trim/normalize/filter parameters)
- Voice listing, dry-run, manifest-only, verification and template generation
"""

from __future__ import annotations

import argparse
import copy
import hashlib
import json
import shutil
import subprocess
import sys
import tempfile
import re
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Dict, Iterable, List, Tuple


DEFAULT_SCENE_IDS = (
    "SCENE_LOCKED",
    "SCENE_LA_DETECTOR",
    "SCENE_WIN_ETAPE",
    "SCENE_LA_OK",
    "SCENE_WINNER",
    "SCENE_FOU_DETECTOR",
    "SCENE_CAM",
)

DEFAULT_PROMPT_FILE = Path(__file__).resolve().parent / "audio_tts_prompts.json"
DEFAULT_MANIFEST_PATH = Path("data/audio/.tts_manifest.json")
MANIFEST_SCHEMA = 2


BUILTIN_PROFILES: Dict[str, Dict[str, Any]] = {
    "gentle": {
        "voice": "Thomas",
        "rate": 170,
        "sample_rate": 16000,
        "post": {
            "enabled": True,
            "trim": True,
            "trim_start_threshold_db": -52.0,
            "trim_start_duration": 0.08,
            "trim_stop_threshold_db": -52.0,
            "trim_stop_duration": 0.12,
            "normalize": True,
            "normalize_mode": "dynaudnorm",
            "normalize_target_rms_db": -19.0,
            "normalize_peak": 0.94,
            "normalize_framelen_ms": 500,
            "normalize_max_gain": 10.0,
            "normalize_compress": 25,
        },
    },
    "aggressive": {
        "voice": "Thomas",
        "rate": 210,
        "sample_rate": 22050,
        "post": {
            "enabled": True,
            "trim": True,
            "trim_start_threshold_db": -46.0,
            "trim_start_duration": 0.04,
            "trim_stop_threshold_db": -46.0,
            "trim_stop_duration": 0.08,
            "normalize": True,
            "normalize_mode": "dynaudnorm",
            "normalize_target_rms_db": -16.0,
            "normalize_peak": 0.97,
            "normalize_framelen_ms": 500,
            "normalize_max_gain": 18.0,
            "normalize_compress": 35,
        },
    },
}


class ConfigError(RuntimeError):
    pass


@dataclass
class PostProcessConfig:
    enabled: bool
    trim: bool
    trim_start_threshold_db: float
    trim_start_duration: float
    trim_stop_threshold_db: float
    trim_stop_duration: float
    normalize: bool
    normalize_mode: str
    normalize_target_rms_db: float
    normalize_peak: float
    normalize_framelen_ms: int
    normalize_max_gain: float
    normalize_compress: int
    normalize_filter: str | None = None

    @classmethod
    def from_mapping(cls, raw: Dict[str, Any]) -> "PostProcessConfig":
        return cls(
            enabled=_coerce_bool(raw.get("enabled", True), True),
            trim=_coerce_bool(raw.get("trim", True), True),
            trim_start_threshold_db=float(raw.get("trim_start_threshold_db", -48.0)),
            trim_start_duration=float(raw.get("trim_start_duration", 0.08)),
            trim_stop_threshold_db=float(raw.get("trim_stop_threshold_db", -48.0)),
            trim_stop_duration=float(raw.get("trim_stop_duration", 0.12)),
            normalize=_coerce_bool(raw.get("normalize", True), True),
            normalize_mode=str(raw.get("normalize_mode", "dynaudnorm")).strip().lower() or "dynaudnorm",
            normalize_target_rms_db=float(raw.get("normalize_target_rms_db", -18.0)),
            normalize_peak=float(raw.get("normalize_peak", 0.95)),
            normalize_framelen_ms=int(raw.get("normalize_framelen_ms", 500)),
            normalize_max_gain=float(raw.get("normalize_max_gain", 10.0)),
            normalize_compress=int(raw.get("normalize_compress", 25)),
            normalize_filter=(
                str(raw.get("normalize_filter")).strip()
                if raw.get("normalize_filter") not in (None, "")
                else None
            ),
        )

    def signature_payload(self) -> Dict[str, Any]:
        return {
            "enabled": self.enabled,
            "trim": self.trim,
            "trim_start_threshold_db": self.trim_start_threshold_db,
            "trim_start_duration": self.trim_start_duration,
            "trim_stop_threshold_db": self.trim_stop_threshold_db,
            "trim_stop_duration": self.trim_stop_duration,
            "normalize": self.normalize,
            "normalize_mode": self.normalize_mode,
            "normalize_target_rms_db": self.normalize_target_rms_db,
            "normalize_peak": self.normalize_peak,
            "normalize_framelen_ms": self.normalize_framelen_ms,
            "normalize_max_gain": self.normalize_max_gain,
            "normalize_compress": self.normalize_compress,
            "normalize_filter": self.normalize_filter,
        }


@dataclass
class TtsTarget:
    text: str
    voice: str
    rate: int
    sample_rate: int
    channels: int
    source: str
    profile: str
    post: PostProcessConfig


def _run(cmd: List[str], check: bool = True, capture: bool = False) -> str | None:
    print(f"[tts] $ {' '.join(cmd)}")
    result = subprocess.run(
        cmd,
        check=check,
        capture_output=capture,
        text=True,
    )
    if capture:
        return result.stdout or ""
    return None


def ensure_tools(require_afconvert: bool = True, require_ffmpeg: bool = False) -> None:
    if shutil.which("say") is None:
        raise ConfigError("Required macOS tool not found in PATH: say")
    if require_afconvert and shutil.which("afconvert") is None:
        raise ConfigError("Required macOS tool not found in PATH: afconvert")
    if require_ffmpeg and shutil.which("ffmpeg") is None:
        raise ConfigError("ffmpeg not found in PATH. Install FFmpeg for RMS/trim post-processing")


def _coerce_bool(value: object, default: bool) -> bool:
    if isinstance(value, bool):
        return value
    if isinstance(value, str):
        return value.strip().lower() in {"1", "true", "yes", "on", "y"}
    if isinstance(value, int):
        return value != 0
    return default


def _deep_merge(base: Dict[str, Any], override: Dict[str, Any]) -> Dict[str, Any]:
    merged = dict(base)
    for key, value in override.items():
        if isinstance(value, dict) and isinstance(merged.get(key), dict):
            merged[key] = _deep_merge(merged[key], value)
        else:
            merged[key] = value
    return merged


def normalize_wav_name(name: str) -> str:
    normalized = (name or "").strip()
    if not normalized:
        raise ConfigError("Audio filename is empty")
    if "/" in normalized:
        raise ConfigError(f"Audio filename must be a file name, not a path: {normalized}")
    if not normalized.lower().endswith(".wav"):
        normalized = f"{normalized}.wav"
    return normalized


def normalize_scene_file_filter(value: str) -> str:
    raw = (value or "").strip()
    if not raw:
        raise ConfigError("File filter token is empty")
    normalized = raw.replace("\\", "/")
    if normalized.endswith("/"):
        raise ConfigError(f"Invalid file filter token: {value}")
    if "/" in normalized:
        scene, filename = normalized.split("/", 1)
        return f"{scene}/{normalize_wav_name(filename)}"
    return normalize_wav_name(normalized)


def parse_file_filters(file_filter: Iterable[str] | None) -> Tuple[set[str], set[str]]:
    if not file_filter:
        return set(), set()

    names: set[str] = set()
    paths: set[str] = set()
    for value in file_filter:
        normalized = normalize_scene_file_filter(value)
        if "/" in normalized:
            paths.add(normalized)
        else:
            names.add(normalized)
    return names, paths


def build_post_overrides_from_args(args: argparse.Namespace) -> Dict[str, Any]:
    overrides: Dict[str, Any] = {}
    if args.post_enabled is not None:
        overrides["enabled"] = args.post_enabled
    if args.trim is not None:
        overrides["trim"] = args.trim
    if args.normalize is not None:
        overrides["normalize"] = args.normalize
    if args.normalize_mode is not None:
        overrides["normalize_mode"] = args.normalize_mode
    if args.normalize_target_rms_db is not None:
        overrides["normalize_target_rms_db"] = args.normalize_target_rms_db
    if args.normalize_peak is not None:
        overrides["normalize_peak"] = args.normalize_peak
    if args.normalize_framelen_ms is not None:
        overrides["normalize_framelen_ms"] = args.normalize_framelen_ms
    if args.normalize_max_gain is not None:
        overrides["normalize_max_gain"] = args.normalize_max_gain
    if args.normalize_compress is not None:
        overrides["normalize_compress"] = args.normalize_compress
    if args.normalize_filter is not None:
        overrides["normalize_filter"] = args.normalize_filter
    if args.trim_start_threshold_db is not None:
        overrides["trim_start_threshold_db"] = args.trim_start_threshold_db
    if args.trim_start_duration is not None:
        overrides["trim_start_duration"] = args.trim_start_duration
    if args.trim_stop_threshold_db is not None:
        overrides["trim_stop_threshold_db"] = args.trim_stop_threshold_db
    if args.trim_stop_duration is not None:
        overrides["trim_stop_duration"] = args.trim_stop_duration
    return overrides


def _parse_say_voice_entries(output: str) -> List[Tuple[str, str]]:
    entries: List[Tuple[str, str]] = []
    for line in output.splitlines():
        line = line.strip()
        if not line:
            continue

        match = re.match(r"^(?P<name>.+?)\s+(?P<locale>[a-z]{2}_[A-Z]{2,3})\s+#", line)
        if not match:
            continue

        raw_name = match.group("name").strip()
        name = raw_name.split(" (", 1)[0].strip()
        locale = match.group("locale")
        if name and locale and (name, locale) not in entries:
            entries.append((name, locale))

    return entries


def _resolve_french_voice(
    requested_voice: str,
    french_voices: List[str],
    fallback_voice: str | None,
    context: str,
) -> str:
    if not french_voices:
        raise ConfigError(f"No French voices available for {context}")

    if requested_voice in french_voices:
        return requested_voice

    fallback = fallback_voice or french_voices[0]
    if requested_voice:
        print(f"[tts] warning: requested voice '{requested_voice}' is not French; fallback to '{fallback}' ({context})")
    return fallback


def filter_voices(voices: List[str], pattern: str | None, *, use_regex: bool = False) -> List[str]:
    if not pattern:
        return voices

    if use_regex:
        import re
        try:
            regex = re.compile(pattern, re.IGNORECASE)
        except re.error as exc:
            raise ConfigError(f"Invalid --voice-filter regular expression: {exc}") from exc
        return [v for v in voices if regex.search(v)]

    lowered = pattern.lower()
    return [v for v in voices if lowered in v.lower()]


def parse_prompts(path: Path) -> Dict:
    if not path.is_file():
        raise ConfigError(f"Prompt file not found: {path}")

    with path.open("r", encoding="utf-8") as f:
        return json.load(f)


def load_manifest(path: Path) -> Dict:
    if not path.exists():
        return {"schema": MANIFEST_SCHEMA, "generated_at": None, "entries": {}}

    with path.open("r", encoding="utf-8") as f:
        data = json.load(f)

    if not isinstance(data, dict) or "entries" not in data or not isinstance(data["entries"], dict):
        raise ConfigError(f"Invalid manifest format: {path}")
    return data


def save_manifest(path: Path, manifest: Dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as f:
        json.dump(manifest, f, indent=2, ensure_ascii=False, sort_keys=True)


def resolve_profiles(
    prompts: Dict,
    default_voice: str,
    default_rate: int,
    default_sample_rate: int,
    default_channels: int,
) -> Dict[str, Dict[str, Any]]:
    user_profiles = prompts.get("profiles", {})
    if not isinstance(user_profiles, dict):
        raise ConfigError("'profiles' section must be an object")

    resolved: Dict[str, Dict[str, Any]] = {}

    for profile_name in ("gentle", "aggressive"):
        base = copy.deepcopy(BUILTIN_PROFILES[profile_name])
        merged = _deep_merge(base, {} if not user_profiles.get(profile_name) else user_profiles[profile_name])
        resolved[profile_name] = merged

    for profile_name, profile_value in user_profiles.items():
        if profile_name in ("gentle", "aggressive"):
            continue
        if not isinstance(profile_value, dict):
            raise ConfigError(f"Profile '{profile_name}' must be an object")
        resolved[profile_name] = _deep_merge(copy.deepcopy(BUILTIN_PROFILES.get("gentle", {})), profile_value)

    defaults = {
        "voice": default_voice,
        "rate": default_rate,
        "sample_rate": default_sample_rate,
        "channels": default_channels,
    }

    for profile_name, profile in resolved.items():
        profile.setdefault("voice", defaults["voice"])
        if profile["voice"] in ("", None):
            profile["voice"] = defaults["voice"]

        profile.setdefault("rate", defaults["rate"])
        profile.setdefault("sample_rate", defaults["sample_rate"])
        if int(profile.get("rate", defaults["rate"])) <= 0:
            profile["rate"] = defaults["rate"]
        if int(profile.get("sample_rate", defaults["sample_rate"])) <= 0:
            profile["sample_rate"] = defaults["sample_rate"]
        if int(profile.get("channels", defaults["channels"])) not in (1, 2):
            profile["channels"] = defaults["channels"]
        profile.setdefault("post", {})

    return resolved


def build_profiled_target(
    entry: object,
    key: str,
    source: str,
    selected_profiles: Dict[str, Dict[str, Any]],
    default_profile: str,
    global_profile_override: str | None,
    voice_override: str | None,
    rate_override: int | None,
    sample_rate_override: int | None,
    channels_override: int | None,
    post_override: Dict[str, Any] | None = None,
) -> Tuple[str, TtsTarget]:
    if not default_profile:
        default_profile = "gentle"
    if default_profile not in selected_profiles:
        raise ConfigError(f"Unknown default profile: {default_profile}")

    if isinstance(entry, str):
        text = entry.strip()
        if not text:
            raise ConfigError(f"Empty prompt text: {key}")
        profile_name = global_profile_override or default_profile
        profile_cfg = selected_profiles.get(profile_name)
        if profile_cfg is None:
            raise ConfigError(f"Unknown profile '{profile_name}' for '{key}'")

        post_cfg = _deep_merge(profile_cfg.get("post", {}), post_override or {})
        post = PostProcessConfig.from_mapping(post_cfg)
        return key, TtsTarget(
            text=str(text),
            voice=voice_override or str(profile_cfg.get("voice", "Thomas")),
            rate=rate_override or int(profile_cfg.get("rate", 180)),
            sample_rate=sample_rate_override or int(profile_cfg.get("sample_rate", 16000)),
            channels=channels_override or int(profile_cfg.get("channels", 1)),
            source=source,
            profile=profile_name,
            post=post,
        )

    if not isinstance(entry, dict):
        raise ConfigError(f"Invalid prompt entry for '{key}': {entry!r}")

    text = str(entry.get("text", "")).strip()
    if not text:
        raise ConfigError(f"Missing text in prompt entry '{key}'")

    profile_name = str(entry.get("profile", global_profile_override or default_profile)).strip() or default_profile
    profile_cfg = selected_profiles.get(profile_name)
    if profile_cfg is None:
        raise ConfigError(f"Unknown profile '{profile_name}' for '{key}'")

    post_cfg = _deep_merge(profile_cfg.get("post", {}), _as_dict(entry.get("post", {})))
    post_cfg = _deep_merge(post_cfg, post_override or {})
    post = PostProcessConfig.from_mapping(post_cfg)

    return key, TtsTarget(
        text=text,
        voice=voice_override or str(entry.get("voice", profile_cfg.get("voice", "Thomas"))),
        rate=rate_override or int(entry.get("rate", profile_cfg.get("rate", 180))),
        sample_rate=sample_rate_override or int(entry.get("sample_rate", profile_cfg.get("sample_rate", 16000))),
        channels=channels_override or int(entry.get("channels", profile_cfg.get("channels", 1))),
        source=source,
        profile=profile_name,
        post=post,
    )


def _as_dict(value: object) -> Dict[str, Any]:
    if not isinstance(value, dict):
        return {}
    return value


def build_tts_targets(
    prompts: Dict,
    scene_filter: Iterable[str] | None = None,
    file_filter: Iterable[str] | None = None,
    voice_override: str | None = None,
    rate_override: int | None = None,
    sample_rate_override: int | None = None,
    channels_override: int | None = None,
    post_override: Dict[str, Any] | None = None,
    profile_override: str | None = None,
) -> Dict[str, TtsTarget]:
    meta = prompts.get("meta", {}) if isinstance(prompts, dict) else {}
    if not isinstance(meta, dict):
        raise ConfigError("'meta' section must be an object")

    default_voice = str(meta.get("default_voice", "Thomas"))
    default_rate = int(meta.get("default_rate", 180))
    default_sample_rate = int(meta.get("default_sample_rate", 16000))
    default_channels = int(meta.get("default_channels", 1))
    default_profile = str(meta.get("default_profile", "gentle")).strip() or "gentle"

    selected_profiles = resolve_profiles(
        prompts,
        default_voice,
        default_rate,
        default_sample_rate,
        default_channels,
    )
    if default_profile not in selected_profiles:
        raise ConfigError(f"Unknown default profile in meta: {default_profile}")

    selected_scenes = set(DEFAULT_SCENE_IDS)
    if scene_filter:
        requested = {scene.strip() for scene in scene_filter if scene.strip()}
        selected_scenes = {s for s in DEFAULT_SCENE_IDS if s in requested}
        unknown = sorted(requested - set(DEFAULT_SCENE_IDS))
        if unknown:
            print(f"[tts] warning: unknown SCENE_ID(s) ignored: {', '.join(unknown)}")

    selected_files, selected_path_filters = parse_file_filters(file_filter)

    global_entries = prompts.get("global", {}) if isinstance(prompts.get("global", {}), dict) else {}
    if not isinstance(global_entries, dict):
        raise ConfigError("'global' section must be an object")

    all_targets: Dict[str, TtsTarget] = {}

    for key, value in global_entries.items():
        wav_name = normalize_wav_name(str(key))
        if (selected_files or selected_path_filters) and wav_name not in selected_files:
            continue

        rel_path, target = build_profiled_target(
            entry=value,
            key=wav_name,
            source="global",
            selected_profiles=selected_profiles,
            default_profile=default_profile,
            global_profile_override=profile_override,
            voice_override=voice_override,
            rate_override=rate_override,
            sample_rate_override=sample_rate_override,
            channels_override=channels_override,
            post_override=post_override or {},
        )
        all_targets[rel_path] = target

    for scene_id, scene_data in prompts.items():
        if scene_id in {"meta", "global", "profiles"}:
            continue
        if not isinstance(scene_data, dict):
            print(f"[tts] warning: scene section ignored (not object): {scene_id}")
            continue
        if scene_id not in selected_scenes:
            continue

        for key, value in scene_data.items():
            fname = normalize_wav_name(str(key))
            rel_path = f"{scene_id}/{fname}"

            if (selected_files or selected_path_filters) and fname not in selected_files and rel_path not in selected_path_filters:
                continue

            _, target = build_profiled_target(
                entry=value,
                key=rel_path,
                source=f"scene:{scene_id}",
                selected_profiles=selected_profiles,
                default_profile=default_profile,
                global_profile_override=profile_override,
                voice_override=voice_override,
                rate_override=rate_override,
                sample_rate_override=sample_rate_override,
                channels_override=channels_override,
                post_override=post_override or {},
            )
            all_targets[rel_path] = target

    return all_targets


def target_signature(target: TtsTarget) -> str:
    payload = {
        "text": target.text,
        "voice": target.voice,
        "rate": target.rate,
        "sample_rate": target.sample_rate,
        "channels": target.channels,
        "profile": target.profile,
        "post": target.post.signature_payload(),
    }
    raw = json.dumps(payload, ensure_ascii=False, sort_keys=True, separators=(",", ":"))
    return hashlib.sha256(raw.encode("utf-8")).hexdigest()


def should_skip_target(manifest: Dict, rel_path: str, output_path: Path, target: TtsTarget, overwrite: bool) -> bool:
    if overwrite:
        return False

    entries = manifest.get("entries", {})
    entry = entries.get(rel_path)
    if not entry or not isinstance(entry, dict):
        return False

    expected_signature = entry.get("signature")
    if expected_signature != target_signature(target):
        return False

    if not output_path.exists() or output_path.stat().st_size <= 0:
        return False

    expected_size = int(entry.get("size", -1))
    return expected_size == output_path.stat().st_size


def requires_post_processing(targets: Dict[str, TtsTarget]) -> bool:
    return any(target.post.enabled and (target.post.trim or target.post.normalize) for target in targets.values())


def _build_trim_filter(post: PostProcessConfig) -> str:
    return (
        "silenceremove="
        f"start_periods=1"
        f":start_duration={post.trim_start_duration}"
        f":start_threshold={post.trim_start_threshold_db}dB"
        f":stop_periods=1"
        f":stop_duration={post.trim_stop_duration}"
        f":stop_threshold={post.trim_stop_threshold_db}dB"
    )


def _build_normalize_filter(post: PostProcessConfig) -> str:
    if post.normalize_filter:
        return post.normalize_filter

    if post.normalize_mode == "dynaudnorm":
        target_rms = 10 ** (post.normalize_target_rms_db / 20.0)
        target_rms = max(0.0001, min(0.9999, target_rms))
        return (
            "dynaudnorm="
            f"f={int(max(10, min(8000, post.normalize_framelen_ms)))}"
            f":g={int(post.normalize_compress)}"
            f":m={post.normalize_max_gain}"
            f":p={max(0.01, min(0.999, post.normalize_peak))}"
            f":r={target_rms:.6f}"
        )

    if post.normalize_mode == "loudnorm":
        return (
            f"loudnorm="
            f"I={post.normalize_target_rms_db}"
            ":TP=-1.5:LRA=11"
        )

    raise ConfigError(f"Unknown normalize mode: {post.normalize_mode}")


def apply_post_process(output_path: Path, target: TtsTarget, profile: PostProcessConfig, use_ffmpeg: bool) -> None:
    if not use_ffmpeg or not profile.enabled:
        return

    if not profile.normalize and not profile.trim:
        return

    if shutil.which("ffmpeg") is None:
        print(f"[tts] ffmpeg not found, skipping post-process: {output_path}")
        return

    filters = []
    if profile.trim:
        filters.append(_build_trim_filter(profile))
    if profile.normalize:
        filters.append(_build_normalize_filter(profile))
    if not filters:
        return

    filter_expr = ",".join(filters)
    tmp = output_path.with_suffix(".post.wav")

    cmd = [
        "ffmpeg",
        "-y",
        "-i",
        str(output_path),
        "-af",
        filter_expr,
        "-ac",
        str(target.channels),
        "-ar",
        str(target.sample_rate),
        "-sample_fmt",
        "s16",
        str(tmp),
    ]

    try:
        _run(cmd)
    except subprocess.CalledProcessError as exc:
        print(f"[tts] warning: ffmpeg post-process failed for {output_path}: {exc}")
        if tmp.exists():
            tmp.unlink()
        return

    tmp.replace(output_path)


def list_voices(
    pattern: str | None = None,
    regex: bool = False,
    *,
    french_only: bool = False,
) -> List[str]:
    output = _run(["say", "-v", "?"], capture=True)
    if output is None:
        return []

    entries = _parse_say_voice_entries(output)
    if french_only:
        entries = [(name, locale) for name, locale in entries if locale.lower().startswith("fr_")]

    voices: List[str] = []
    seen: set[str] = set()
    for name, _ in entries:
        if name and name not in seen:
            voices.append(name)
            seen.add(name)

    if french_only:
        print(f"[tts] available French voices: {len(voices)}")

    if not output:
        return voices

    return filter_voices(voices, pattern, use_regex=regex)


def validate_voice(voice: str, voices: List[str]) -> None:
    if voice and voices and voice not in voices:
        raise ConfigError(f"Unknown voice: {voice}. Run with --list-voices to see available voices")


def build_manifest_only(
    audio_root: Path,
    prompts: Dict,
    scene_filter: Tuple[str, ...] | None,
    file_filter: Tuple[str, ...] | None,
    voice_override: str | None,
    rate_override: int | None,
    sample_rate_override: int | None,
    channels_override: int | None,
    post_override: Dict[str, Any] | None,
    profile_override: str | None,
) -> Dict:
    targets = build_tts_targets(
        prompts=prompts,
        scene_filter=scene_filter,
        file_filter=file_filter,
        voice_override=voice_override,
        rate_override=rate_override,
        sample_rate_override=sample_rate_override,
        channels_override=channels_override,
        post_override=post_override,
        profile_override=profile_override,
    )

    entries = {}
    for rel_path, target in targets.items():
        output_path = audio_root / rel_path
        signature = target_signature(target)
        size = output_path.stat().st_size if output_path.exists() else 0
        if size <= 0:
            continue
        entries[rel_path] = {
            "signature": signature,
            "size": size,
            "source": target.source,
            "voice": target.voice,
            "rate": target.rate,
            "sample_rate": target.sample_rate,
            "channels": target.channels,
            "profile": target.profile,
            "post": target.post.signature_payload(),
            "updated_at": datetime.now(timezone.utc).isoformat(),
        }

    return {
        "schema": MANIFEST_SCHEMA,
        "generated_at": datetime.now(timezone.utc).isoformat(),
        "entries": entries,
    }


def verify_targets(audio_root: Path, targets: Dict[str, TtsTarget], manifest: Dict) -> int:
    errors = 0
    manifest_entries = manifest.get("entries", {})
    if not isinstance(manifest_entries, dict):
        manifest_entries = {}

    for rel_path, target in sorted(targets.items()):
        output_path = audio_root / rel_path
        if not output_path.exists():
            print(f"[tts] missing: {rel_path}")
            errors += 1
            continue
        if output_path.stat().st_size <= 0:
            print(f"[tts] empty: {rel_path}")
            errors += 1
            continue
        signature = target_signature(target)
        manifest_entry = manifest_entries.get(rel_path)
        if not isinstance(manifest_entry, dict):
            print(f"[tts] untracked: {rel_path}")
            errors += 1
            continue
        if manifest_entry.get("signature") != signature:
            print(f"[tts] stale: {rel_path}")
            errors += 1

    if errors:
        print(f"[tts] verify failed: {errors} issues")
    else:
        print(f"[tts] verify ok: {len(targets)} files")
    return errors


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Generate WAV files from macOS TTS prompts.")
    parser.add_argument(
        "--prompts",
        type=Path,
        default=DEFAULT_PROMPT_FILE,
        help=f"JSON prompts file (default: {DEFAULT_PROMPT_FILE})",
    )
    parser.add_argument(
        "--audio-root",
        type=Path,
        default=Path("data/audio"),
        help="Target root directory for generated WAV files (default: data/audio)",
    )
    parser.add_argument(
        "--manifest",
        type=Path,
        default=DEFAULT_MANIFEST_PATH,
        help="Path for generation manifest (default: data/audio/.tts_manifest.json)",
    )
    parser.add_argument("--scene", action="append", default=[], help="Limit to one or more SCENE_ID")
    parser.add_argument(
        "--file",
        action="append",
        default=[],
        help="Limit generation to one or more file names (`1`, `1.wav`, or `SCENE_ID/1.wav`)",
    )
    parser.add_argument("--profile", help="Default profile to force for all entries (e.g. gentle/aggressive)")
    parser.add_argument("--batch", action="store_true", help="Batch mode: skip files unchanged in manifest")
    parser.add_argument("--overwrite", action="store_true", help="Overwrite all generated files")
    parser.add_argument("--dry-run", action="store_true", help="Print planned operations without generating files")
    parser.add_argument("--channels", type=int, default=None, help="Override output channels for all prompts (1=mono, 2=stereo)")
    parser.add_argument("--list-voices", action="store_true", help="List available macOS voices and exit")
    parser.add_argument(
        "--voice-filter",
        default=None,
        help="Filter voices when listing with --list-voices (case-insensitive substring)",
    )
    parser.add_argument(
        "--voice-filter-regex",
        action="store_true",
        help="Interpret --voice-filter as a regular expression",
    )
    parser.add_argument("--voice", default=None, help="Override voice for all prompts")
    parser.add_argument(
        "--french-only",
        action="store_true",
        default=True,
        help="Restrict syntheses to French locale voices (default)",
    )
    parser.add_argument(
        "--allow-non-french",
        action="store_false",
        dest="french_only",
        help="Allow non-French voice selection",
    )
    parser.add_argument("--rate", type=int, default=None, help="Override rate (words-per-minute) for all prompts")
    parser.add_argument("--sample-rate", type=int, default=None, help="Override sample rate (Hz) for generated WAV")
    parser.add_argument(
        "--voice-from-filter",
        action="store_true",
        help="Use first filtered voice from --voice-filter for --voice; errors if no match",
    )
    parser.add_argument(
        "--init-template",
        action="store_true",
        help="Create/overwrite prompt template and exit",
    )
    parser.add_argument(
        "--create-manifest-only",
        action="store_true",
        help="Generate manifest from existing files without re-synthesizing audio",
    )
    parser.add_argument(
        "--verify",
        action="store_true",
        help="Verify expected files exist and match manifest, then exit",
    )
    parser.add_argument(
        "--skip-post",
        action="store_true",
        help="Skip FFmpeg trim/normalization post-processing",
    )
    post_group = parser.add_argument_group("post-processing options (overrides)")
    post_group.add_argument(
        "--post-enable",
        dest="post_enabled",
        action="store_const",
        const=True,
        default=None,
        help="Force post-processing on",
    )
    post_group.add_argument(
        "--post-disable",
        dest="post_enabled",
        action="store_const",
        const=False,
        help="Force post-processing off",
    )
    post_group.add_argument(
        "--trim",
        dest="trim",
        action="store_const",
        const=True,
        default=None,
        help="Force trim on",
    )
    post_group.add_argument(
        "--no-trim",
        dest="trim",
        action="store_const",
        const=False,
        help="Disable trim",
    )
    post_group.add_argument(
        "--normalize",
        dest="normalize",
        action="store_const",
        const=True,
        default=None,
        help="Force RMS/normalization on",
    )
    post_group.add_argument(
        "--no-normalize",
        dest="normalize",
        action="store_const",
        const=False,
        help="Disable RMS/normalization",
    )
    post_group.add_argument(
        "--normalize-mode",
        choices=["dynaudnorm", "loudnorm"],
        default=None,
        help="Post normalizer mode: dynaudnorm (default from profile) or loudnorm",
    )
    post_group.add_argument(
        "--normalize-target-rms-db",
        type=float,
        default=None,
        help="RMS target in dBFS for dynaudnorm/loudnorm",
    )
    post_group.add_argument(
        "--normalize-peak",
        type=float,
        default=None,
        help="Output peak target [0..1]",
    )
    post_group.add_argument(
        "--normalize-framelen-ms",
        type=int,
        default=None,
        help="Dynaudnorm frame length in ms",
    )
    post_group.add_argument(
        "--normalize-max-gain",
        type=float,
        default=None,
        help="Dynaudnorm max gain dB",
    )
    post_group.add_argument(
        "--normalize-compress",
        type=int,
        default=None,
        help="Dynaudnorm compress factor (1..100)",
    )
    post_group.add_argument(
        "--normalize-filter",
        type=str,
        default=None,
        help="Custom ffmpeg normalize filter string (overrides normalize mode)",
    )
    post_group.add_argument(
        "--trim-start-threshold-db",
        type=float,
        default=None,
        help="Trim start silence threshold in dB",
    )
    post_group.add_argument(
        "--trim-start-duration",
        type=float,
        default=None,
        help="Trim start minimum duration in seconds",
    )
    post_group.add_argument(
        "--trim-stop-threshold-db",
        type=float,
        default=None,
        help="Trim stop silence threshold in dB",
    )
    post_group.add_argument(
        "--trim-stop-duration",
        type=float,
        default=None,
        help="Trim stop minimum duration in seconds",
    )
    parser.add_argument(
        "--validate-ffmpeg",
        action="store_true",
        help="Fail early if post-processing is required and ffmpeg is missing",
    )
    return parser


def create_default_prompt_file(path: Path) -> None:
    template = {
        "meta": {
            "default_voice": "Thomas",
            "default_rate": 180,
            "default_sample_rate": 16000,
            "default_channels": 1,
            "default_profile": "gentle",
            "scenes": list(DEFAULT_SCENE_IDS),
        },
        "profiles": {
            "gentle": {
                "voice": "Thomas",
                "rate": 170,
                "sample_rate": 16000,
                "post": {
                    "enabled": True,
                    "trim": True,
                    "trim_start_threshold_db": -52,
                    "trim_start_duration": 0.08,
                    "trim_stop_threshold_db": -52,
                    "trim_stop_duration": 0.12,
                    "normalize": True,
                    "normalize_mode": "dynaudnorm",
                    "normalize_target_rms_db": -19,
                    "normalize_peak": 0.94,
                    "normalize_framelen_ms": 500,
                    "normalize_max_gain": 10,
                    "normalize_compress": 25,
                },
            },
            "aggressive": {
                "voice": "Thomas",
                "rate": 210,
                "sample_rate": 22050,
                "post": {
                    "enabled": True,
                    "trim": True,
                    "trim_start_threshold_db": -46,
                    "trim_start_duration": 0.04,
                    "trim_stop_threshold_db": -46,
                    "trim_stop_duration": 0.08,
                    "normalize": True,
                    "normalize_mode": "dynaudnorm",
                    "normalize_target_rms_db": -16,
                    "normalize_peak": 0.97,
                    "normalize_framelen_ms": 500,
                    "normalize_max_gain": 18,
                    "normalize_compress": 35,
                },
            },
        },
        "global": {
            "welcome.wav": {"text": "Bienvenue. Fichier de test audio.", "voice": "Thomas"},
            "la_ok.wav": {"text": "Message d'acquittement détecté.", "voice": "Amélie"},
            "la_busy.wav": {"text": "Ligne occupée. Veuillez réessayer.", "voice": "Flo"},
            "bip.wav": {"text": "Bip.", "profile": "aggressive", "voice": "Thomas"},
            "souffle.wav": {"text": "Souffle léger.", "profile": "gentle", "voice": "Sandy"},
            "radio.wav": {"text": "Signal radio.", "profile": "gentle", "voice": "Rocko"},
            "musique.wav": {"text": "Musique d'attente.", "profile": "aggressive", "voice": "Reed"},
            "note.wav": {"text": "Note de service.", "profile": "gentle", "voice": "Jacques"},
        },
        "SCENE_LOCKED": {
            "1.wav": {"text": "Scène verrouillée. Étape 1.", "profile": "gentle", "voice": "Thomas"},
            "2.wav": {"text": "Scène verrouillée. Étape 2.", "profile": "aggressive", "voice": "Rocko"},
            "3.wav": {"text": "Scène verrouillée. Étape 3.", "profile": "gentle", "voice": "Sandy"},
        },
        "SCENE_LA_DETECTOR": {
            "1.wav": {"text": "Détecteur LA actif.", "profile": "aggressive", "voice": "Eddy"},
            "2.wav": {"text": "Détecteur LA étape deux.", "profile": "gentle", "voice": "Grandma"},
            "3.wav": {"text": "Détecteur LA terminé.", "profile": "aggressive", "voice": "Rocko"},
        },
        "SCENE_WIN_ETAPE": {
            "1.wav": {"text": "Scène WIN étape 1.", "profile": "aggressive", "voice": "Thomas"},
            "2.wav": {"text": "Scène WIN étape 2.", "profile": "aggressive", "voice": "Amélie"},
            "3.wav": {"text": "Scène WIN étape 3.", "profile": "gentle", "voice": "Reed"},
        },
        "SCENE_LA_OK": {
            "1.wav": {"text": "La détection est validée.", "profile": "gentle", "voice": "Sandy"},
            "2.wav": {"text": "Étape LA OK suivante.", "profile": "gentle", "voice": "Flo"},
            "3.wav": {"text": "Succès LA OK.", "profile": "aggressive", "voice": "Rocko"},
        },
        "SCENE_WINNER": {
            "1.wav": {"text": "Félicitations, vous gagnez.", "profile": "aggressive", "voice": "Jacques"},
            "2.wav": {"text": "Bravo. Prochaine étape.", "profile": "gentle", "voice": "Thomas"},
            "3.wav": {"text": "Fin de la séquence.", "profile": "gentle", "voice": "Sandy"},
        },
        "SCENE_FOU_DETECTOR": {
            "1.wav": {"text": "Détecteur FOU actif.", "profile": "aggressive", "voice": "Rocko"},
            "2.wav": {"text": "Détecteur FOU étape deux.", "profile": "gentle", "voice": "Thomas"},
            "3.wav": {"text": "Détecteur FOU terminé.", "profile": "aggressive", "voice": "Reed"},
        },
        "SCENE_CAM": {
            "1.wav": {"text": "Caméra activée.", "profile": "gentle", "voice": "Jacques"},
            "2.wav": {"text": "Caméra en cours d'analyse.", "profile": "aggressive", "voice": "Thomas"},
            "3.wav": {"text": "Caméra terminée.", "profile": "gentle", "voice": "Amélie"},
        },
    }

    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as f:
        json.dump(template, f, ensure_ascii=False, indent=2, sort_keys=True)
    print(f"[tts] template written: {path}")


def main(argv: List[str] | None = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)

    if args.list_voices:
        try:
            ensure_tools(require_afconvert=False, require_ffmpeg=False)
            voices = list_voices(pattern=args.voice_filter, regex=args.voice_filter_regex, french_only=args.french_only)
        except ConfigError as exc:
            print(f"[tts] error: {exc}", file=sys.stderr)
            return 1
        for voice in voices:
            print(voice)
        return 0

    if args.init_template:
        create_default_prompt_file(args.prompts)
        return 0

    try:
        ensure_tools(require_afconvert=True, require_ffmpeg=False)
        prompts = parse_prompts(args.prompts)

        scene_filter = tuple(args.scene) if args.scene else ()
        file_filter = tuple(args.file) if args.file else ()
        post_override = build_post_overrides_from_args(args)

        if args.rate is not None and args.rate <= 0:
            raise ConfigError("--rate must be a positive integer")
        if args.sample_rate is not None and args.sample_rate <= 0:
            raise ConfigError("--sample-rate must be a positive integer")
        if args.channels is not None and args.channels not in (1, 2):
            raise ConfigError("--channels must be 1 (mono) or 2 (stereo)")
        if args.normalize_framelen_ms is not None and args.normalize_framelen_ms < 10:
            raise ConfigError("--normalize-framelen-ms must be >= 10")
        if args.normalize_compress is not None and not (1 <= args.normalize_compress <= 100):
            raise ConfigError("--normalize-compress must be between 1 and 100")
        if args.normalize_peak is not None and not (0.0 < args.normalize_peak <= 1.0):
            raise ConfigError("--normalize-peak must be in the interval (0, 1]")
        if args.trim_start_duration is not None and args.trim_start_duration <= 0:
            raise ConfigError("--trim-start-duration must be > 0")
        if args.trim_stop_duration is not None and args.trim_stop_duration <= 0:
            raise ConfigError("--trim-stop-duration must be > 0")
        if args.normalize_target_rms_db is not None and args.normalize_target_rms_db > 0:
            raise ConfigError("--normalize-target-rms-db must be <= 0")

        if args.voice_filter and args.voice and args.voice_from_filter:
            raise ConfigError("--voice and --voice-from-filter are mutually exclusive")

        available_voices = list_voices(french_only=args.french_only)
        if args.voice_filter:
            available_voices = filter_voices(available_voices, args.voice_filter, use_regex=args.voice_filter_regex)
            if not available_voices:
                raise ConfigError(f"voice filter '{args.voice_filter}' did not match any voice")
        if args.voice_from_filter:
            args.voice = available_voices[0]
        if args.voice:
            if args.french_only:
                args.voice = _resolve_french_voice(args.voice, available_voices, available_voices[0] if available_voices else None, "--voice")
            else:
                validate_voice(args.voice, available_voices)

        if args.profile:
            meta = prompts.get("meta", {})
            if not isinstance(meta, dict):
                meta = {}
            meta["default_profile"] = args.profile
            prompts["meta"] = meta

        targets = build_tts_targets(
            prompts=prompts,
            scene_filter=scene_filter,
            file_filter=file_filter,
            voice_override=args.voice,
            rate_override=args.rate,
            sample_rate_override=args.sample_rate,
            channels_override=args.channels,
            post_override=post_override,
            profile_override=args.profile,
        )

        if args.french_only:
            french_voices = available_voices
            if not french_voices:
                raise ConfigError("No French voice available on this system for --french-only")
            fallback_voice = french_voices[0]
            for rel_path, target in targets.items():
                target.voice = _resolve_french_voice(
                    target.voice,
                    french_voices,
                    fallback_voice,
                    f"prompt '{rel_path}'",
                )

        if not targets:
            print("[tts] no targets to process")
            return 0

        manifest = load_manifest(args.manifest)
        if args.create_manifest_only:
            manifest = build_manifest_only(
                audio_root=args.audio_root,
                prompts=prompts,
                scene_filter=scene_filter or None,
                file_filter=file_filter or None,
                voice_override=args.voice,
                rate_override=args.rate,
                sample_rate_override=args.sample_rate,
                channels_override=args.channels,
                post_override=post_override,
                profile_override=args.profile,
            )
            save_manifest(args.manifest, manifest)
            print(f"[tts] manifest updated only: {args.manifest}")
            return 0

        if args.verify:
            errors = verify_targets(args.audio_root, targets, manifest)
            return 1 if errors else 0

        if args.dry_run:
            if args.validate_ffmpeg:
                if args.skip_post:
                    print("[tts] ffmpeg validation skipped (post-processing disabled with --skip-post)")
                elif requires_post_processing(targets):
                    ensure_tools(require_afconvert=False, require_ffmpeg=True)
                    print("[tts] ffmpeg available for requested post-processing")
                else:
                    print("[tts] ffmpeg validation skipped (no post-processing requested by current targets)")
            print("[tts] dry-run plan:")
            for rel_path in sorted(targets):
                target = targets[rel_path]
                print(f"- {args.audio_root / rel_path}")
                print(
                    f"  profile={target.profile} voice={target.voice} rate={target.rate} sample_rate={target.sample_rate} "
                    f"channels={target.channels} "
                    f"source={target.source}"
                )
                print(f"  text={target.text!r}")
            return 0

        if args.validate_ffmpeg and not args.skip_post and requires_post_processing(targets):
            ensure_tools(require_afconvert=False, require_ffmpeg=True)

        overwrite = args.overwrite or (not args.batch)

        changed = 0
        kept = 0
        for rel_path in sorted(targets):
            target = targets[rel_path]
            output_path = args.audio_root / rel_path
            skip = should_skip_target(manifest, rel_path, output_path, target, overwrite)

            if skip:
                kept += 1
                continue

            synthesize_to_wav(output_path, target, skip_post=args.skip_post)
            manifest.setdefault("entries", {})[rel_path] = {
                "signature": target_signature(target),
                "size": output_path.stat().st_size,
                "source": target.source,
                "voice": target.voice,
                "rate": target.rate,
                "sample_rate": target.sample_rate,
                "channels": target.channels,
                "profile": target.profile,
                "post": target.post.signature_payload(),
                "updated_at": datetime.now(timezone.utc).isoformat(),
            }
            changed += 1

        manifest["schema"] = MANIFEST_SCHEMA
        manifest["generated_at"] = datetime.now(timezone.utc).isoformat()
        save_manifest(args.manifest, manifest)

        print(f"[tts] done. generated={changed} skipped={kept} total={len(targets)}")
        print(f"[tts] manifest: {args.manifest}")
        return 0
    except (ConfigError, OSError, json.JSONDecodeError, subprocess.CalledProcessError, ValueError) as exc:
        print(f"[tts] error: {exc}", file=sys.stderr)
        return 1


def synthesize_to_wav(output_path: Path, target: TtsTarget, *, skip_post: bool) -> None:
    output_path.parent.mkdir(parents=True, exist_ok=True)

    with tempfile.TemporaryDirectory(prefix="rtcbl_tts_", dir=None) as tmpdir:
        aiff_path = Path(tmpdir) / "speech.aiff"

        _run([
            "say",
            "-v",
            target.voice,
            "-r",
            str(target.rate),
            "-o",
            str(aiff_path),
            target.text,
        ])
        _run([
            "afconvert",
            "-f",
            "WAVE",
            "-d",
            "LEI16",
            "-c",
            str(target.channels),
            "-r",
            str(target.sample_rate),
            str(aiff_path),
            str(output_path),
        ])

    apply_post_process(output_path, target, target.post, use_ffmpeg=not skip_post)
    print(f"[tts] written: {output_path}")


if __name__ == "__main__":
    raise SystemExit(main())
