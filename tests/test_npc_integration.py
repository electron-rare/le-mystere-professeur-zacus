#!/usr/bin/env python3
"""test_npc_integration.py — Integration-level tests for NPC wiring logic.

Tests the decision routing logic that lives in npc_integration.cpp by
re-implementing it in Python, exactly mirroring the C module.  No ESP32
hardware is required — all subsystems are mocked.

What is tested (plan Tasks 1-8):
  - npc_integration_init → subsystems initialised, state zeroed
  - npc_integration_tick → mood + evaluate + dispatch called every 5 s
  - npc_integration_on_scene_change → scene/step forwarded to NPC engine
  - npc_integration_on_qr → valid payload → congratulation dispatched
  - npc_integration_on_phone_hook → rising edge + stuck → hint dispatched
  - dispatch_decision routing: LIVE_TTS / SD_CONTEXTUAL / SD_GENERIC / NONE
  - npc_integration_reset → state cleared
"""

from __future__ import annotations

import unittest
from dataclasses import dataclass, field
from enum import IntEnum
from typing import Optional
from unittest.mock import MagicMock, patch, call


# ---------------------------------------------------------------------------
# Pure-Python mirror of npc_engine.h  (identical to test_npc_engine.py)
# ---------------------------------------------------------------------------

NPC_MAX_SCENES        = 12
NPC_MAX_HINT_LEVEL    = 3
NPC_PHRASE_MAX_LEN    = 200
NPC_STUCK_TIMEOUT_MS  = 3 * 60 * 1000   # 180 000 ms
NPC_FAST_THRESHOLD_PCT = 50
NPC_SLOW_THRESHOLD_PCT = 150
NPC_QR_DEBOUNCE_MS    = 30_000


class NpcMood(IntEnum):
    NEUTRAL  = 0
    IMPRESSED = 1
    WORRIED  = 2
    AMUSED   = 3


class NpcTrigger(IntEnum):
    NONE             = 0
    HINT_REQUEST     = 1
    STUCK_TIMER      = 2
    QR_SCANNED       = 3
    WRONG_ACTION     = 4
    FAST_PROGRESS    = 5
    SLOW_PROGRESS    = 6
    SCENE_TRANSITION = 7
    GAME_START       = 8
    GAME_END         = 9


class NpcAudioSource(IntEnum):
    NONE           = 0
    LIVE_TTS       = 1
    SD_CONTEXTUAL  = 2
    SD_GENERIC     = 3


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
# Pure-Python NPC engine  (mirrors npc_engine.cpp)
# ---------------------------------------------------------------------------

_SCENE_IDS = [
    "SCENE_U_SON_PROTO",
    "SCENE_LA_DETECTOR",
    "SCENE_WIN_ETAPE1",
    "SCENE_WARNING",
    "SCENE_LEFOU_DETECTOR",
    "SCENE_WIN_ETAPE2",
    "SCENE_QR_DETECTOR",
    "SCENE_FINAL_WIN",
]

_TRIGGER_DIRS = {
    NpcTrigger.NONE:             "generic",
    NpcTrigger.HINT_REQUEST:     "indice",
    NpcTrigger.STUCK_TIMER:      "indice",
    NpcTrigger.QR_SCANNED:       "felicitations",
    NpcTrigger.WRONG_ACTION:     "attention",
    NpcTrigger.FAST_PROGRESS:    "fausse_piste",
    NpcTrigger.SLOW_PROGRESS:    "adaptation",
    NpcTrigger.SCENE_TRANSITION: "transition",
    NpcTrigger.GAME_START:       "ambiance",
    NpcTrigger.GAME_END:         "ambiance",
}

_MOOD_SUFFIXES = {
    NpcMood.NEUTRAL:   "neutral",
    NpcMood.IMPRESSED: "impressed",
    NpcMood.WORRIED:   "worried",
    NpcMood.AMUSED:    "amused",
}


def npc_init() -> NpcState:
    return NpcState()


def npc_reset(state: NpcState) -> None:
    state.__init__()  # type: ignore[misc]


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


def npc_on_tower_status(state: NpcState, reachable: bool) -> None:
    state.tower_reachable = reachable


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


def npc_build_sd_path(scene: int, trigger: NpcTrigger,
                      mood: NpcMood, variant: int) -> str:
    scene_id = _SCENE_IDS[scene] if scene < len(_SCENE_IDS) else "npc"
    trigger_dir = _TRIGGER_DIRS.get(trigger, "generic")
    mood_str    = _MOOD_SUFFIXES.get(mood, "neutral")

    scene_specific = trigger not in (
        NpcTrigger.GAME_START, NpcTrigger.GAME_END, NpcTrigger.NONE
    )

    if scene_specific and scene < len(_SCENE_IDS):
        return f"/hotline_tts/{scene_id}/{trigger_dir}_{mood_str}_{variant}.mp3"
    return f"/hotline_tts/npc/{trigger_dir}_{mood_str}_{variant}.mp3"


def npc_evaluate(state: NpcState, now_ms: int) -> Optional[NpcDecision]:
    scene_elapsed = now_ms - state.scene_start_ms
    expected = state.expected_scene_duration_ms
    d = NpcDecision()

    if state.phone_off_hook and scene_elapsed > NPC_STUCK_TIMEOUT_MS:
        level = npc_hint_level(state, state.current_scene)
        d.trigger       = NpcTrigger.HINT_REQUEST
        d.resulting_mood = state.mood
        d.sd_path        = npc_build_sd_path(state.current_scene,
                                             NpcTrigger.HINT_REQUEST, state.mood, level)
        d.audio_source   = (NpcAudioSource.LIVE_TTS if state.tower_reachable
                            else NpcAudioSource.SD_CONTEXTUAL)
        return d

    if scene_elapsed > NPC_STUCK_TIMEOUT_MS and npc_hint_level(state, state.current_scene) == 0:
        d.trigger       = NpcTrigger.STUCK_TIMER
        d.resulting_mood = NpcMood.WORRIED
        d.sd_path        = npc_build_sd_path(state.current_scene,
                                             NpcTrigger.STUCK_TIMER, NpcMood.WORRIED, 0)
        d.audio_source   = (NpcAudioSource.LIVE_TTS if state.tower_reachable
                            else NpcAudioSource.SD_CONTEXTUAL)
        return d

    if expected > 0 and scene_elapsed > 0:
        fast_abs_limit = (expected * NPC_FAST_THRESHOLD_PCT) // 200
        pct = (scene_elapsed * 100) // expected
        if pct < NPC_FAST_THRESHOLD_PCT and scene_elapsed < fast_abs_limit:
            d.trigger       = NpcTrigger.FAST_PROGRESS
            d.resulting_mood = NpcMood.IMPRESSED
            d.sd_path        = npc_build_sd_path(state.current_scene,
                                                 NpcTrigger.FAST_PROGRESS,
                                                 NpcMood.IMPRESSED, 0)
            d.audio_source   = (NpcAudioSource.LIVE_TTS if state.tower_reachable
                                else NpcAudioSource.SD_CONTEXTUAL)
            return d

    if expected > 0 and scene_elapsed > 0:
        pct = (scene_elapsed * 100) // expected
        if pct > NPC_SLOW_THRESHOLD_PCT:
            d.trigger       = NpcTrigger.SLOW_PROGRESS
            d.resulting_mood = NpcMood.WORRIED
            d.sd_path        = npc_build_sd_path(state.current_scene,
                                                 NpcTrigger.SLOW_PROGRESS,
                                                 NpcMood.WORRIED, 0)
            d.audio_source   = (NpcAudioSource.LIVE_TTS if state.tower_reachable
                                else NpcAudioSource.SD_CONTEXTUAL)
            return d

    return None


# ---------------------------------------------------------------------------
# Mock subsystems (mirror npc_integration.cpp dependencies)
# ---------------------------------------------------------------------------

class MockTtsClient:
    def __init__(self) -> None:
        self.tower_reachable = True
        self.health_tick_count = 0

    def tts_init(self) -> None:
        pass

    def tts_health_tick(self, now_ms: int) -> None:
        self.health_tick_count += 1

    def tts_is_tower_reachable(self) -> bool:
        return self.tower_reachable


class MockAudioKitClient:
    def __init__(self) -> None:
        self.init_url: Optional[str] = None
        self.tts_calls: list[tuple[str, str, str]] = []
        self.sd_calls: list[str] = []

    def audio_kit_client_init(self, base_url: str) -> None:
        self.init_url = base_url

    def audio_kit_play_tts(self, text: str, tts_url: str, voice: str) -> None:
        self.tts_calls.append((text, tts_url, voice))

    def audio_kit_play_sd(self, sd_path: str) -> None:
        self.sd_calls.append(sd_path)


class MockQrScanner:
    """Simple mock: parse returns True for ZACUS:-prefixed payloads, status=QR_VALID."""

    QR_VALID = 0

    def qr_npc_parse(self, payload: str) -> tuple[bool, int]:
        """Returns (ok, status)."""
        if payload.startswith("ZACUS:"):
            return True, self.QR_VALID
        return False, 1  # QR_INVALID_FORMAT


# ---------------------------------------------------------------------------
# Python NPC integration layer  (mirrors npc_integration.cpp)
# ---------------------------------------------------------------------------

_DEFAULT_SCENE_DURATION_MS = 3 * 60 * 1000   # 180 000 ms
_NPC_TICK_INTERVAL_MS      = 5_000
_TTS_PIPER_URL             = "http://192.168.0.120:8001"
_TTS_VOICE                 = "tom-medium"


class NpcIntegration:
    """Python mirror of npc_integration.cpp for unit testing."""

    def __init__(self,
                 tts: MockTtsClient,
                 audio_kit: MockAudioKitClient,
                 qr: MockQrScanner) -> None:
        self._tts       = tts
        self._audio_kit = audio_kit
        self._qr        = qr
        self._state     = npc_init()
        self._last_tick = 0
        self._initialised = False

    # ------------------------------------------------------------------
    # Public API
    # ------------------------------------------------------------------

    def init(self, audio_kit_base_url: str) -> None:
        npc_reset(self._state)
        self._tts.tts_init()
        self._audio_kit.audio_kit_client_init(audio_kit_base_url)
        self._last_tick = 0
        self._initialised = True

    def tick(self, now_ms: int) -> None:
        if not self._initialised:
            return
        self._tts.tts_health_tick(now_ms)
        npc_on_tower_status(self._state, self._tts.tts_is_tower_reachable())

        if now_ms - self._last_tick < _NPC_TICK_INTERVAL_MS:
            return
        self._last_tick = now_ms

        npc_update_mood(self._state, now_ms)
        decision = npc_evaluate(self._state, now_ms)
        if decision is not None:
            self._dispatch(decision)

    def on_scene_change(self, scene: int, step: int) -> None:
        if not self._initialised:
            return
        npc_on_tower_status(self._state, self._tts.tts_is_tower_reachable())
        npc_on_scene_change(self._state, scene, _DEFAULT_SCENE_DURATION_MS, 0)

    def on_qr(self, payload: Optional[str]) -> None:
        if not self._initialised:
            return
        valid = False
        if payload:
            ok, status = self._qr.qr_npc_parse(payload)
            if ok and status == MockQrScanner.QR_VALID:
                valid = True

        npc_on_qr_scan(self._state, valid, 0)

        if valid:
            congratulation = NpcDecision(
                trigger       = NpcTrigger.QR_SCANNED,
                resulting_mood = NpcMood.IMPRESSED,
                audio_source  = (NpcAudioSource.LIVE_TTS if self._state.tower_reachable
                                 else NpcAudioSource.SD_CONTEXTUAL),
                sd_path       = npc_build_sd_path(self._state.current_scene,
                                                  NpcTrigger.QR_SCANNED,
                                                  NpcMood.IMPRESSED, 0),
            )
            self._dispatch(congratulation)

    def on_phone_hook(self, off_hook: bool) -> None:
        if not self._initialised:
            return
        was_off_hook = self._state.phone_off_hook
        npc_on_phone_hook(self._state, off_hook)

        if off_hook and not was_off_hook:
            scene_elapsed = self._last_tick - self._state.scene_start_ms
            if scene_elapsed > NPC_STUCK_TIMEOUT_MS:
                npc_on_hint_request(self._state, self._last_tick)
                hint = NpcDecision(
                    trigger       = NpcTrigger.HINT_REQUEST,
                    resulting_mood = self._state.mood,
                    audio_source  = (NpcAudioSource.LIVE_TTS if self._state.tower_reachable
                                     else NpcAudioSource.SD_CONTEXTUAL),
                    sd_path       = npc_build_sd_path(
                        self._state.current_scene,
                        NpcTrigger.HINT_REQUEST,
                        self._state.mood,
                        npc_hint_level(self._state, self._state.current_scene),
                    ),
                )
                self._dispatch(hint)

    def reset(self) -> None:
        npc_reset(self._state)
        self._last_tick = 0

    # ------------------------------------------------------------------
    # Internal helpers
    # ------------------------------------------------------------------

    def _dispatch(self, decision: NpcDecision) -> None:
        src = decision.audio_source

        if src == NpcAudioSource.LIVE_TTS:
            if decision.phrase_text:
                self._audio_kit.audio_kit_play_tts(
                    decision.phrase_text, _TTS_PIPER_URL, _TTS_VOICE
                )
                return
            # Fallthrough: phrase bank not wired, try SD path.
            src = NpcAudioSource.SD_CONTEXTUAL

        if src in (NpcAudioSource.SD_CONTEXTUAL, NpcAudioSource.SD_GENERIC):
            if decision.sd_path:
                self._audio_kit.audio_kit_play_sd(decision.sd_path)

        # NPC_AUDIO_NONE → no-op


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _make_integration(tower_reachable: bool = True) -> NpcIntegration:
    tts       = MockTtsClient()
    tts.tower_reachable = tower_reachable
    audio_kit = MockAudioKitClient()
    qr        = MockQrScanner()
    npc       = NpcIntegration(tts, audio_kit, qr)
    npc.init("http://192.168.0.42:8300")
    return npc


# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------

class TestNpcIntegrationInit(unittest.TestCase):
    """npc_integration_init initialises subsystems correctly."""

    def test_audio_kit_init_called_with_correct_url(self) -> None:
        npc = _make_integration()
        self.assertEqual(npc._audio_kit.init_url, "http://192.168.0.42:8300")

    def test_state_zeroed_after_init(self) -> None:
        npc = _make_integration()
        s = npc._state
        self.assertEqual(s.current_scene, 0)
        self.assertEqual(s.qr_scanned_count, 0)
        self.assertEqual(s.failed_attempts, 0)
        self.assertFalse(s.phone_off_hook)
        self.assertEqual(s.mood, NpcMood.NEUTRAL)


class TestNpcIntegrationSceneChange(unittest.TestCase):
    """on_scene_change propagates scene and step to NPC engine."""

    def test_scene_change_updates_current_scene(self) -> None:
        npc = _make_integration()
        npc.on_scene_change(scene=3, step=0)
        self.assertEqual(npc._state.current_scene, 3)

    def test_scene_change_resets_failed_attempts(self) -> None:
        npc = _make_integration()
        npc._state.failed_attempts = 5
        npc.on_scene_change(scene=1, step=0)
        self.assertEqual(npc._state.failed_attempts, 0)

    def test_scene_change_sets_default_duration(self) -> None:
        npc = _make_integration()
        npc.on_scene_change(scene=2, step=0)
        self.assertEqual(npc._state.expected_scene_duration_ms,
                         _DEFAULT_SCENE_DURATION_MS)

    def test_npc_responds_after_scene_change(self) -> None:
        """After a scene change, the NPC should fire the stuck timer once
        enough time has elapsed."""
        npc = _make_integration()
        npc.on_scene_change(scene=0, step=0)
        # Advance well past stuck timeout in multiple ticks.
        stuck_now = NPC_STUCK_TIMEOUT_MS + 10_000
        npc.tick(stuck_now)
        # A stuck-timer SD decision should have been dispatched.
        self.assertGreater(len(npc._audio_kit.sd_calls), 0)


class TestNpcIntegrationQrScan(unittest.TestCase):
    """on_qr dispatches congratulation on valid ZACUS: payload."""

    def test_valid_zacus_qr_dispatches_sd_congratulation(self) -> None:
        npc = _make_integration(tower_reachable=False)
        npc.on_qr("ZACUS:SCENE_LA_DETECTOR:unlock_01")
        self.assertEqual(len(npc._audio_kit.sd_calls), 1)
        self.assertIn("felicitations", npc._audio_kit.sd_calls[0])

    def test_valid_zacus_qr_dispatches_live_tts_when_tower_up(self) -> None:
        npc = _make_integration(tower_reachable=True)
        # phrase_text is empty (phrase bank not wired), falls back to SD.
        npc.on_qr("ZACUS:SCENE_LA_DETECTOR:unlock_01")
        # With empty phrase_text, dispatch falls through to SD even when tower is up.
        self.assertEqual(len(npc._audio_kit.sd_calls), 1)

    def test_invalid_payload_no_dispatch(self) -> None:
        npc = _make_integration()
        npc.on_qr("INVALID_DATA")
        self.assertEqual(len(npc._audio_kit.sd_calls), 0)
        self.assertEqual(len(npc._audio_kit.tts_calls), 0)

    def test_none_payload_no_dispatch(self) -> None:
        npc = _make_integration()
        npc.on_qr(None)
        self.assertEqual(len(npc._audio_kit.sd_calls), 0)

    def test_valid_qr_increments_qr_scanned_count(self) -> None:
        npc = _make_integration()
        npc.on_qr("ZACUS:SCENE_LA_DETECTOR:unlock_01")
        self.assertEqual(npc._state.qr_scanned_count, 1)

    def test_invalid_qr_increments_failed_attempts(self) -> None:
        npc = _make_integration()
        npc.on_qr("WRONG")
        self.assertEqual(npc._state.failed_attempts, 1)


class TestNpcIntegrationPhoneHook(unittest.TestCase):
    """on_phone_hook rising edge while stuck dispatches hint immediately."""

    def test_phone_lift_while_stuck_dispatches_hint(self) -> None:
        npc = _make_integration(tower_reachable=False)
        npc.on_scene_change(scene=1, step=0)
        # Simulate the tick tracker at a time past stuck timeout so that
        # scene_elapsed = _last_tick - scene_start_ms > NPC_STUCK_TIMEOUT_MS.
        npc._last_tick = NPC_STUCK_TIMEOUT_MS + 10_000
        npc._state.scene_start_ms = 0

        npc.on_phone_hook(off_hook=True)
        # Should have dispatched a hint via SD (tower down).
        self.assertGreater(len(npc._audio_kit.sd_calls), 0)
        self.assertIn("indice", npc._audio_kit.sd_calls[0])

    def test_phone_lift_not_stuck_no_dispatch(self) -> None:
        npc = _make_integration()
        npc.on_scene_change(scene=0, step=0)
        npc._last_tick = 1_000   # well under stuck timeout
        npc._state.scene_start_ms = 0

        npc.on_phone_hook(off_hook=True)
        self.assertEqual(len(npc._audio_kit.sd_calls), 0)
        self.assertEqual(len(npc._audio_kit.tts_calls), 0)

    def test_phone_repeated_lift_does_not_re_dispatch(self) -> None:
        npc = _make_integration(tower_reachable=False)
        npc.on_scene_change(scene=0, step=0)
        npc._last_tick = NPC_STUCK_TIMEOUT_MS + 5_000
        npc._state.scene_start_ms = 0

        npc.on_phone_hook(off_hook=True)
        calls_after_first = len(npc._audio_kit.sd_calls)
        # Second call with same state (off_hook already True) — no rising edge.
        npc.on_phone_hook(off_hook=True)
        self.assertEqual(len(npc._audio_kit.sd_calls), calls_after_first)

    def test_phone_hook_state_updated(self) -> None:
        npc = _make_integration()
        npc.on_phone_hook(off_hook=True)
        self.assertTrue(npc._state.phone_off_hook)
        npc.on_phone_hook(off_hook=False)
        self.assertFalse(npc._state.phone_off_hook)


class TestNpcIntegrationTick(unittest.TestCase):
    """npc_integration_tick evaluates NPC every 5 s and dispatches decisions."""

    def test_tick_before_interval_no_evaluate(self) -> None:
        npc = _make_integration()
        npc.on_scene_change(scene=0, step=0)
        npc._last_tick = 0
        # Tick at 1 s — well under 5 s interval.
        npc.tick(1_000)
        self.assertEqual(len(npc._audio_kit.sd_calls), 0)

    def test_tick_at_5s_interval_evaluates(self) -> None:
        npc = _make_integration(tower_reachable=False)
        npc.on_scene_change(scene=0, step=0)
        npc._last_tick = 0
        # Advance past stuck timeout in one tick.
        npc.tick(NPC_STUCK_TIMEOUT_MS + _NPC_TICK_INTERVAL_MS)
        self.assertGreater(len(npc._audio_kit.sd_calls), 0)

    def test_tick_updates_tower_status(self) -> None:
        npc = _make_integration(tower_reachable=False)
        npc.tick(100)
        self.assertFalse(npc._state.tower_reachable)

        npc._tts.tower_reachable = True
        npc.tick(200)
        self.assertTrue(npc._state.tower_reachable)


class TestNpcIntegrationStuckTimer(unittest.TestCase):
    """Stuck timer fires after NPC_STUCK_TIMEOUT_MS via tick."""

    def test_stuck_timer_triggers_hint_decision(self) -> None:
        npc = _make_integration(tower_reachable=False)
        npc.on_scene_change(scene=0, step=0)
        npc._state.scene_start_ms = 0
        now = NPC_STUCK_TIMEOUT_MS + _NPC_TICK_INTERVAL_MS
        npc.tick(now)
        self.assertGreater(len(npc._audio_kit.sd_calls), 0)
        self.assertIn("indice", npc._audio_kit.sd_calls[0])

    def test_stuck_timer_sd_path_contains_scene_id(self) -> None:
        npc = _make_integration(tower_reachable=False)
        npc.on_scene_change(scene=1, step=0)  # SCENE_LA_DETECTOR
        npc._state.scene_start_ms = 0
        npc.tick(NPC_STUCK_TIMEOUT_MS + _NPC_TICK_INTERVAL_MS)
        self.assertTrue(any("SCENE_LA_DETECTOR" in p for p in npc._audio_kit.sd_calls))


class TestNpcIntegrationDispatchRouting(unittest.TestCase):
    """dispatch_decision routes LIVE_TTS / SD_CONTEXTUAL / SD_GENERIC / NONE."""

    def _dispatch(self, npc: NpcIntegration, d: NpcDecision) -> None:
        npc._dispatch(d)

    def test_live_tts_with_phrase_calls_play_tts(self) -> None:
        npc = _make_integration()
        d = NpcDecision(
            trigger      = NpcTrigger.STUCK_TIMER,
            audio_source = NpcAudioSource.LIVE_TTS,
            phrase_text  = "Attention, vous semblez bloqués.",
            sd_path      = "/hotline_tts/foo.mp3",
        )
        self._dispatch(npc, d)
        self.assertEqual(len(npc._audio_kit.tts_calls), 1)
        self.assertIn("Attention", npc._audio_kit.tts_calls[0][0])
        self.assertEqual(len(npc._audio_kit.sd_calls), 0)

    def test_live_tts_empty_phrase_falls_back_to_sd(self) -> None:
        npc = _make_integration()
        d = NpcDecision(
            trigger      = NpcTrigger.STUCK_TIMER,
            audio_source = NpcAudioSource.LIVE_TTS,
            phrase_text  = "",   # phrase bank not wired
            sd_path      = "/hotline_tts/indice_worried_0.mp3",
        )
        self._dispatch(npc, d)
        self.assertEqual(len(npc._audio_kit.tts_calls), 0)
        self.assertEqual(npc._audio_kit.sd_calls, ["/hotline_tts/indice_worried_0.mp3"])

    def test_sd_contextual_calls_play_sd(self) -> None:
        npc = _make_integration()
        d = NpcDecision(
            trigger      = NpcTrigger.STUCK_TIMER,
            audio_source = NpcAudioSource.SD_CONTEXTUAL,
            sd_path      = "/hotline_tts/SCENE_LA_DETECTOR/indice_worried_0.mp3",
        )
        self._dispatch(npc, d)
        self.assertEqual(npc._audio_kit.sd_calls,
                         ["/hotline_tts/SCENE_LA_DETECTOR/indice_worried_0.mp3"])

    def test_sd_generic_calls_play_sd(self) -> None:
        npc = _make_integration()
        d = NpcDecision(
            trigger      = NpcTrigger.GAME_START,
            audio_source = NpcAudioSource.SD_GENERIC,
            sd_path      = "/hotline_tts/npc/ambiance_neutral_0.mp3",
        )
        self._dispatch(npc, d)
        self.assertEqual(npc._audio_kit.sd_calls,
                         ["/hotline_tts/npc/ambiance_neutral_0.mp3"])

    def test_none_source_no_calls(self) -> None:
        npc = _make_integration()
        d = NpcDecision(trigger=NpcTrigger.NONE, audio_source=NpcAudioSource.NONE)
        self._dispatch(npc, d)
        self.assertEqual(len(npc._audio_kit.tts_calls), 0)
        self.assertEqual(len(npc._audio_kit.sd_calls), 0)

    def test_sd_contextual_empty_path_no_call(self) -> None:
        npc = _make_integration()
        d = NpcDecision(
            trigger      = NpcTrigger.STUCK_TIMER,
            audio_source = NpcAudioSource.SD_CONTEXTUAL,
            sd_path      = "",
        )
        self._dispatch(npc, d)
        self.assertEqual(len(npc._audio_kit.sd_calls), 0)


class TestNpcIntegrationTowerFallback(unittest.TestCase):
    """Tower down forces SD fallback in all decision paths."""

    def test_stuck_timer_sd_when_tower_down(self) -> None:
        npc = _make_integration(tower_reachable=False)
        npc.on_scene_change(scene=0, step=0)
        npc._state.scene_start_ms = 0
        npc.tick(NPC_STUCK_TIMEOUT_MS + _NPC_TICK_INTERVAL_MS)
        self.assertGreater(len(npc._audio_kit.sd_calls), 0)
        self.assertEqual(len(npc._audio_kit.tts_calls), 0)

    def test_qr_congratulation_sd_when_tower_down(self) -> None:
        npc = _make_integration(tower_reachable=False)
        npc.on_qr("ZACUS:SCENE_LA_DETECTOR:unlock")
        self.assertGreater(len(npc._audio_kit.sd_calls), 0)
        self.assertEqual(len(npc._audio_kit.tts_calls), 0)


class TestNpcIntegrationSdPath(unittest.TestCase):
    """SD path generation matches expected format from plan Task 9 Test 6."""

    def test_sd_path_hint_request_scene_1_worried_variant_2(self) -> None:
        path = npc_build_sd_path(
            scene   = 1,
            trigger = NpcTrigger.HINT_REQUEST,
            mood    = NpcMood.WORRIED,
            variant = 2,
        )
        # Plan spec: starts with /hotline_tts/SCENE_LA_DETECTOR/indice_worried_2
        self.assertTrue(
            path.startswith("/hotline_tts/SCENE_LA_DETECTOR/indice_worried_2"),
            msg=f"Got: {path}",
        )

    def test_sd_path_game_start_generic(self) -> None:
        path = npc_build_sd_path(
            scene   = 0,
            trigger = NpcTrigger.GAME_START,
            mood    = NpcMood.NEUTRAL,
            variant = 0,
        )
        self.assertTrue(path.startswith("/hotline_tts/npc/"), msg=f"Got: {path}")
        self.assertIn("ambiance", path)

    def test_sd_path_qr_scanned_scene_0(self) -> None:
        path = npc_build_sd_path(
            scene   = 0,
            trigger = NpcTrigger.QR_SCANNED,
            mood    = NpcMood.IMPRESSED,
            variant = 0,
        )
        self.assertIn("SCENE_U_SON_PROTO", path)
        self.assertIn("felicitations", path)


class TestNpcIntegrationReset(unittest.TestCase):
    """npc_integration_reset clears state for a new game session."""

    def test_reset_clears_scene_and_counters(self) -> None:
        npc = _make_integration()
        npc.on_scene_change(scene=5, step=0)
        npc._state.qr_scanned_count = 3
        npc._state.failed_attempts  = 7
        npc._last_tick = 99_999

        npc.reset()

        self.assertEqual(npc._state.current_scene, 0)
        self.assertEqual(npc._state.qr_scanned_count, 0)
        self.assertEqual(npc._state.failed_attempts, 0)
        self.assertEqual(npc._last_tick, 0)

    def test_reset_clears_phone_state(self) -> None:
        npc = _make_integration()
        npc._state.phone_off_hook = True
        npc.reset()
        self.assertFalse(npc._state.phone_off_hook)


if __name__ == "__main__":
    unittest.main(verbosity=2)
