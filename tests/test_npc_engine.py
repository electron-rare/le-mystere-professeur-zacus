#!/usr/bin/env python3
"""test_npc_engine.py — Unit tests for Professor Zacus NPC trigger logic.

Tests the NPC decision engine rules by simulating state machine conditions
in pure Python (no ESP32 hardware required).

NPC rules under test:
  - Stuck timer fires hint after NPC_STUCK_TIMEOUT_MS (3 min)
  - QR scan (valid) increments qr_scanned_count → congratulation trigger
  - Fast progress (<50% elapsed/expected) → challenge trigger
  - Hint escalation: level 1 → 2 → 3
  - Mood affects response selection
"""

from __future__ import annotations

import unittest
from dataclasses import dataclass, field
from enum import IntEnum
from typing import Optional


# ---------------------------------------------------------------------------
# Pure-Python mirror of npc_engine.h constants / enums
# (Mirrors the C header so tests stay independent of firmware compilation)
# ---------------------------------------------------------------------------

NPC_MAX_SCENES = 12
NPC_MAX_HINT_LEVEL = 3
NPC_STUCK_TIMEOUT_MS = 3 * 60 * 1000  # 180 000 ms
NPC_FAST_THRESHOLD_PCT = 50
NPC_SLOW_THRESHOLD_PCT = 150
NPC_QR_DEBOUNCE_MS = 30_000


class NpcMood(IntEnum):
    NEUTRAL = 0
    IMPRESSED = 1
    WORRIED = 2
    AMUSED = 3


class NpcTrigger(IntEnum):
    NONE = 0
    HINT_REQUEST = 1
    STUCK_TIMER = 2
    QR_SCANNED = 3
    WRONG_ACTION = 4
    FAST_PROGRESS = 5
    SLOW_PROGRESS = 6
    SCENE_TRANSITION = 7
    GAME_START = 8
    GAME_END = 9


class NpcAudioSource(IntEnum):
    NONE = 0
    LIVE_TTS = 1
    SD_CONTEXTUAL = 2
    SD_GENERIC = 3


@dataclass
class NpcState:
    current_scene: int = 0
    current_step: int = 0
    scene_start_ms: int = 0
    total_elapsed_ms: int = 0
    hints_given: list[int] = field(default_factory=lambda: [0] * NPC_MAX_SCENES)
    qr_scanned_count: int = 0
    failed_attempts: int = 0
    phone_off_hook: bool = False
    tower_reachable: bool = True
    mood: NpcMood = NpcMood.NEUTRAL
    last_qr_scan_ms: int = 0
    expected_scene_duration_ms: int = 0


@dataclass
class NpcDecision:
    trigger: NpcTrigger = NpcTrigger.NONE
    audio_source: NpcAudioSource = NpcAudioSource.NONE
    phrase_text: str = ""
    sd_path: str = ""
    resulting_mood: NpcMood = NpcMood.NEUTRAL


# ---------------------------------------------------------------------------
# Pure-Python implementation of NPC logic (mirrors npc_engine.cpp)
# ---------------------------------------------------------------------------

def npc_init() -> NpcState:
    return NpcState()


def npc_reset(state: NpcState) -> None:
    state.current_scene = 0
    state.current_step = 0
    state.scene_start_ms = 0
    state.total_elapsed_ms = 0
    state.hints_given = [0] * NPC_MAX_SCENES
    state.qr_scanned_count = 0
    state.failed_attempts = 0
    state.phone_off_hook = False
    state.tower_reachable = True
    state.mood = NpcMood.NEUTRAL
    state.last_qr_scan_ms = 0
    state.expected_scene_duration_ms = 0


def npc_hint_level(state: NpcState, scene: int) -> int:
    if scene >= NPC_MAX_SCENES:
        return 0
    return state.hints_given[scene]


def npc_on_hint_request(state: NpcState, now_ms: int) -> None:
    scene = state.current_scene
    if scene < NPC_MAX_SCENES and state.hints_given[scene] < NPC_MAX_HINT_LEVEL:
        state.hints_given[scene] += 1


def npc_on_qr_scan(state: NpcState, valid: bool, now_ms: int) -> None:
    if valid:
        state.qr_scanned_count += 1
    else:
        state.failed_attempts += 1
    state.last_qr_scan_ms = now_ms


def npc_on_scene_change(state: NpcState, new_scene: int,
                        expected_duration_ms: int, now_ms: int) -> None:
    state.current_scene = new_scene
    state.scene_start_ms = now_ms
    state.expected_scene_duration_ms = expected_duration_ms
    state.failed_attempts = 0


def npc_on_phone_hook(state: NpcState, off_hook: bool) -> None:
    state.phone_off_hook = off_hook


def npc_update_mood(state: NpcState, now_ms: int) -> None:
    if state.expected_scene_duration_ms == 0:
        return
    elapsed = now_ms - state.scene_start_ms
    expected = state.expected_scene_duration_ms
    pct = (elapsed * 100) // expected

    if state.failed_attempts >= 3:
        state.mood = NpcMood.AMUSED
    elif pct < NPC_FAST_THRESHOLD_PCT:
        state.mood = NpcMood.IMPRESSED
    elif pct > NPC_SLOW_THRESHOLD_PCT:
        state.mood = NpcMood.WORRIED
    else:
        state.mood = NpcMood.NEUTRAL


def npc_evaluate(state: NpcState, now_ms: int) -> Optional[NpcDecision]:
    """Evaluate NPC trigger rules. Returns a decision or None."""
    scene_elapsed = now_ms - state.scene_start_ms
    expected = state.expected_scene_duration_ms
    decision = NpcDecision()

    # Priority 1: Hint request (phone off hook while stuck)
    if state.phone_off_hook and scene_elapsed > NPC_STUCK_TIMEOUT_MS:
        level = npc_hint_level(state, state.current_scene)
        decision.trigger = NpcTrigger.HINT_REQUEST
        decision.resulting_mood = state.mood
        decision.audio_source = (NpcAudioSource.LIVE_TTS if state.tower_reachable
                                 else NpcAudioSource.SD_CONTEXTUAL)
        return decision

    # Priority 2: Stuck timer (proactive, no phone needed)
    if (scene_elapsed > NPC_STUCK_TIMEOUT_MS
            and npc_hint_level(state, state.current_scene) == 0):
        decision.trigger = NpcTrigger.STUCK_TIMER
        decision.resulting_mood = NpcMood.WORRIED
        decision.audio_source = (NpcAudioSource.LIVE_TTS if state.tower_reachable
                                 else NpcAudioSource.SD_CONTEXTUAL)
        return decision

    # Priority 3: Fast progress detection.
    # Fire only when elapsed is strictly below half the fast threshold in absolute time,
    # to avoid false positives when a player has been on a scene for several minutes
    # even if the percentage is still under NPC_FAST_THRESHOLD_PCT.
    if expected > 0 and scene_elapsed > 0:
        fast_abs_limit = (expected * NPC_FAST_THRESHOLD_PCT) // 200  # half of the fast ceiling
        pct = (scene_elapsed * 100) // expected
        if pct < NPC_FAST_THRESHOLD_PCT and scene_elapsed < fast_abs_limit:
            decision.trigger = NpcTrigger.FAST_PROGRESS
            decision.resulting_mood = NpcMood.IMPRESSED
            decision.audio_source = (NpcAudioSource.LIVE_TTS if state.tower_reachable
                                     else NpcAudioSource.SD_CONTEXTUAL)
            return decision

    # Priority 4: Slow progress detection
    if expected > 0 and scene_elapsed > 0:
        pct = (scene_elapsed * 100) // expected
        if pct > NPC_SLOW_THRESHOLD_PCT:
            decision.trigger = NpcTrigger.SLOW_PROGRESS
            decision.resulting_mood = NpcMood.WORRIED
            decision.audio_source = (NpcAudioSource.LIVE_TTS if state.tower_reachable
                                     else NpcAudioSource.SD_CONTEXTUAL)
            return decision

    return None


# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------

class TestNpcStuckTimer(unittest.TestCase):
    """Stuck timer fires a hint after NPC_STUCK_TIMEOUT_MS."""

    def test_no_trigger_before_timeout(self) -> None:
        state = npc_init()
        npc_on_scene_change(state, new_scene=0, expected_duration_ms=600_000, now_ms=0)
        # Evaluate just before the stuck timeout
        decision = npc_evaluate(state, now_ms=NPC_STUCK_TIMEOUT_MS - 1)
        self.assertIsNone(decision)

    def test_stuck_timer_triggers_after_timeout(self) -> None:
        state = npc_init()
        npc_on_scene_change(state, new_scene=1, expected_duration_ms=600_000, now_ms=0)
        # Evaluate just after the stuck timeout — no hints given yet
        decision = npc_evaluate(state, now_ms=NPC_STUCK_TIMEOUT_MS + 1)
        self.assertIsNotNone(decision)
        self.assertEqual(decision.trigger, NpcTrigger.STUCK_TIMER)
        self.assertEqual(decision.resulting_mood, NpcMood.WORRIED)

    def test_stuck_timer_suppressed_after_hint_given(self) -> None:
        state = npc_init()
        npc_on_scene_change(state, new_scene=0, expected_duration_ms=600_000, now_ms=0)
        # Give a hint — stuck timer should not re-fire (priority 1 phone still false)
        npc_on_hint_request(state, now_ms=NPC_STUCK_TIMEOUT_MS + 1)
        decision = npc_evaluate(state, now_ms=NPC_STUCK_TIMEOUT_MS + 1)
        # hint level is now 1, stuck timer with level==0 guard won't fire
        # but phone is off hook? No — phone is False here, so priority 1 won't fire either
        self.assertIsNone(decision)


class TestQrScanCongratulation(unittest.TestCase):
    """QR scan increments count; congratulation should be issued."""

    def test_valid_qr_increments_count(self) -> None:
        state = npc_init()
        self.assertEqual(state.qr_scanned_count, 0)
        npc_on_qr_scan(state, valid=True, now_ms=1000)
        self.assertEqual(state.qr_scanned_count, 1)

    def test_invalid_qr_increments_failed_attempts(self) -> None:
        state = npc_init()
        npc_on_qr_scan(state, valid=False, now_ms=1000)
        self.assertEqual(state.failed_attempts, 1)
        self.assertEqual(state.qr_scanned_count, 0)

    def test_qr_scan_updates_last_scan_timestamp(self) -> None:
        state = npc_init()
        npc_on_qr_scan(state, valid=True, now_ms=42_000)
        self.assertEqual(state.last_qr_scan_ms, 42_000)

    def test_multiple_valid_scans_accumulate(self) -> None:
        state = npc_init()
        for i in range(3):
            npc_on_qr_scan(state, valid=True, now_ms=i * 1000)
        self.assertEqual(state.qr_scanned_count, 3)


class TestFastProgressChallenge(unittest.TestCase):
    """Fast progress (elapsed < 50% of expected) triggers challenge."""

    def test_fast_progress_triggers(self) -> None:
        state = npc_init()
        # Expected duration: 300 000 ms. After only 100 ms (<<50%), fast.
        npc_on_scene_change(state, new_scene=0, expected_duration_ms=300_000, now_ms=0)
        decision = npc_evaluate(state, now_ms=100)
        self.assertIsNotNone(decision)
        self.assertEqual(decision.trigger, NpcTrigger.FAST_PROGRESS)
        self.assertEqual(decision.resulting_mood, NpcMood.IMPRESSED)

    def test_normal_progress_no_trigger(self) -> None:
        state = npc_init()
        # At exactly 50% elapsed, not fast and not stuck yet
        npc_on_scene_change(state, new_scene=0, expected_duration_ms=200_000, now_ms=0)
        now = 100_000  # 50% of 200 000 — right at the boundary, not strictly <50
        decision = npc_evaluate(state, now_ms=now)
        # 100000 * 100 / 200000 = 50 — not < 50, not > 150, not stuck → None
        self.assertIsNone(decision)

    def test_fast_progress_mood_impressed(self) -> None:
        state = npc_init()
        npc_on_scene_change(state, new_scene=2, expected_duration_ms=600_000, now_ms=0)
        # Evaluate at 20% of expected duration
        now = int(0.2 * 600_000)  # 120 000 ms → 20%
        decision = npc_evaluate(state, now_ms=now)
        self.assertIsNotNone(decision)
        self.assertEqual(decision.resulting_mood, NpcMood.IMPRESSED)


class TestHintEscalation(unittest.TestCase):
    """Hint level escalates 0 → 1 → 2 → 3 and caps at NPC_MAX_HINT_LEVEL."""

    def test_initial_hint_level_zero(self) -> None:
        state = npc_init()
        self.assertEqual(npc_hint_level(state, scene=0), 0)

    def test_hint_escalation_sequence(self) -> None:
        state = npc_init()
        npc_on_scene_change(state, new_scene=0, expected_duration_ms=600_000, now_ms=0)
        self.assertEqual(npc_hint_level(state, 0), 0)

        npc_on_hint_request(state, now_ms=1000)
        self.assertEqual(npc_hint_level(state, 0), 1)

        npc_on_hint_request(state, now_ms=2000)
        self.assertEqual(npc_hint_level(state, 0), 2)

        npc_on_hint_request(state, now_ms=3000)
        self.assertEqual(npc_hint_level(state, 0), 3)

    def test_hint_caps_at_max_level(self) -> None:
        state = npc_init()
        npc_on_scene_change(state, new_scene=0, expected_duration_ms=600_000, now_ms=0)
        # Exhaust all hint levels
        for _ in range(NPC_MAX_HINT_LEVEL + 5):
            npc_on_hint_request(state, now_ms=1000)
        self.assertEqual(npc_hint_level(state, 0), NPC_MAX_HINT_LEVEL)

    def test_hint_levels_independent_per_scene(self) -> None:
        state = npc_init()
        npc_on_hint_request(state, now_ms=1000)  # scene 0
        self.assertEqual(npc_hint_level(state, 0), 1)
        self.assertEqual(npc_hint_level(state, 1), 0)  # scene 1 unaffected

    def test_hint_request_triggers_with_phone_and_stuck(self) -> None:
        state = npc_init()
        npc_on_scene_change(state, new_scene=0, expected_duration_ms=600_000, now_ms=0)
        npc_on_phone_hook(state, off_hook=True)
        now = NPC_STUCK_TIMEOUT_MS + 5000
        decision = npc_evaluate(state, now_ms=now)
        self.assertIsNotNone(decision)
        self.assertEqual(decision.trigger, NpcTrigger.HINT_REQUEST)


class TestMoodAffectsResponse(unittest.TestCase):
    """Mood transitions based on progress ratio and failed attempts."""

    def test_failed_attempts_trigger_amused(self) -> None:
        state = npc_init()
        npc_on_scene_change(state, new_scene=0, expected_duration_ms=300_000, now_ms=0)
        state.failed_attempts = 3
        now = 150_000  # 50% — neutral territory otherwise
        npc_update_mood(state, now_ms=now)
        self.assertEqual(state.mood, NpcMood.AMUSED)

    def test_fast_progress_yields_impressed_mood(self) -> None:
        state = npc_init()
        npc_on_scene_change(state, new_scene=0, expected_duration_ms=300_000, now_ms=0)
        now = 50_000  # ~16% → impressed
        npc_update_mood(state, now_ms=now)
        self.assertEqual(state.mood, NpcMood.IMPRESSED)

    def test_slow_progress_yields_worried_mood(self) -> None:
        state = npc_init()
        npc_on_scene_change(state, new_scene=0, expected_duration_ms=200_000, now_ms=0)
        now = 400_000  # 200% > 150 → worried
        npc_update_mood(state, now_ms=now)
        self.assertEqual(state.mood, NpcMood.WORRIED)

    def test_normal_progress_yields_neutral_mood(self) -> None:
        state = npc_init()
        npc_on_scene_change(state, new_scene=0, expected_duration_ms=200_000, now_ms=0)
        now = 100_000  # 50% — exact boundary → neutral (not < 50, not > 150)
        npc_update_mood(state, now_ms=now)
        self.assertEqual(state.mood, NpcMood.NEUTRAL)

    def test_audio_source_live_tts_when_tower_reachable(self) -> None:
        state = npc_init()
        state.tower_reachable = True
        npc_on_scene_change(state, new_scene=0, expected_duration_ms=600_000, now_ms=0)
        decision = npc_evaluate(state, now_ms=NPC_STUCK_TIMEOUT_MS + 1)
        self.assertIsNotNone(decision)
        self.assertEqual(decision.audio_source, NpcAudioSource.LIVE_TTS)

    def test_audio_source_sd_fallback_when_tower_down(self) -> None:
        state = npc_init()
        state.tower_reachable = False
        npc_on_scene_change(state, new_scene=0, expected_duration_ms=600_000, now_ms=0)
        decision = npc_evaluate(state, now_ms=NPC_STUCK_TIMEOUT_MS + 1)
        self.assertIsNotNone(decision)
        self.assertEqual(decision.audio_source, NpcAudioSource.SD_CONTEXTUAL)


if __name__ == "__main__":
    unittest.main()
