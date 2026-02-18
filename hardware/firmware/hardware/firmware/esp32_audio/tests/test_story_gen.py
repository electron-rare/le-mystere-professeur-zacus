import argparse
import sys
import tarfile
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.append(str(ROOT / "tools" / "story_gen"))

import story_gen  # type: ignore


def write_yaml(path: Path, content: str) -> None:
    path.write_text(content, encoding="utf-8")


VALID_YAML = """
id: DEFAULT
version: 2
initial_step: STEP_A
estimated_duration_s: 120

app_bindings:
  - id: APP_AUDIO
    app: AUDIO_PACK
  - id: APP_SCREEN
    app: SCREEN_SCENE

steps:
  - step_id: STEP_A
    screen_scene_id: SCENE_LOCKED
    audio_pack_id: PACK_INTRO
    actions:
      - ACTION_TRACE_STEP
    apps:
      - APP_AUDIO
      - APP_SCREEN
    mp3_gate_open: false
    transitions:
      - trigger: on_event
        event_type: unlock
        event_name: UNLOCK
        target_step_id: STEP_B
        after_ms: 0
        priority: 100

  - step_id: STEP_B
    screen_scene_id: SCENE_WIN
    audio_pack_id: ""
    actions: []
    apps:
      - APP_SCREEN
    mp3_gate_open: true
    transitions: []
"""


class StoryGenTests(unittest.TestCase):
    def test_validate_yaml_valid(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            spec_dir = Path(tmp)
            write_yaml(spec_dir / "valid.yaml", VALID_YAML)
            args = argparse.Namespace(spec_dir=str(spec_dir), strict=True)
            exit_code = story_gen.cmd_validate(args)
            self.assertEqual(exit_code, 0)

    def test_validate_yaml_invalid(self) -> None:
        invalid_yaml = """
id: BROKEN
version: "bad"
initial_step: STEP_A
app_bindings: []
steps: []
"""
        with tempfile.TemporaryDirectory() as tmp:
            spec_dir = Path(tmp)
            write_yaml(spec_dir / "invalid.yaml", invalid_yaml)
            args = argparse.Namespace(spec_dir=str(spec_dir), strict=True)
            exit_code = story_gen.cmd_validate(args)
            self.assertNotEqual(exit_code, 0)

    def test_generate_json_deterministic(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            spec_dir = Path(tmp) / "specs"
            spec_dir.mkdir()
            write_yaml(spec_dir / "valid.yaml", VALID_YAML)
            scenarios, issues, _ = story_gen.collect_scenarios(spec_dir, strict=True)
            self.assertFalse(issues)

            root_a = Path(tmp) / "out_a"
            root_b = Path(tmp) / "out_b"
            story_gen.build_story_fs(root_a, scenarios)
            story_gen.build_story_fs(root_b, scenarios)

            file_a = (root_a / "story" / "scenarios" / "DEFAULT.json").read_bytes()
            file_b = (root_b / "story" / "scenarios" / "DEFAULT.json").read_bytes()
            self.assertEqual(file_a, file_b)

    def test_checksum_mismatch(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            spec_dir = Path(tmp) / "specs"
            spec_dir.mkdir()
            write_yaml(spec_dir / "valid.yaml", VALID_YAML)
            scenarios, issues, _ = story_gen.collect_scenarios(spec_dir, strict=True)
            self.assertFalse(issues)

            out_root = Path(tmp) / "out"
            story_gen.build_story_fs(out_root, scenarios)
            json_path = out_root / "story" / "scenarios" / "DEFAULT.json"
            checksum_path = json_path.with_suffix(".sha256")

            original = json_path.read_bytes()
            expected = checksum_path.read_text(encoding="utf-8").strip()
            self.assertEqual(story_gen.compute_sha256_hex(original), expected)

            json_path.write_bytes(original + b" ")
            mutated = json_path.read_bytes()
            self.assertNotEqual(story_gen.compute_sha256_hex(mutated), expected)

    def test_deploy_tar_creation(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            spec_dir = Path(tmp) / "specs"
            spec_dir.mkdir()
            write_yaml(spec_dir / "valid.yaml", VALID_YAML)
            out_dir = Path(tmp) / "out"
            archive = out_dir / "story_deploy.tar.gz"

            args = argparse.Namespace(
                spec_dir=str(spec_dir),
                out_dir=str(out_dir),
                archive=str(archive),
                scenario_id="",
                strict=True,
              port="",
              baud=115200,
            )
            exit_code = story_gen.cmd_deploy(args)
            self.assertEqual(exit_code, 0)
            self.assertTrue(archive.exists())

            with tarfile.open(archive, "r:gz") as tar:
                names = sorted(tar.getnames())

            self.assertIn("story/scenarios/DEFAULT.json", names)
            self.assertIn("story/scenarios/DEFAULT.sha256", names)
            self.assertIn("story/apps/APP_AUDIO.json", names)
            self.assertIn("story/apps/APP_AUDIO.sha256", names)
            self.assertIn("story/screens/SCENE_LOCKED.json", names)
            self.assertIn("story/audio/PACK_INTRO.json", names)
            self.assertIn("story/actions/ACTION_TRACE_STEP.json", names)


if __name__ == "__main__":
    unittest.main()
