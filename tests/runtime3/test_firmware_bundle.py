#!/usr/bin/env python3
"""End-to-end pipeline test: scenario YAML -> Runtime 3 IR -> firmware bundle.

Exercises the script that the atelier output is funneled into for ESP32 OTA.
The atelier produces YAML via Blockly export; this test picks up at the
'YAML on disk' stage and asserts that the firmware-bound JSON is produced
with the contract the firmware expects.
"""

from __future__ import annotations

import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
EXPORT_SCRIPT = REPO_ROOT / "tools" / "scenario" / "export_runtime3_firmware_bundle.py"
CANONICAL_SCENARIOS = [
    REPO_ROOT / "game" / "scenarios" / "zacus_v2.yaml",
    REPO_ROOT / "game" / "scenarios" / "zacus_v3_complete.yaml",
]


class FirmwareBundleExportTests(unittest.TestCase):
    """Run the firmware-bundle export CLI and inspect its JSON output."""

    def _export(self, scenario: Path, out_dir: Path) -> Path:
        out_path = out_dir / "DEFAULT.json"
        result = subprocess.run(
            [sys.executable, str(EXPORT_SCRIPT), str(scenario), "-o", str(out_path)],
            cwd=REPO_ROOT,
            capture_output=True,
            text=True,
            timeout=30,
        )
        self.assertEqual(
            result.returncode,
            0,
            f"export script failed:\nstdout={result.stdout}\nstderr={result.stderr}",
        )
        self.assertTrue(out_path.exists(), f"expected {out_path} to be created")
        return out_path

    def test_exports_canonical_scenarios_to_valid_firmware_bundle(self) -> None:
        for scenario in CANONICAL_SCENARIOS:
            with self.subTest(scenario=scenario.name):
                self.assertTrue(scenario.exists(), f"missing fixture: {scenario}")
                with tempfile.TemporaryDirectory() as tmp:
                    out_path = self._export(scenario, Path(tmp))
                    document = json.loads(out_path.read_text())

                    # Firmware contract: top-level keys
                    self.assertIn("scenario", document)
                    self.assertIn("steps", document)
                    self.assertIn("metadata", document)

                    # scenario block
                    scenario_block = document["scenario"]
                    self.assertIn("id", scenario_block)
                    self.assertIsInstance(scenario_block["id"], str)
                    self.assertGreater(len(scenario_block["id"]), 0)

                    # steps must be a non-empty array of typed entries
                    steps = document["steps"]
                    self.assertIsInstance(steps, list)
                    self.assertGreater(len(steps), 0)
                    for step in steps:
                        self.assertIn("id", step)
                        self.assertIn("transitions", step)
                        self.assertIsInstance(step["transitions"], list)

                    # Connectivity: every transition target must be a known step id
                    step_ids = {step["id"] for step in steps}
                    for step in steps:
                        for transition in step["transitions"]:
                            target = transition.get("target_step_id")
                            if target is None:
                                continue
                            self.assertIn(
                                target,
                                step_ids,
                                f"dangling transition from {step['id']} -> {target}",
                            )

    def test_default_output_path_is_firmware_data_tree(self) -> None:
        """The script's default output path matches the firmware LittleFS layout."""
        # The default lives in the source rather than --help output (argparse
        # does not render defaults without a custom formatter). We grep the
        # script directly to lock the contract: ESP32 expects the JSON at
        # hardware/firmware/data/story/runtime3/DEFAULT.json.
        source = EXPORT_SCRIPT.read_text()
        self.assertIn("hardware/firmware/data/story/runtime3/DEFAULT.json", source)


class FirmwareBundleRejectsInvalidScenarioTests(unittest.TestCase):
    """The pipeline must surface failures rather than emit garbage."""

    def test_export_fails_on_missing_yaml(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            missing = Path(tmp) / "does-not-exist.yaml"
            out_path = Path(tmp) / "out.json"
            result = subprocess.run(
                [sys.executable, str(EXPORT_SCRIPT), str(missing), "-o", str(out_path)],
                cwd=REPO_ROOT,
                capture_output=True,
                text=True,
                timeout=10,
            )
            self.assertNotEqual(result.returncode, 0)
            self.assertFalse(out_path.exists())


if __name__ == "__main__":
    unittest.main()
