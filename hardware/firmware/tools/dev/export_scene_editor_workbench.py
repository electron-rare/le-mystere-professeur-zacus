#!/usr/bin/env python3
"""Export/import a single YAML workbench for scene screen LVGL/FX/audio tuning."""

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path
from typing import Any

import yaml

_DUPLICATE_SUFFIX_RE = re.compile(r" \d+\.[^.]+$")


def _looks_like_duplicate_copy(path: Path) -> bool:
    return _DUPLICATE_SUFFIX_RE.search(path.name) is not None


def _load_yaml(path: Path) -> dict[str, Any]:
    with path.open("r", encoding="utf-8") as fh:
        data = yaml.safe_load(fh) or {}
    if not isinstance(data, dict):
        raise ValueError(f"Invalid YAML root in {path}")
    return data


def _load_json(path: Path) -> dict[str, Any]:
    with path.open("r", encoding="utf-8") as fh:
        data = json.load(fh)
    if not isinstance(data, dict):
        raise ValueError(f"Invalid JSON root in {path}")
    return data


def _scene_audio_index(scenario_path: Path) -> dict[str, dict[str, Any]]:
    scenario = _load_yaml(scenario_path)
    steps = scenario.get("steps", [])
    out: dict[str, dict[str, Any]] = {}
    if not isinstance(steps, list):
        return out
    for step in steps:
        if not isinstance(step, dict):
            continue
        scene = str(step.get("screen_scene_id", "") or "").strip()
        if not scene:
            continue
        bucket = out.setdefault(scene, {"default_audio_pack_id": "", "runtime_step_ids": []})
        audio = str(step.get("audio_pack_id", "") or "").strip()
        if audio and not bucket["default_audio_pack_id"]:
            bucket["default_audio_pack_id"] = audio
        step_id = str(step.get("step_id", "") or "").strip()
        if step_id:
            bucket["runtime_step_ids"].append(step_id)
    return out


def _scenario_scene_order(scenario_path: Path) -> list[str]:
    scenario = _load_yaml(scenario_path)
    steps = scenario.get("steps", [])
    order: list[str] = []
    if not isinstance(steps, list):
        return order
    for step in steps:
        if not isinstance(step, dict):
            continue
        scene = str(step.get("screen_scene_id", "") or "").strip()
        if scene and scene not in order:
            order.append(scene)
    return order


def export_workbench(repo_root: Path, out_path: Path, scenario_path: Path, include_unused: bool) -> None:
    screens_dir = repo_root / "hardware" / "firmware" / "data" / "story" / "screens"
    if not screens_dir.exists():
        raise FileNotFoundError(f"Missing directory: {screens_dir}")

    scene_audio = _scene_audio_index(scenario_path)
    scene_order = _scenario_scene_order(scenario_path)
    entries_by_scene: dict[str, dict[str, Any]] = {}
    all_scene_ids: list[str] = []
    ignored_duplicate_files: list[str] = []
    for json_path in sorted(screens_dir.glob("*.json")):
        if _looks_like_duplicate_copy(json_path):
            ignored_duplicate_files.append(str(json_path.relative_to(repo_root)).replace("\\", "/"))
            continue
        payload = _load_json(json_path)
        scene_id = str(payload.get("id", json_path.stem)).strip()
        if scene_id:
            all_scene_ids.append(scene_id)
        audio_info = scene_audio.get(scene_id, {"default_audio_pack_id": "", "runtime_step_ids": []})
        entry = {
            "scene_id": scene_id,
            "screen_json": str(json_path.relative_to(repo_root)).replace("\\", "/"),
            "default_audio_pack_id": audio_info["default_audio_pack_id"],
            "runtime_step_ids": audio_info["runtime_step_ids"],
            "lvgl": {
                "title": payload.get("title", ""),
                "subtitle": payload.get("subtitle", ""),
                "symbol": payload.get("symbol", ""),
                "visual": payload.get("visual", {}),
                "theme": payload.get("theme", {}),
                "transition": payload.get("transition", {}),
            },
            "fx": {
                "effect": payload.get("effect", "none"),
                "timeline": payload.get("timeline", {}),
            },
            "render": payload.get("render", {}),
            "hardware": payload.get("hardware", {}),
        }
        entries_by_scene[scene_id] = entry

    entries: list[dict[str, Any]] = []
    missing_screen_ids: list[str] = []
    for scene_id in scene_order:
        entry = entries_by_scene.get(scene_id)
        if entry is None:
            missing_screen_ids.append(scene_id)
            continue
        entries.append(entry)

    unused_scene_ids: list[str] = []
    if include_unused:
        for scene_id in sorted(all_scene_ids):
            if scene_id in scene_order:
                continue
            entry = entries_by_scene.get(scene_id)
            if entry is not None:
                entries.append(entry)
                unused_scene_ids.append(scene_id)
    else:
        for scene_id in sorted(all_scene_ids):
            if scene_id not in scene_order:
                unused_scene_ids.append(scene_id)

    out = {
        "meta": {
            "purpose": "Edit LVGL/FX/audio mapping scene by scene, then sync to screen JSON.",
            "source_runtime_scenario": str(scenario_path.relative_to(repo_root)).replace("\\", "/"),
            "scope": "runtime_plus_unused" if include_unused else "runtime_only",
            "used_scene_count": len(scene_order),
            "exported_scene_count": len(entries),
            "unused_scene_ids": unused_scene_ids,
            "missing_screen_ids": missing_screen_ids,
            "ignored_duplicate_files": ignored_duplicate_files,
        },
        "scenes": entries,
    }
    out_path.parent.mkdir(parents=True, exist_ok=True)
    with out_path.open("w", encoding="utf-8") as fh:
        yaml.safe_dump(out, fh, sort_keys=False, allow_unicode=False, width=120)


def apply_workbench(repo_root: Path, in_path: Path) -> None:
    wb = _load_yaml(in_path)
    scenes = wb.get("scenes", [])
    if not isinstance(scenes, list):
        raise ValueError("workbench.scenes must be a list")
    for entry in scenes:
        if not isinstance(entry, dict):
            continue
        rel = str(entry.get("screen_json", "") or "").strip()
        if not rel:
            continue
        if _looks_like_duplicate_copy(Path(rel)):
            continue
        screen_path = repo_root / rel
        if not screen_path.exists():
            continue
        payload = _load_json(screen_path)
        lvgl = entry.get("lvgl", {})
        fx = entry.get("fx", {})
        if isinstance(lvgl, dict):
            for key in ("title", "subtitle", "symbol", "visual", "theme", "transition"):
                if key in lvgl:
                    payload[key] = lvgl[key]
            visual = lvgl.get("visual")
            if isinstance(visual, dict):
                # Keep text visibility flags aligned with visual flags to avoid runtime drift.
                text_cfg = payload.get("text")
                if not isinstance(text_cfg, dict):
                    text_cfg = {}
                for flag in ("show_title", "show_subtitle", "show_symbol"):
                    value = visual.get(flag)
                    if isinstance(value, bool):
                        text_cfg[flag] = value
                payload["text"] = text_cfg
        if isinstance(fx, dict):
            if "effect" in fx:
                payload["effect"] = fx["effect"]
            if "timeline" in fx:
                payload["timeline"] = fx["timeline"]
        render_cfg = entry.get("render")
        if isinstance(render_cfg, dict):
            payload["render"] = render_cfg
        hardware_cfg = entry.get("hardware")
        if isinstance(hardware_cfg, dict):
            payload["hardware"] = hardware_cfg
        with screen_path.open("w", encoding="utf-8") as fh:
            json.dump(payload, fh, indent=2, ensure_ascii=True)
            fh.write("\n")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo-root", type=Path, default=Path(__file__).resolve().parents[4])
    parser.add_argument("--workbench", type=Path, default=None)
    parser.add_argument("--scenario", type=Path, default=None)
    parser.add_argument(
        "--include-unused",
        action="store_true",
        help="Append scenes that are not referenced by scenario steps.",
    )
    parser.add_argument("--apply", action="store_true", help="Apply workbench changes back to JSON files.")
    args = parser.parse_args()

    repo_root = args.repo_root.resolve()
    workbench = args.workbench or (repo_root / "game" / "scenarios" / "scene_editor_all.yaml")
    scenario = args.scenario or (repo_root / "game" / "scenarios" / "default_unlock_win_etape2.yaml")
    if args.apply:
        apply_workbench(repo_root, workbench.resolve())
    else:
        export_workbench(repo_root, workbench.resolve(), scenario.resolve(), args.include_unused)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
