from __future__ import annotations

import json
from pathlib import Path

from zacus_story_gen_ai.generator import StoryPaths, run_generate_bundle, run_generate_cpp, run_validate


def _write(path: Path, content: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding="utf-8")


def _paths(tmp_path: Path) -> StoryPaths:
    fw = tmp_path / "hardware" / "firmware"
    repo = tmp_path
    return StoryPaths(
        fw_root=fw,
        repo_root=repo,
        game_scenarios_dir=repo / "game" / "scenarios",
        story_specs_dir=fw / "docs" / "protocols" / "story_specs" / "scenarios",
        story_data_dir=fw / "data" / "story",
        generated_cpp_dir=fw / "hardware" / "libs" / "story" / "src" / "generated",
        bundle_root=fw / "artifacts" / "story_fs" / "deploy",
    )


def _seed(tmp_path: Path) -> StoryPaths:
    paths = _paths(tmp_path)
    _write(
        paths.game_scenarios_dir / "zacus_v1.yaml",
        """
id: zacus_v1
version: 1
title: Demo
theme: Demo
players: {min: 6, max: 10}
ages: 8+
duration_minutes: {min: 60, max: 90}
canon:
  timeline:
    - label: L1
      note: N1
stations:
  - name: S1
    focus: F1
    clue: C1
puzzles:
  - id: P1
    type: t
    clue: c
    effect: e
solution:
  culprit: X
  motive: M
  method: D
  proof: [a, b, c]
solution_unique: true
""".strip(),
    )
    _write(
        paths.story_specs_dir / "default.yaml",
        """
id: DEFAULT
version: 2
initial_step: STEP_WAIT_UNLOCK
app_bindings:
  - id: APP_SCREEN
    app: SCREEN_SCENE
  - id: APP_GATE
    app: MP3_GATE
steps:
  - step_id: STEP_WAIT_UNLOCK
    screen_scene_id: SCENE_LOCKED
    audio_pack_id: ""
    actions: [ACTION_TRACE_STEP]
    apps: [APP_SCREEN, APP_GATE]
    mp3_gate_open: false
    transitions:
      - trigger: on_event
        event_type: unlock
        event_name: UNLOCK
        target_step_id: STEP_DONE
        after_ms: 0
        priority: 100
  - step_id: STEP_DONE
    screen_scene_id: SCENE_READY
    audio_pack_id: ""
    actions: [ACTION_REFRESH_SD]
    apps: [APP_SCREEN, APP_GATE]
    mp3_gate_open: true
    transitions: []
""".strip(),
    )
    return paths


def test_validate(tmp_path: Path) -> None:
    paths = _seed(tmp_path)
    result = run_validate(paths)
    assert result["scenario_count"] == 1
    assert result["game_scenario_count"] == 1


def test_generate_cpp_and_bundle(tmp_path: Path) -> None:
    paths = _seed(tmp_path)
    cpp = run_generate_cpp(paths)
    bundle = run_generate_bundle(paths)
    assert cpp["spec_hash"]
    assert (paths.generated_cpp_dir / "scenarios_gen.cpp").exists()
    assert (paths.generated_cpp_dir / "apps_gen.cpp").exists()
    assert (paths.bundle_root / "story" / "scenarios" / "DEFAULT.json").exists()
    assert (paths.bundle_root / "story" / "manifest.sha256").exists()
    assert bundle["scenario_count"] == 1


def test_generate_bundle_uses_resource_payloads(tmp_path: Path) -> None:
    paths = _seed(tmp_path)
    _write(paths.story_data_dir / "screens" / "SCENE_LOCKED.json", '{"id":"SCENE_LOCKED","title":"Custom Locked"}')
    _write(
        paths.story_data_dir / "apps" / "APP_SCREEN.json",
        '{"id":"APP_SCREEN","app":"SCREEN_SCENE","config":{"show_title":true,"fps":30}}',
    )
    _write(
        paths.story_data_dir / "actions" / "ACTION_TRACE_STEP.json",
        '{"id":"ACTION_TRACE_STEP","type":"trace_step","config":{"serial_log":false}}',
    )

    run_generate_bundle(paths)

    screen_payload = json.loads((paths.bundle_root / "story" / "screens" / "SCENE_LOCKED.json").read_text())
    app_payload = json.loads((paths.bundle_root / "story" / "apps" / "APP_SCREEN.json").read_text())
    action_payload = json.loads((paths.bundle_root / "story" / "actions" / "ACTION_TRACE_STEP.json").read_text())

    assert screen_payload["title"] == "Custom Locked"
    assert app_payload["app"] == "SCREEN_SCENE"
    assert app_payload["config"]["show_title"] is True
    assert app_payload["config"]["fps"] == 30
    assert action_payload["config"]["serial_log"] is False


def test_generate_bundle_normalizes_screen_timeline_and_transition(tmp_path: Path) -> None:
    paths = _seed(tmp_path)
    _write(paths.story_data_dir / "screens" / "SCENE_READY.json", '{"id":"SCENE_READY","title":"Ready Lite"}')

    run_generate_bundle(paths)

    payload = json.loads((paths.bundle_root / "story" / "screens" / "SCENE_READY.json").read_text())
    assert payload["id"] == "SCENE_READY"
    assert payload["title"] == "Ready Lite"
    assert isinstance(payload["transition"], dict)
    assert payload["transition"]["effect"]
    assert payload["transition"]["duration_ms"] > 0
    assert isinstance(payload["timeline"], dict)
    assert isinstance(payload["timeline"]["keyframes"], list)
    assert len(payload["timeline"]["keyframes"]) >= 2
    assert payload["timeline"]["keyframes"][0]["at_ms"] == 0
    assert isinstance(payload["text"], dict)
    assert isinstance(payload["framing"], dict)
    assert isinstance(payload["scroll"], dict)
    assert isinstance(payload["demo"], dict)


def test_generate_bundle_normalizes_palette_options(tmp_path: Path) -> None:
    paths = _seed(tmp_path)
    _write(
        paths.story_data_dir / "screens" / "SCENE_READY.json",
        """
{
  "id": "SCENE_READY",
  "title": "Ready custom",
  "effect": "reward",
  "text": {
    "show_title": "1",
    "show_subtitle": "true",
    "title_case": "lower",
    "subtitle_align": "center"
  },
  "framing": {
    "preset": "split",
    "x_offset": 120,
    "scale_pct": 20
  },
  "scroll": {
    "mode": "ticker",
    "speed_ms": 120,
    "pause_ms": 12000,
    "loop": "0"
  },
  "demo": {
    "mode": "arcade",
    "particle_count": 9,
    "strobe_level": 180
  },
  "transition": {
    "effect": "crossfade",
    "duration_ms": 0
  }
}
""".strip(),
    )

    run_generate_bundle(paths)

    payload = json.loads((paths.bundle_root / "story" / "screens" / "SCENE_READY.json").read_text())
    assert payload["effect"] == "celebrate"
    assert payload["text"]["show_title"] is True
    assert payload["text"]["show_subtitle"] is True
    assert payload["text"]["title_case"] == "lower"
    assert payload["text"]["subtitle_align"] == "center"
    assert payload["framing"]["preset"] == "split"
    assert payload["framing"]["x_offset"] == 80
    assert payload["framing"]["scale_pct"] == 60
    assert payload["scroll"]["mode"] == "marquee"
    assert payload["scroll"]["speed_ms"] == 600
    assert payload["scroll"]["pause_ms"] == 10000
    assert payload["scroll"]["loop"] is False
    assert payload["demo"]["mode"] == "arcade"
    assert payload["demo"]["particle_count"] == 4
    assert payload["demo"]["strobe_level"] == 100
    assert payload["transition"]["effect"] == "fade"
    assert payload["transition"]["duration_ms"] > 0
