from __future__ import annotations

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
