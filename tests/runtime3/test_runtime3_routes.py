#!/usr/bin/env python3

from __future__ import annotations

import json
import sys
import tempfile
import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO_ROOT / "tools" / "scenario"))

from runtime3_common import (
    compile_runtime3_document,
    dump_json,
    normalize_event_type,
    normalize_token,
    read_yaml,
    simulate_runtime3_document,
    validate_runtime3_document,
)


SCENARIO_PATH = REPO_ROOT / "game" / "scenarios" / "zacus_v2.yaml"
EXPECTED_HISTORY = [
    "STEP_U_SON_PROTO",
    "STEP_LA_DETECTOR",
    "STEP_RTC_ESP_ETAPE1",
    "STEP_WIN_ETAPE1",
    "STEP_WARNING",
    "STEP_LEFOU_DETECTOR",
    "STEP_RTC_ESP_ETAPE2",
    "STEP_QR_DETECTOR",
    "STEP_FINAL_WIN",
]


class Runtime3CanonicalScenarioTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.raw = read_yaml(SCENARIO_PATH)
        cls.document = compile_runtime3_document(cls.raw)
        validate_runtime3_document(cls.document)
        cls.steps_by_id = {step["id"]: step for step in cls.document["steps"]}

    def test_canonical_history_preserves_expected_route(self) -> None:
        self.assertEqual(self.document["metadata"]["migration_mode"], "firmware_import")
        history = simulate_runtime3_document(self.document, max_steps=16).history
        self.assertEqual(history, EXPECTED_HISTORY)

    def test_detector_targets_match_story_pivots(self) -> None:
        self.assertEqual(
            self.steps_by_id["STEP_LA_DETECTOR"]["transitions"][0]["event_type"],
            "unlock",
        )
        self.assertEqual(
            self.steps_by_id["STEP_LEFOU_DETECTOR"]["transitions"][0]["event_type"],
            "unlock",
        )
        self.assertEqual(
            self.steps_by_id["STEP_QR_DETECTOR"]["transitions"][0]["event_type"],
            "unlock",
        )
        self.assertIn(
            "STEP_RTC_ESP_ETAPE1",
            {transition["target_step_id"] for transition in self.steps_by_id["STEP_LA_DETECTOR"]["transitions"]},
        )
        self.assertIn(
            "STEP_RTC_ESP_ETAPE2",
            {transition["target_step_id"] for transition in self.steps_by_id["STEP_LEFOU_DETECTOR"]["transitions"]},
        )
        self.assertIn(
            "STEP_FINAL_WIN",
            {transition["target_step_id"] for transition in self.steps_by_id["STEP_QR_DETECTOR"]["transitions"]},
        )

    def test_exported_document_roundtrips_to_json(self) -> None:
        with tempfile.TemporaryDirectory() as tmpdir:
            out_path = Path(tmpdir) / "runtime3.json"
            dump_json(self.document, out_path)
            payload = json.loads(out_path.read_text(encoding="utf-8"))
        self.assertEqual(payload["schema_version"], "zacus.runtime3.v1")
        self.assertEqual(payload["scenario"]["entry_step_id"], EXPECTED_HISTORY[0])
        self.assertEqual(len(payload["steps"]), len(EXPECTED_HISTORY))

    def test_explicit_firmware_graph_prefers_highest_priority_transition(self) -> None:
        document = compile_runtime3_document(
            {
                "id": "fixture_graph",
                "version": 1,
                "title": "Fixture Graph",
                "firmware": {
                    "initial_step": "boot",
                    "steps": [
                        {
                            "step_id": "boot",
                            "screen_scene_id": "scene_boot",
                            "transitions": [
                                {
                                    "event_type": "serial",
                                    "event_name": "btn_next",
                                    "target_step_id": "gate",
                                    "priority": 0,
                                }
                            ],
                        },
                        {
                            "step_id": "gate",
                            "screen_scene_id": "scene_gate",
                            "transitions": [
                                {
                                    "event_type": "timer",
                                    "event_name": "timeout",
                                    "target_step_id": "fail",
                                    "priority": 1,
                                },
                                {
                                    "event_type": "unlock",
                                    "event_name": "pass",
                                    "target_step_id": "win",
                                    "priority": 10,
                                },
                            ],
                        },
                        {"step_id": "fail", "screen_scene_id": "scene_fail"},
                        {"step_id": "win", "screen_scene_id": "scene_win"},
                    ],
                },
            }
        )
        validate_runtime3_document(document)
        history = simulate_runtime3_document(document, max_steps=4).history
        self.assertEqual(document["metadata"]["migration_mode"], "firmware_import")
        self.assertEqual(document["scenario"]["entry_step_id"], "BOOT")
        self.assertEqual(history, ["BOOT", "GATE", "WIN"])


def _make_doc(steps, entry_step_id):
    """Helper: build a minimal valid runtime3 document."""
    return {
        "schema_version": "zacus.runtime3.v1",
        "scenario": {
            "id": "TEST",
            "version": 1,
            "title": "Test",
            "entry_step_id": entry_step_id,
            "source_kind": "test",
        },
        "steps": steps,
        "metadata": {"migration_mode": "test", "generated_by": "test"},
    }


class TestNormalization(unittest.TestCase):
    def test_normalize_token_empty_string(self) -> None:
        self.assertEqual(normalize_token("", "FALLBACK"), "FALLBACK")

    def test_normalize_token_special_chars_only(self) -> None:
        self.assertEqual(normalize_token("@#$%", "FALLBACK"), "FALLBACK")

    def test_normalize_token_unicode(self) -> None:
        result = normalize_token("étape_un", "FALLBACK")
        self.assertTrue(len(result) > 0)
        self.assertNotEqual(result, "FALLBACK")

    def test_normalize_token_collision(self) -> None:
        a = normalize_token("step-1", "X")
        b = normalize_token("step_1", "X")
        self.assertEqual(a, b)

    def test_normalize_event_type_valid(self) -> None:
        self.assertEqual(normalize_event_type("button"), "button")
        self.assertEqual(normalize_event_type("TIMER"), "timer")

    def test_normalize_event_type_invalid_returns_serial(self) -> None:
        self.assertEqual(normalize_event_type("nonsense"), "serial")
        self.assertEqual(normalize_event_type(""), "serial")


class TestValidation(unittest.TestCase):
    def test_missing_schema_version_fails(self) -> None:
        doc = _make_doc([{"id": "A", "transitions": []}], "A")
        del doc["schema_version"]
        with self.assertRaises(ValueError):
            validate_runtime3_document(doc)

    def test_wrong_schema_version_fails(self) -> None:
        doc = _make_doc([{"id": "A", "transitions": []}], "A")
        doc["schema_version"] = "zacus.runtime3.v999"
        with self.assertRaises(ValueError):
            validate_runtime3_document(doc)

    def test_duplicate_step_ids_fails(self) -> None:
        doc = _make_doc(
            [{"id": "A", "transitions": []}, {"id": "A", "transitions": []}],
            "A",
        )
        with self.assertRaises(ValueError, msg="duplicate step id"):
            validate_runtime3_document(doc)

    def test_transition_target_nonexistent_fails(self) -> None:
        doc = _make_doc(
            [{"id": "A", "transitions": [{"event_type": "serial", "event_name": "X", "target_step_id": "NOWHERE", "priority": 0, "after_ms": 0}]}],
            "A",
        )
        with self.assertRaises(ValueError):
            validate_runtime3_document(doc)

    def test_entry_step_not_found_fails(self) -> None:
        doc = _make_doc([{"id": "A", "transitions": []}], "MISSING")
        with self.assertRaises(ValueError):
            validate_runtime3_document(doc)

    def test_empty_steps_list(self) -> None:
        doc = _make_doc([], "A")
        with self.assertRaises(ValueError):
            validate_runtime3_document(doc)

    def test_valid_document_passes(self) -> None:
        doc = _make_doc(
            [
                {"id": "A", "transitions": [{"event_type": "serial", "event_name": "GO", "target_step_id": "B", "priority": 0, "after_ms": 0}]},
                {"id": "B", "transitions": []},
            ],
            "A",
        )
        validate_runtime3_document(doc)  # should not raise


class TestCycleDetection(unittest.TestCase):
    def test_simple_cycle_detected(self) -> None:
        doc = _make_doc(
            [
                {"id": "A", "transitions": [{"event_type": "serial", "event_name": "X", "target_step_id": "B", "priority": 0, "after_ms": 0}]},
                {"id": "B", "transitions": [{"event_type": "serial", "event_name": "X", "target_step_id": "A", "priority": 0, "after_ms": 0}]},
            ],
            "A",
        )
        with self.assertRaises(ValueError, msg="circular transition detected"):
            validate_runtime3_document(doc)

    def test_self_loop_detected(self) -> None:
        doc = _make_doc(
            [{"id": "A", "transitions": [{"event_type": "serial", "event_name": "X", "target_step_id": "A", "priority": 0, "after_ms": 0}]}],
            "A",
        )
        with self.assertRaises(ValueError, msg="circular transition detected"):
            validate_runtime3_document(doc)

    def test_long_cycle_detected(self) -> None:
        doc = _make_doc(
            [
                {"id": "A", "transitions": [{"event_type": "serial", "event_name": "X", "target_step_id": "B", "priority": 0, "after_ms": 0}]},
                {"id": "B", "transitions": [{"event_type": "serial", "event_name": "X", "target_step_id": "C", "priority": 0, "after_ms": 0}]},
                {"id": "C", "transitions": [{"event_type": "serial", "event_name": "X", "target_step_id": "A", "priority": 0, "after_ms": 0}]},
            ],
            "A",
        )
        with self.assertRaises(ValueError, msg="circular transition detected"):
            validate_runtime3_document(doc)

    def test_no_cycle_passes(self) -> None:
        doc = _make_doc(
            [
                {"id": "A", "transitions": [{"event_type": "serial", "event_name": "X", "target_step_id": "B", "priority": 0, "after_ms": 0}]},
                {"id": "B", "transitions": [{"event_type": "serial", "event_name": "X", "target_step_id": "C", "priority": 0, "after_ms": 0}]},
                {"id": "C", "transitions": []},
            ],
            "A",
        )
        validate_runtime3_document(doc)  # should not raise

    def test_dag_with_shared_node_passes(self) -> None:
        doc = _make_doc(
            [
                {"id": "A", "transitions": [{"event_type": "serial", "event_name": "X", "target_step_id": "C", "priority": 0, "after_ms": 0}]},
                {"id": "B", "transitions": [{"event_type": "serial", "event_name": "X", "target_step_id": "C", "priority": 0, "after_ms": 0}]},
                {"id": "C", "transitions": []},
            ],
            "A",
        )
        validate_runtime3_document(doc)  # should not raise


class TestCompilation(unittest.TestCase):
    def test_compile_missing_firmware_section(self) -> None:
        doc = compile_runtime3_document({"id": "test", "version": 1, "title": "Test"})
        self.assertEqual(doc["metadata"]["migration_mode"], "linear_import")
        self.assertIn("steps", doc)

    def test_compile_empty_transitions(self) -> None:
        doc = compile_runtime3_document(
            {
                "id": "test",
                "version": 1,
                "title": "Test",
                "firmware": {
                    "initial_step": "only",
                    "steps": [
                        {"step_id": "only", "screen_scene_id": "scene_only", "transitions": []},
                    ],
                },
            }
        )
        validate_runtime3_document(doc)
        step = doc["steps"][0]
        self.assertEqual(step["transitions"], [])


if __name__ == "__main__":
    unittest.main()
