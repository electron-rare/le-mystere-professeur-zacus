#!/usr/bin/env python3
"""test_npc_phrases_schema.py — Schema validation for npc_phrases.yaml.

Validates the structure and content of the Professor Zacus NPC phrase bank:
- Required top-level categories present
- hints: all 6 scenes present, each with levels 1-3
- Each phrase entry has a non-empty text field (or is a non-empty string)
- No empty strings anywhere
- congratulations and warnings have per-scene entries
- personality has all required mood subcategories
"""

from __future__ import annotations

import os
import unittest

import yaml


YAML_PATH = os.path.join(os.path.dirname(__file__), '..', 'game', 'scenarios', 'npc_phrases.yaml')

REQUIRED_CATEGORIES = [
    'hints', 'congratulations', 'warnings', 'personality',
    'adaptation', 'bridges', 'false_leads', 'ambiance',
]

EXPECTED_HINT_SCENES = [
    'SCENE_U_SON_PROTO',
    'SCENE_LA_DETECTOR',
    'SCENE_WARNING',
    'SCENE_LEFOU_DETECTOR',
    'SCENE_QR_DETECTOR',
    'SCENE_FINAL_WIN',
]

EXPECTED_HINT_LEVELS = ['level_1', 'level_2', 'level_3']

EXPECTED_PERSONALITY_MOODS = ['impressed', 'worried', 'amused', 'neutral']


def _extract_text(entry: object) -> str:
    """Return text content from a phrase entry (dict with 'text' key or bare string)."""
    if isinstance(entry, dict):
        return entry.get('text', '')
    if isinstance(entry, str):
        return entry
    return ''


def _collect_all_texts(obj: object, path: str = '') -> list[tuple[str, str]]:
    """Recursively collect all text values with their path for empty-string checking."""
    results: list[tuple[str, str]] = []
    if isinstance(obj, dict):
        for k, v in obj.items():
            child_path = f"{path}.{k}" if path else str(k)
            if k == 'text' and isinstance(v, str):
                results.append((child_path, v))
            else:
                results.extend(_collect_all_texts(v, child_path))
    elif isinstance(obj, list):
        for i, item in enumerate(obj):
            results.extend(_collect_all_texts(item, f"{path}[{i}]"))
    elif isinstance(obj, str):
        results.append((path, obj))
    return results


class TestNpcPhrasesSchema(unittest.TestCase):

    @classmethod
    def setUpClass(cls) -> None:
        with open(YAML_PATH, 'r', encoding='utf-8') as f:
            cls.phrases: dict = yaml.safe_load(f)

    # ------------------------------------------------------------------
    # 1. Top-level structure
    # ------------------------------------------------------------------

    def test_required_categories_present(self) -> None:
        for cat in REQUIRED_CATEGORIES:
            self.assertIn(cat, self.phrases, f"Missing top-level category: '{cat}'")

    def test_top_level_types(self) -> None:
        self.assertIsInstance(self.phrases['hints'], dict)
        self.assertIsInstance(self.phrases['congratulations'], dict)
        self.assertIsInstance(self.phrases['warnings'], dict)
        self.assertIsInstance(self.phrases['personality'], dict)
        self.assertIsInstance(self.phrases['adaptation'], dict)
        self.assertIsInstance(self.phrases['bridges'], dict)
        self.assertIsInstance(self.phrases['false_leads'], list)
        self.assertIsInstance(self.phrases['ambiance'], dict)

    # ------------------------------------------------------------------
    # 2. hints: 6 scenes × 3 levels, each level non-empty
    # ------------------------------------------------------------------

    def test_hints_all_scenes_present(self) -> None:
        hints = self.phrases['hints']
        for scene in EXPECTED_HINT_SCENES:
            self.assertIn(scene, hints, f"hints: missing scene '{scene}'")

    def test_hints_each_scene_has_three_levels(self) -> None:
        hints = self.phrases['hints']
        for scene in EXPECTED_HINT_SCENES:
            scene_data = hints[scene]
            self.assertIsInstance(scene_data, dict, f"hints.{scene} must be a dict")
            for level in EXPECTED_HINT_LEVELS:
                self.assertIn(level, scene_data,
                              f"hints.{scene}: missing '{level}'")
                entries = scene_data[level]
                self.assertIsInstance(entries, list,
                                      f"hints.{scene}.{level} must be a list")
                self.assertGreater(len(entries), 0,
                                   f"hints.{scene}.{level} is empty — at least 1 phrase required")

    def test_hints_each_entry_has_non_empty_text(self) -> None:
        hints = self.phrases['hints']
        for scene in EXPECTED_HINT_SCENES:
            for level in EXPECTED_HINT_LEVELS:
                entries = hints.get(scene, {}).get(level, [])
                for idx, entry in enumerate(entries):
                    text = _extract_text(entry)
                    self.assertIsInstance(text, str,
                                         f"hints.{scene}.{level}[{idx}] text must be a string")
                    self.assertGreater(len(text.strip()), 0,
                                       f"hints.{scene}.{level}[{idx}] has empty text")

    # ------------------------------------------------------------------
    # 3. congratulations and warnings: per-scene non-empty entries
    # ------------------------------------------------------------------

    def test_congratulations_has_entries_per_scene(self) -> None:
        congratulations = self.phrases['congratulations']
        self.assertGreater(len(congratulations), 0,
                           "congratulations must have at least one scene")
        for scene, entries in congratulations.items():
            self.assertIsInstance(entries, list,
                                  f"congratulations.{scene} must be a list")
            self.assertGreater(len(entries), 0,
                               f"congratulations.{scene} is empty")
            for idx, entry in enumerate(entries):
                text = _extract_text(entry)
                self.assertGreater(len(text.strip()), 0,
                                   f"congratulations.{scene}[{idx}] has empty text")

    def test_warnings_has_entries_per_scene(self) -> None:
        warnings = self.phrases['warnings']
        self.assertGreater(len(warnings), 0,
                           "warnings must have at least one scene")
        for scene, entries in warnings.items():
            self.assertIsInstance(entries, list,
                                  f"warnings.{scene} must be a list")
            self.assertGreater(len(entries), 0,
                               f"warnings.{scene} is empty")
            for idx, entry in enumerate(entries):
                text = _extract_text(entry)
                self.assertGreater(len(text.strip()), 0,
                                   f"warnings.{scene}[{idx}] has empty text")

    # ------------------------------------------------------------------
    # 4. personality: required mood subcategories, each non-empty
    # ------------------------------------------------------------------

    def test_personality_has_required_moods(self) -> None:
        personality = self.phrases['personality']
        for mood in EXPECTED_PERSONALITY_MOODS:
            self.assertIn(mood, personality,
                          f"personality: missing mood '{mood}'")

    def test_personality_moods_have_phrases(self) -> None:
        personality = self.phrases['personality']
        for mood in EXPECTED_PERSONALITY_MOODS:
            entries = personality.get(mood, [])
            self.assertIsInstance(entries, list,
                                  f"personality.{mood} must be a list")
            self.assertGreater(len(entries), 0,
                               f"personality.{mood} is empty")
            for idx, entry in enumerate(entries):
                text = _extract_text(entry)
                self.assertGreater(len(text.strip()), 0,
                                   f"personality.{mood}[{idx}] has empty text")

    # ------------------------------------------------------------------
    # 5. No empty strings anywhere in the file
    # ------------------------------------------------------------------

    def test_no_empty_strings_in_entire_file(self) -> None:
        all_texts = _collect_all_texts(self.phrases)
        empty = [(path, val) for path, val in all_texts if val.strip() == '']
        self.assertEqual(empty, [],
                         f"Empty strings found at paths: {[p for p, _ in empty]}")

    # ------------------------------------------------------------------
    # 6. false_leads: non-empty list with valid entries
    # ------------------------------------------------------------------

    def test_false_leads_non_empty_list(self) -> None:
        false_leads = self.phrases['false_leads']
        self.assertGreater(len(false_leads), 0, "false_leads must not be empty")
        for idx, entry in enumerate(false_leads):
            text = _extract_text(entry)
            self.assertGreater(len(text.strip()), 0,
                               f"false_leads[{idx}] has empty text")

    # ------------------------------------------------------------------
    # 7. ambiance: required subtypes present
    # ------------------------------------------------------------------

    def test_ambiance_has_required_types(self) -> None:
        ambiance = self.phrases['ambiance']
        required_types = ['intro', 'outro']
        for t in required_types:
            self.assertIn(t, ambiance, f"ambiance: missing type '{t}'")
            entries = ambiance[t]
            self.assertIsInstance(entries, list, f"ambiance.{t} must be a list")
            self.assertGreater(len(entries), 0, f"ambiance.{t} is empty")


if __name__ == '__main__':
    unittest.main()
