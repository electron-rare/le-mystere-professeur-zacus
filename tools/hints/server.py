"""Hints engine — Phase P4 FastAPI server.

Static phrase lookup (P1) + LLM rewrite layer via LiteLLM `hints-deep` (P2)
+ in-memory anti-cheat (cap 3 / cooldown / progressive penalties) (P3)
+ adaptive level selection (group profile + stuck-timer + failed attempts) (P4).

The rewriter restyles the static base phrase in the Professor Zacus voice
without inventing new content. Safety filter blocks rewrites that leak
solution tokens. On timeout / error / safety trip, the engine falls back
to the verbatim static phrase so the game never blocks.

Anti-cheat (P3):
  Per `(session_id, puzzle_id)` we track `count`, `last_at_ms`, `total_penalty`.
  Refusals returned as **HTTP 200 + `{refused: true, reason, ...}`** rather
  than 4xx/429 — the ESP32 firmware state machine is much simpler when it
  always parses a coherent JSON envelope. The transport layer remains a
  success ; the body conveys business outcome.

Adaptive level (P4):
  Optional `group_profile` (TECH | NON_TECH | MIXED | BOTH) + per-puzzle
  `puzzle_started_at_ms` + `failed_attempts_for_puzzle` produce an
  adaptive **floor** on the level served. Auto-trigger on stuck-timer is
  NOT implemented server-side — the firmware polls `/hints/ask` itself
  (see slice D_P6). Configuration: `game/scenarios/hints_adaptive.yaml`.

Endpoints:
  - GET  /healthz                       — liveness + counts
  - POST /hints/ask                     — base phrase, optionally LLM-rewritten,
                                          with anti-cheat (cap/cooldown/penalty)
                                          + adaptive level (P4).
                                          ?rewrite=false bypasses LLM.
  - POST /hints/rewrite                 — debug: rewrite-only, no static fallback,
                                          no anti-cheat accounting.
  - POST /hints/puzzle_start            — mark puzzle_started_at_ms (P4)
  - POST /hints/attempt_failed          — bump failed_attempts_for_puzzle (P4)
  - GET  /hints/coverage                — per-puzzle level coverage audit
  - GET  /hints/sessions                — admin: list active sessions/state
  - DELETE /hints/sessions/{session_id} — admin: reset session state
  - GET  /hints/events                  — SSE stream of live events (P5):
                                          hint_served, hint_refused, puzzle_start,
                                          attempt_failed, session_reset.
  - GET  /hints/events/test             — admin: trigger a synthetic broadcast
                                          (X-Admin-Key required when configured).

Run:
  uv run --with fastapi --with uvicorn --with pyyaml --with pydantic \
         --with httpx \
      uvicorn tools.hints.server:app --reload --port 8300

Env:
  LITELLM_URL            (default http://192.168.0.120:4000)
  LITELLM_MASTER_KEY     (default sk-zacus-local-dev-do-not-share)
  HINTS_LLM_MODEL        (default hints-deep)
  HINTS_LLM_TIMEOUT_S    (default 8.0)
  HINTS_SAFETY_PATH      (default game/scenarios/hints_safety.yaml)
  HINTS_ADAPTIVE_PATH    (default game/scenarios/hints_adaptive.yaml)
  ZACUS_NPC_PHRASES_PATH (default game/scenarios/npc_phrases.yaml)
  HINTS_COOLDOWN_S       (default 60)   — minimum gap between hints per puzzle
  HINTS_MAX_PER_PUZZLE   (default 3)    — hard cap per (session, puzzle)
  HINTS_PENALTY_L1       (default 50)   — score penalty for level_1 hint
  HINTS_PENALTY_L2       (default 100)  — score penalty for level_2 hint
  HINTS_PENALTY_L3       (default 200)  — score penalty for level_3 hint
  HINTS_ADMIN_KEY        (default unset) — when set, admin endpoints require
                                           matching `X-Admin-Key` header.

Spec: docs/superpowers/specs/2026-05-03-hints-engine-design.md §3-§5
P6 (auto-trigger on stuck-timer, firmware-side) is NOT yet implemented.
"""
from __future__ import annotations

import asyncio
import json
import logging
import os
import random
import re
import sys
import time
import unicodedata
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple

import httpx
import yaml
from fastapi import FastAPI, HTTPException, Query, Request
from pydantic import BaseModel, Field, conint
from sse_starlette.sse import EventSourceResponse


# ---------------------------------------------------------------------------
# Paths and constants
# ---------------------------------------------------------------------------

REPO_ROOT = Path(__file__).resolve().parents[2]
DEFAULT_PHRASES_PATH = REPO_ROOT / "game" / "scenarios" / "npc_phrases.yaml"
DEFAULT_SAFETY_PATH = REPO_ROOT / "game" / "scenarios" / "hints_safety.yaml"
DEFAULT_ADAPTIVE_PATH = REPO_ROOT / "game" / "scenarios" / "hints_adaptive.yaml"
PHRASES_PATH_ENV = "ZACUS_NPC_PHRASES_PATH"
SAFETY_PATH_ENV = "HINTS_SAFETY_PATH"
ADAPTIVE_PATH_ENV = "HINTS_ADAPTIVE_PATH"

DEFAULT_LITELLM_URL = "http://192.168.0.120:4000"
DEFAULT_LITELLM_KEY = "sk-zacus-local-dev-do-not-share"
DEFAULT_LLM_MODEL = "hints-deep"
DEFAULT_LLM_TIMEOUT_S = 8.0
DEFAULT_LLM_MAX_TOKENS = 80
DEFAULT_LLM_TEMPERATURE = 0.4

ZACUS_SYSTEM_PROMPT = (
    "Tu es le Professeur Zacus, savant excentrique. Réécris l'indice "
    "ci-dessous en restant fidèle à son contenu mais avec ton style "
    "théâtral et un ton dramatique adapté à un escape room. 2 phrases "
    "max. Ne révèle pas la solution, garde les nombres, codes et noms "
    "propres exacts. Pas d'emoji."
)

PUZZLE_ID_PATTERN = re.compile(r"^P\d+_[A-Z0-9_]+$")
ALL_LEVELS = (1, 2, 3)
NPC_MAX_HINT_LEVEL = max(ALL_LEVELS)
GROUP_PROFILES = ("TECH", "NON_TECH", "MIXED", "BOTH")
DEFAULT_GROUP_PROFILE = "MIXED"

# P3 anti-cheat defaults (overridable via env at app startup)
DEFAULT_COOLDOWN_S = 60
DEFAULT_MAX_PER_PUZZLE = 3
DEFAULT_PENALTY_L1 = 50
DEFAULT_PENALTY_L2 = 100
DEFAULT_PENALTY_L3 = 200

LOG = logging.getLogger("hints.server")


# ---------------------------------------------------------------------------
# P5 — SSE broadcast plumbing (module-level so SessionTracker hooks reach it)
# ---------------------------------------------------------------------------

# Allowed event types — kept narrow so the dashboard's switch/case stays sane.
SSE_EVENT_TYPES = (
    "hint_served",
    "hint_refused",
    "puzzle_start",
    "attempt_failed",
    "session_reset",
    "test",
)
SSE_QUEUE_MAXSIZE = 128
SSE_HEARTBEAT_S = 15

# Subscribers are plain asyncio.Queue instances. New queue per /hints/events
# connection; removed on disconnect or full-queue drop.
_subscribers: List["asyncio.Queue[Dict[str, Any]]"] = []


def _broadcast_event(event_type: str, payload: Dict[str, Any]) -> int:
    """Push an event to every active SSE subscriber (best-effort, non-blocking).

    The envelope mirrors what the dashboard expects:
        {"type": <event_type>, "ts_ms": <wall clock ms>, "payload": <dict>}

    Best-effort: if a subscriber's queue is full, the event is dropped FOR
    THAT SUBSCRIBER ONLY (queue stays in the list — the slow consumer just
    misses one event). Returns the number of queues that accepted the event.

    Safe to call from sync request handlers — `Queue.put_nowait` does not
    require an event loop, only that the queue was created on one.
    """
    if event_type not in SSE_EVENT_TYPES:
        LOG.warning("broadcast: unknown event_type=%r — dropping", event_type)
        return 0
    envelope = {
        "type": event_type,
        "ts_ms": int(time.time() * 1000),
        "payload": payload,
    }
    delivered = 0
    for queue in list(_subscribers):  # copy: subscribers may mutate concurrently
        try:
            queue.put_nowait(envelope)
            delivered += 1
        except asyncio.QueueFull:
            LOG.warning(
                "broadcast: subscriber queue full — dropping event=%s", event_type,
            )
        except Exception as exc:  # noqa: BLE001 — broadcast must never raise
            LOG.warning("broadcast: unexpected error %s — dropping event", exc)
    return delivered


# ---------------------------------------------------------------------------
# Clock provider — overridable in tests
# ---------------------------------------------------------------------------


class Clock:
    """Wall-clock provider returning epoch milliseconds.

    Tests inject a `MutableClock` to advance virtual time without sleeping.
    """

    def now_ms(self) -> int:
        return int(time.time() * 1000)


class MutableClock(Clock):
    """Deterministic clock for tests. Set `now_ms_value` to advance time."""

    def __init__(self, start_ms: int = 0) -> None:
        self.now_ms_value = start_ms

    def now_ms(self) -> int:  # type: ignore[override]
        return self.now_ms_value

    def advance(self, delta_ms: int) -> None:
        self.now_ms_value += delta_ms


# ---------------------------------------------------------------------------
# Anti-cheat session tracker (in-memory, single-process)
# ---------------------------------------------------------------------------


class SessionTracker:
    """Per-`(session_id, puzzle_id)` counters for anti-cheat enforcement.

    State is in-memory only ; lost on restart, which is acceptable for the
    duration of a single escape-room session. Single-process FastAPI means
    a plain dict + per-call mutation is race-safe enough (FastAPI runs each
    request handler to completion on one event loop).

    P4 extends each entry with `puzzle_started_at_ms` and
    `failed_attempts_for_puzzle` to drive adaptive level selection.
    """

    # Default shape of a fresh entry — kept centralised so .get(),
    # .commit() and the new P4 helpers stay in sync.
    _EMPTY_ENTRY: Dict[str, int] = {
        "count": 0,
        "last_at_ms": 0,
        "total_penalty": 0,
        "puzzle_started_at_ms": 0,
        "failed_attempts_for_puzzle": 0,
    }

    def __init__(
        self,
        *,
        cooldown_s: int,
        max_per_puzzle: int,
        penalty_per_level: Dict[int, int],
        clock: Clock,
    ) -> None:
        self.cooldown_s = cooldown_s
        self.max_per_puzzle = max_per_puzzle
        self.penalty_per_level = penalty_per_level
        self.clock = clock
        # (session_id, puzzle_id) -> entry dict (see _EMPTY_ENTRY)
        self._state: Dict[Tuple[str, str], Dict[str, int]] = {}

    # -- read helpers -------------------------------------------------------

    def get(self, session_id: str, puzzle_id: str) -> Dict[str, int]:
        return self._state.get((session_id, puzzle_id), dict(self._EMPTY_ENTRY))

    def _ensure(self, session_id: str, puzzle_id: str) -> Dict[str, int]:
        """Return the live entry for (sid, pid), creating it if absent."""
        key = (session_id, puzzle_id)
        entry = self._state.get(key)
        if entry is None:
            entry = dict(self._EMPTY_ENTRY)
            self._state[key] = entry
        return entry

    # -- P4 mutators (puzzle_start + attempt_failed + first-ask side-effect) -

    def mark_puzzle_started(
        self, session_id: str, puzzle_id: str, *, force: bool = False,
    ) -> int:
        """Record `puzzle_started_at_ms` if not already set.

        Returns the effective `puzzle_started_at_ms`. If `force=True`, resets
        the timestamp even when one already exists (used by the explicit
        /hints/puzzle_start endpoint when the firmware wants to restart the
        clock — though current default is to NOT force).
        """
        entry = self._ensure(session_id, puzzle_id)
        changed = False
        if entry.get("puzzle_started_at_ms", 0) == 0 or force:
            entry["puzzle_started_at_ms"] = self.clock.now_ms()
            changed = True
        # P5 SSE: only broadcast when the timestamp actually moved, so the
        # dashboard isn't spammed by idempotent firmware retries.
        if changed:
            _broadcast_event(
                "puzzle_start",
                {
                    "session_id": session_id,
                    "puzzle_id": puzzle_id,
                    "puzzle_started_at_ms": int(entry["puzzle_started_at_ms"]),
                    "forced": bool(force),
                },
            )
        return int(entry["puzzle_started_at_ms"])

    def increment_failed_attempts(
        self, session_id: str, puzzle_id: str,
    ) -> int:
        """Bump the failed-attempts counter for a puzzle. Returns new count."""
        entry = self._ensure(session_id, puzzle_id)
        entry["failed_attempts_for_puzzle"] = (
            int(entry.get("failed_attempts_for_puzzle", 0)) + 1
        )
        # First failed attempt also seeds the puzzle-started clock if absent —
        # the player is clearly engaged on this puzzle now.
        if entry.get("puzzle_started_at_ms", 0) == 0:
            entry["puzzle_started_at_ms"] = self.clock.now_ms()
        new_count = int(entry["failed_attempts_for_puzzle"])
        # P5 SSE
        _broadcast_event(
            "attempt_failed",
            {
                "session_id": session_id,
                "puzzle_id": puzzle_id,
                "failed_attempts_for_puzzle": new_count,
                "puzzle_started_at_ms": int(entry.get("puzzle_started_at_ms", 0)),
            },
        )
        return new_count

    def cooldown_until_ms(self, entry: Dict[str, int]) -> int:
        return int(entry.get("last_at_ms", 0)) + self.cooldown_s * 1000

    def penalty_for_level(self, level: int) -> int:
        return int(self.penalty_per_level.get(level, 0))

    # -- decisions ----------------------------------------------------------

    def check(
        self, session_id: str, puzzle_id: str, level_requested: int,
    ) -> Dict[str, Any]:
        """Decide whether a hint can be served and what level to serve.

        Returns a dict describing the decision. Does NOT mutate state ;
        caller calls `commit()` if it wants the served hint accounted for.

        Decision keys:
          - allow:           bool
          - reason:          "rate_limit" | "cooldown" | None
          - level_served:    int (auto-bumped if needed)
          - level_requested: int (echoed)
          - count_before:    int
          - cooldown_until_ms: int
          - retry_in_s:      int (only meaningful when reason == "cooldown")
        """
        now_ms = self.clock.now_ms()
        entry = self.get(session_id, puzzle_id)
        count = int(entry.get("count", 0))
        last_at_ms = int(entry.get("last_at_ms", 0))

        # Cap: if we've already served `max_per_puzzle`, no more hints.
        if count >= self.max_per_puzzle:
            return {
                "allow": False,
                "reason": "rate_limit",
                "level_served": min(count, max(ALL_LEVELS)),
                "level_requested": level_requested,
                "count_before": count,
                "cooldown_until_ms": self.cooldown_until_ms(entry),
                "retry_in_s": 0,
            }

        # Cooldown: minimum gap between two served hints for same puzzle.
        if count > 0 and self.cooldown_s > 0:
            cooldown_end = last_at_ms + self.cooldown_s * 1000
            if now_ms < cooldown_end:
                return {
                    "allow": False,
                    "reason": "cooldown",
                    "level_served": max(level_requested, count + 1),
                    "level_requested": level_requested,
                    "count_before": count,
                    "cooldown_until_ms": cooldown_end,
                    "retry_in_s": max(0, (cooldown_end - now_ms + 999) // 1000),
                }

        # Auto-bump: a player cannot redemand an easier hint than their progress.
        # If they ask level=1 after already getting 1 hint, force level=2.
        level_served = max(level_requested, count + 1)
        if level_served > max(ALL_LEVELS):
            level_served = max(ALL_LEVELS)

        return {
            "allow": True,
            "reason": None,
            "level_served": level_served,
            "level_requested": level_requested,
            "count_before": count,
            "cooldown_until_ms": last_at_ms + self.cooldown_s * 1000,
            "retry_in_s": 0,
        }

    def commit(
        self, session_id: str, puzzle_id: str, level_served: int,
    ) -> Dict[str, int]:
        """Account for a served hint — bump counters, return updated entry.

        P4: preserves `puzzle_started_at_ms` and `failed_attempts_for_puzzle`,
        and seeds `puzzle_started_at_ms` on the very first ask if absent.
        """
        now_ms = self.clock.now_ms()
        entry = self._ensure(session_id, puzzle_id)
        entry["count"] = int(entry.get("count", 0)) + 1
        entry["last_at_ms"] = now_ms
        entry["total_penalty"] = (
            int(entry.get("total_penalty", 0)) + self.penalty_for_level(level_served)
        )
        # First /hints/ask for this puzzle implicitly starts the clock —
        # firmware may also POST /hints/puzzle_start explicitly.
        if entry.get("puzzle_started_at_ms", 0) == 0:
            entry["puzzle_started_at_ms"] = now_ms
        # P5 SSE
        _broadcast_event(
            "hint_served",
            {
                "session_id": session_id,
                "puzzle_id": puzzle_id,
                "level_served": int(level_served),
                "count": int(entry["count"]),
                "score_penalty": self.penalty_for_level(level_served),
                "total_penalty": int(entry["total_penalty"]),
                "cooldown_until_ms": self.cooldown_until_ms(entry),
            },
        )
        return dict(entry)

    # -- admin --------------------------------------------------------------

    def list_sessions(self) -> List[Dict[str, Any]]:
        """Group state by session_id for admin debug."""
        by_session: Dict[str, List[Dict[str, Any]]] = {}
        for (sid, pid), entry in self._state.items():
            by_session.setdefault(sid, []).append({
                "puzzle_id": pid,
                "count": int(entry.get("count", 0)),
                "last_at_ms": int(entry.get("last_at_ms", 0)),
                "total_penalty": int(entry.get("total_penalty", 0)),
                "cooldown_until_ms": self.cooldown_until_ms(entry),
                "puzzle_started_at_ms": int(entry.get("puzzle_started_at_ms", 0)),
                "failed_attempts_for_puzzle": int(
                    entry.get("failed_attempts_for_puzzle", 0)
                ),
            })
        out = []
        for sid, puzzles in sorted(by_session.items()):
            out.append({
                "session_id": sid,
                "puzzles": sorted(puzzles, key=lambda p: p["puzzle_id"]),
                "total_penalty": sum(p["total_penalty"] for p in puzzles),
                "total_hints": sum(p["count"] for p in puzzles),
            })
        return out

    def reset_session(self, session_id: str) -> int:
        """Remove all entries for `session_id`. Returns # entries removed."""
        keys = [k for k in self._state if k[0] == session_id]
        for k in keys:
            del self._state[k]
        # P5 SSE — broadcast even when nothing was removed; the dashboard may
        # use this to clear stale UI state regardless.
        _broadcast_event(
            "session_reset",
            {
                "session_id": session_id,
                "entries_removed": len(keys),
            },
        )
        return len(keys)


# ---------------------------------------------------------------------------
# Phrase bank loading + validation
# ---------------------------------------------------------------------------


def _extract_text(entry: Any) -> str:
    """Return the human-readable phrase from an entry (dict with 'text' or bare str)."""
    if isinstance(entry, dict):
        val = entry.get("text", "")
        return val if isinstance(val, str) else ""
    if isinstance(entry, str):
        return entry
    return ""


def _normalize_level_block(block: Any) -> List[str]:
    """Coerce a level block into a list of non-empty strings."""
    if block is None:
        return []
    if isinstance(block, list):
        out: List[str] = []
        for entry in block:
            text = _extract_text(entry).strip()
            if text:
                out.append(text)
        return out
    if isinstance(block, str):
        return [block.strip()] if block.strip() else []
    if isinstance(block, dict):
        text = _extract_text(block).strip()
        return [text] if text else []
    return []


def load_phrase_bank(path: Path) -> Dict[str, Dict[str, List[str]]]:
    """Load and normalize hints from `npc_phrases.yaml`.

    Returns a flat mapping: `{puzzle_id: {"level_1": [str, ...], ...}}`.
    Pulls from two places to honor the YAML's authorial intent:
      1. Top-level `hints.<puzzle_id>.level_N`  (canonical)
      2. `endings.<puzzle_id>.level_N` where puzzle_id matches PUZZLE_ID_PATTERN
         (the V3 P1-P7 hints currently mis-nested under `endings:`)

    Raises FileNotFoundError or ValueError on schema problems.
    """
    if not path.is_file():
        raise FileNotFoundError(f"npc_phrases.yaml not found at {path}")

    with path.open("r", encoding="utf-8") as f:
        data = yaml.safe_load(f)

    if not isinstance(data, dict):
        raise ValueError("npc_phrases.yaml: root is not a YAML mapping")

    hints_section = data.get("hints")
    if not isinstance(hints_section, dict):
        raise ValueError("npc_phrases.yaml: missing or non-dict top-level 'hints'")

    bank: Dict[str, Dict[str, List[str]]] = {}

    for puzzle_id, levels in hints_section.items():
        if not isinstance(levels, dict):
            continue
        bank[puzzle_id] = {
            f"level_{n}": _normalize_level_block(levels.get(f"level_{n}"))
            for n in ALL_LEVELS
        }

    endings_section = data.get("endings")
    if isinstance(endings_section, dict):
        for puzzle_id, levels in endings_section.items():
            if not PUZZLE_ID_PATTERN.match(puzzle_id):
                continue
            if not isinstance(levels, dict):
                continue
            if puzzle_id in bank:
                continue
            bank[puzzle_id] = {
                f"level_{n}": _normalize_level_block(levels.get(f"level_{n}"))
                for n in ALL_LEVELS
            }

    if not bank:
        raise ValueError(
            "npc_phrases.yaml: no usable puzzle hints found "
            "(expected mapping under 'hints.<puzzle_id>.level_N')"
        )

    return bank


def load_safety_bank(path: Path) -> Dict[str, List[str]]:
    """Load `hints_safety.yaml` and return `{puzzle_id: [banned_word, ...]}`.

    Empty mapping if the file is absent (P2 default, no filtering anywhere).
    Banned words are stored as-is; comparison is normalized at filter time.
    """
    if not path.is_file():
        LOG.info("hints_safety.yaml not found at %s — running with no safety filter", path)
        return {}
    with path.open("r", encoding="utf-8") as f:
        data = yaml.safe_load(f) or {}
    if not isinstance(data, dict):
        return {}
    raw = data.get("banned_per_puzzle") or {}
    if not isinstance(raw, dict):
        return {}
    out: Dict[str, List[str]] = {}
    for puzzle_id, words in raw.items():
        if not isinstance(words, list):
            continue
        cleaned = [str(w).strip() for w in words if isinstance(w, (str, int, float)) and str(w).strip()]
        if cleaned:
            out[str(puzzle_id)] = cleaned
    return out


# ---------------------------------------------------------------------------
# Adaptive config (P4)
# ---------------------------------------------------------------------------


# Sentinel returned when the YAML is absent — adaptive mode disabled, the
# floor logic short-circuits to 0 (no auto-bump from group / stuck / fails).
_DISABLED_ADAPTIVE: Dict[str, Any] = {
    "enabled": False,
    "profiles": {},
    "failed_attempts": {"bump_every": 0, "max_bump": 0},
}


def load_adaptive_config(path: Path) -> Dict[str, Any]:
    """Load `hints_adaptive.yaml` and validate its minimal schema.

    Returns a dict with keys:
      - enabled: bool — False when the file is absent or unparseable
      - profiles: {profile_name: {base_modifier, stuck_minutes_per_bump,
                                  max_auto_bump}}
      - failed_attempts: {bump_every, max_bump}

    Missing files are tolerated (warning logged, adaptive mode disabled).
    Malformed files raise ValueError so misconfiguration surfaces at boot.
    """
    if not path.is_file():
        LOG.warning(
            "hints_adaptive.yaml not found at %s — adaptive mode DISABLED",
            path,
        )
        return dict(_DISABLED_ADAPTIVE)

    with path.open("r", encoding="utf-8") as f:
        data = yaml.safe_load(f)

    if not isinstance(data, dict):
        raise ValueError(
            f"hints_adaptive.yaml: root must be a YAML mapping, got {type(data).__name__}"
        )

    profiles_raw = data.get("profiles") or {}
    if not isinstance(profiles_raw, dict) or not profiles_raw:
        raise ValueError(
            "hints_adaptive.yaml: missing or empty 'profiles' mapping"
        )

    profiles: Dict[str, Dict[str, int]] = {}
    for name, body in profiles_raw.items():
        if not isinstance(body, dict):
            raise ValueError(
                f"hints_adaptive.yaml: profile {name!r} must be a mapping"
            )
        try:
            profiles[str(name)] = {
                "base_modifier": int(body.get("base_modifier", 0)),
                "stuck_minutes_per_bump": int(
                    body.get("stuck_minutes_per_bump", 0)
                ),
                "max_auto_bump": int(body.get("max_auto_bump", 0)),
            }
        except (TypeError, ValueError) as exc:
            raise ValueError(
                f"hints_adaptive.yaml: profile {name!r} has non-integer field: {exc}"
            ) from exc

    fa_raw = data.get("failed_attempts") or {}
    if not isinstance(fa_raw, dict):
        raise ValueError(
            "hints_adaptive.yaml: 'failed_attempts' must be a mapping when present"
        )
    try:
        failed_attempts = {
            "bump_every": int(fa_raw.get("bump_every", 0)),
            "max_bump": int(fa_raw.get("max_bump", 0)),
        }
    except (TypeError, ValueError) as exc:
        raise ValueError(
            f"hints_adaptive.yaml: failed_attempts has non-integer field: {exc}"
        ) from exc

    return {
        "enabled": True,
        "profiles": profiles,
        "failed_attempts": failed_attempts,
    }


def compute_level_floor(
    *,
    config: Dict[str, Any],
    profile: str,
    stuck_ms: int,
    failed_attempts: int,
) -> int:
    """Return the adaptive *floor* on the level to serve.

    Semantic: the floor is the minimum **absolute** level the engine should
    serve given the adaptive context. It combines additively at the call
    site as ``level_served = max(level_requested, count + 1, floor)``.

    Computation:
      - Start from the base level (= 1) IF there is any adaptive contribution
        (profile base_modifier > 0, stuck-timer bumps, or failed-attempt
        bumps). Otherwise return 0 — meaning "no adaptive opinion", so the
        existing P3 logic decides freely.
      - Add `base_modifier` (constant per profile, usually 0).
      - Add `min(max_auto_bump, stuck_minutes // stuck_minutes_per_bump)`.
      - Add `min(failed_attempts.max_bump,
                 failed_attempts // failed_attempts.bump_every)`.

    Returns 0 when adaptive mode is disabled or the profile is unknown —
    callers then fall back to the existing P3 logic. Final clamping to
    NPC_MAX_HINT_LEVEL happens at the call site.
    """
    if not config.get("enabled"):
        return 0
    profiles = config.get("profiles") or {}
    p = profiles.get(profile)
    if p is None:
        return 0

    base = int(p.get("base_modifier", 0))

    # Stuck-timer bump
    per_bump = max(0, int(p.get("stuck_minutes_per_bump", 0)))
    max_stuck_bump = max(0, int(p.get("max_auto_bump", 0)))
    if per_bump > 0 and stuck_ms > 0 and max_stuck_bump > 0:
        stuck_minutes = stuck_ms // 60_000
        stuck_bump = min(max_stuck_bump, stuck_minutes // per_bump)
    else:
        stuck_bump = 0

    # Failed-attempts bump (independent of the stuck-timer)
    fa_cfg = config.get("failed_attempts") or {}
    fa_every = max(0, int(fa_cfg.get("bump_every", 0)))
    fa_max = max(0, int(fa_cfg.get("max_bump", 0)))
    if fa_every > 0 and failed_attempts > 0 and fa_max > 0:
        fa_bump = min(fa_max, failed_attempts // fa_every)
    else:
        fa_bump = 0

    total_bump = int(stuck_bump) + int(fa_bump)
    # If nothing nudges us, return 0 (no opinion)
    if total_bump <= 0 and base <= 0:
        return 0

    # Floor is at least level 1 (base level), plus any adaptive bumps
    floor = 1 + base + total_bump
    if floor < 0:
        floor = 0
    return floor


def compute_coverage(bank: Dict[str, Dict[str, List[str]]]) -> Dict[str, Any]:
    """Build the audit summary returned by GET /hints/coverage."""
    per_puzzle: List[Dict[str, Any]] = []
    buckets = {0: 0, 1: 0, 2: 0, 3: 0}
    for puzzle_id, levels in sorted(bank.items()):
        present = [n for n in ALL_LEVELS if levels.get(f"level_{n}")]
        missing = [n for n in ALL_LEVELS if not levels.get(f"level_{n}")]
        buckets[len(present)] += 1
        per_puzzle.append({
            "puzzle_id": puzzle_id,
            "levels_present": present,
            "levels_missing": missing,
            "phrase_counts": {
                f"level_{n}": len(levels.get(f"level_{n}", [])) for n in ALL_LEVELS
            },
        })
    total = sum(buckets.values())
    pct = {
        str(k): round((v / total) * 100, 1) if total else 0.0
        for k, v in buckets.items()
    }
    return {
        "total_puzzles": total,
        "puzzles_by_level_count": {str(k): v for k, v in buckets.items()},
        "percent_by_level_count": pct,
        "per_puzzle": per_puzzle,
    }


def count_phrases(bank: Dict[str, Dict[str, List[str]]]) -> int:
    """Total number of distinct phrase strings across the whole bank."""
    return sum(len(level) for puzzle in bank.values() for level in puzzle.values())


# ---------------------------------------------------------------------------
# Safety filter
# ---------------------------------------------------------------------------


def _strip_accents(s: str) -> str:
    """Lower + strip diacritics for case/accent-insensitive matching."""
    nfkd = unicodedata.normalize("NFKD", s)
    return "".join(ch for ch in nfkd if not unicodedata.combining(ch)).lower()


def safety_check(text: str, banned_words: List[str]) -> Optional[str]:
    """Return the first matched banned word if `text` contains one, else None.

    Matching is case- and accent-insensitive on a normalized substring.
    Empty `banned_words` means "no filter" → always None.
    """
    if not banned_words:
        return None
    haystack = _strip_accents(text)
    for word in banned_words:
        needle = _strip_accents(word)
        if needle and needle in haystack:
            return word
    return None


# ---------------------------------------------------------------------------
# LiteLLM call (mockable in tests via monkeypatch)
# ---------------------------------------------------------------------------


def _build_messages(hint_static: str, puzzle_id: str, level: int) -> List[Dict[str, str]]:
    user_prompt = (
        f"Niveau {level}, énigme {puzzle_id}. "
        f"Phrase originale à reformuler : « {hint_static} »"
    )
    return [
        {"role": "system", "content": ZACUS_SYSTEM_PROMPT},
        {"role": "user", "content": user_prompt},
    ]


async def _call_litellm(
    *,
    base_url: str,
    api_key: str,
    model: str,
    messages: List[Dict[str, str]],
    timeout_s: float,
    max_tokens: int,
    temperature: float,
) -> Tuple[str, Dict[str, Any]]:
    """POST to LiteLLM `/v1/chat/completions`, return (content, raw_json).

    Raises `asyncio.TimeoutError`, `httpx.HTTPError`, or `RuntimeError` on
    a non-200 / malformed response. Callers MUST catch and fall back.
    Tests monkeypatch this whole function to avoid network I/O.
    """
    payload = {
        "model": model,
        "messages": messages,
        "max_tokens": max_tokens,
        "temperature": temperature,
    }
    async with httpx.AsyncClient(timeout=timeout_s) as client:
        resp = await client.post(
            f"{base_url}/v1/chat/completions",
            headers={"Authorization": f"Bearer {api_key}"},
            json=payload,
        )
    if resp.status_code != 200:
        raise RuntimeError(f"litellm http {resp.status_code}: {resp.text[:200]}")
    data = resp.json()
    try:
        content = data["choices"][0]["message"]["content"].strip()
    except (KeyError, IndexError, TypeError) as exc:
        raise RuntimeError(f"litellm malformed response: {exc}") from exc
    if not content:
        raise RuntimeError("litellm returned empty content")
    return content, data


# ---------------------------------------------------------------------------
# Pydantic request/response models
# ---------------------------------------------------------------------------


class HintAskRequest(BaseModel):
    puzzle_id: str = Field(..., min_length=1)
    level: conint(ge=1, le=3)  # type: ignore[valid-type]
    # session_id is logically required (validated in handler → HTTP 400).
    # Kept Optional[str] in the model so we can return a friendly 400 body
    # rather than Pydantic's 422 envelope, which the firmware doesn't parse.
    session_id: Optional[str] = None
    # P4: optional group profile (TECH | NON_TECH | MIXED | BOTH).
    # Anything outside the enum falls back to MIXED with a warning log.
    group_profile: Optional[str] = None


class HintAskResponse(BaseModel):
    """Successful served hint OR refusal envelope (transport always 200).

    Refusal: `refused=True` + `reason in {"rate_limit", "cooldown"}`.
    Served:  `refused=False` + populated `hint` / `hint_static`.
    """
    refused: bool = False
    reason: Optional[str] = None
    hint: Optional[str] = None
    hint_static: Optional[str] = None
    hint_rewritten: Optional[str] = None
    level: Optional[int] = None
    level_requested: Optional[int] = None
    level_served: Optional[int] = None
    puzzle_id: str
    source: str  # "static" | "llm_rewritten" | "llm_fallback_static" | "refused"
    model_used: str  # "hints-deep" | "none"
    latency_ms: float
    # Anti-cheat surface (always present, even on refusal)
    count: int = 0
    score_penalty: int = 0
    total_penalty: int = 0
    cooldown_until_ms: int = 0
    retry_in_s: int = 0
    details: Optional[str] = None
    # P4 adaptive surface (always present; zeros when adaptive disabled)
    level_floor_adaptive: int = 0
    stuck_minutes: int = 0
    failed_attempts: int = 0
    group_profile_used: Optional[str] = None


class HintRewriteRequest(BaseModel):
    puzzle_id: str = Field(..., min_length=1)
    level: conint(ge=1, le=3)  # type: ignore[valid-type]
    session_id: Optional[str] = None
    max_tokens: Optional[conint(ge=8, le=512)] = None  # type: ignore[valid-type]
    temperature: Optional[float] = Field(None, ge=0.0, le=2.0)


class HintRewriteResponse(BaseModel):
    hint_static: str
    hint_rewritten: str
    level: int
    puzzle_id: str
    model_used: str
    latency_ms: float
    tokens_used: Optional[int] = None


# P4 — puzzle lifecycle helpers (firmware-driven)
class PuzzleStartRequest(BaseModel):
    session_id: str = Field(..., min_length=1)
    puzzle_id: str = Field(..., min_length=1)
    # When true, resets the timestamp even if one already exists. Default
    # behaviour is no-op when the puzzle is already started, so the firmware
    # can safely call it on every retry.
    force: Optional[bool] = False


class AttemptFailedRequest(BaseModel):
    session_id: str = Field(..., min_length=1)
    puzzle_id: str = Field(..., min_length=1)


# ---------------------------------------------------------------------------
# FastAPI app factory
# ---------------------------------------------------------------------------


def _resolve_phrases_path() -> Path:
    override = os.environ.get(PHRASES_PATH_ENV)
    return Path(override).resolve() if override else DEFAULT_PHRASES_PATH


def _resolve_safety_path() -> Path:
    override = os.environ.get(SAFETY_PATH_ENV)
    return Path(override).resolve() if override else DEFAULT_SAFETY_PATH


def _resolve_adaptive_path() -> Path:
    override = os.environ.get(ADAPTIVE_PATH_ENV)
    return Path(override).resolve() if override else DEFAULT_ADAPTIVE_PATH


def _emit_log(record: Dict[str, Any]) -> None:
    """Structured JSON log line on stdout (one dict per request)."""
    sys.stdout.write(json.dumps(record, ensure_ascii=False) + "\n")
    sys.stdout.flush()


def create_app(
    phrases_path: Optional[Path] = None,
    safety_path: Optional[Path] = None,
    *,
    clock: Optional[Clock] = None,
    adaptive_path: Optional[Path] = None,
) -> FastAPI:
    """Build the FastAPI app. Pass paths/clock for tests, defaults for prod."""
    p_path = phrases_path or _resolve_phrases_path()
    s_path = safety_path or _resolve_safety_path()
    a_path = adaptive_path or _resolve_adaptive_path()
    bank = load_phrase_bank(p_path)
    safety = load_safety_bank(s_path)
    adaptive = load_adaptive_config(a_path)
    coverage_cache = compute_coverage(bank)
    phrase_total = count_phrases(bank)

    app = FastAPI(
        title="Zacus Hints Engine",
        version="0.5.0-P5",
        description=(
            "Static NPC hint phrase lookup + LLM rewrite via LiteLLM hints-deep "
            "+ in-memory anti-cheat (cap/cooldown/penalty) "
            "+ adaptive level (group profile / stuck-timer / failed attempts) "
            "+ SSE live event stream for the dashboard. "
            "Phase P5 of the hints engine spec."
        ),
    )

    app.state.phrase_bank = bank
    app.state.safety_bank = safety
    app.state.adaptive = adaptive
    app.state.coverage = coverage_cache
    app.state.phrase_total = phrase_total
    app.state.phrases_path = str(p_path)
    app.state.safety_path = str(s_path)
    app.state.adaptive_path = str(a_path)
    app.state.litellm_url = os.environ.get("LITELLM_URL", DEFAULT_LITELLM_URL)
    app.state.litellm_key = os.environ.get("LITELLM_MASTER_KEY", DEFAULT_LITELLM_KEY)
    app.state.llm_model = os.environ.get("HINTS_LLM_MODEL", DEFAULT_LLM_MODEL)
    app.state.llm_timeout_s = float(
        os.environ.get("HINTS_LLM_TIMEOUT_S", str(DEFAULT_LLM_TIMEOUT_S))
    )

    # P3 anti-cheat config
    cooldown_s = int(os.environ.get("HINTS_COOLDOWN_S", str(DEFAULT_COOLDOWN_S)))
    max_per_puzzle = int(
        os.environ.get("HINTS_MAX_PER_PUZZLE", str(DEFAULT_MAX_PER_PUZZLE))
    )
    penalty_per_level = {
        1: int(os.environ.get("HINTS_PENALTY_L1", str(DEFAULT_PENALTY_L1))),
        2: int(os.environ.get("HINTS_PENALTY_L2", str(DEFAULT_PENALTY_L2))),
        3: int(os.environ.get("HINTS_PENALTY_L3", str(DEFAULT_PENALTY_L3))),
    }
    app.state.admin_key = os.environ.get("HINTS_ADMIN_KEY")  # None → no auth

    # Security warning: the placeholder LITELLM master key is publicly known
    # (committed to the public repo as a default). Acceptable for a closed
    # LAN, never for a public deployment. See tools/macstudio/.env.example.
    if app.state.litellm_key == DEFAULT_LITELLM_KEY:
        log.warning("HINTS using default LITELLM_MASTER_KEY — set a real one "
                    "via env (see tools/macstudio/.env.example)")
    if not app.state.admin_key:
        log.warning("HINTS_ADMIN_KEY unset — /hints/sessions admin endpoints "
                    "are open. Set HINTS_ADMIN_KEY to gate them.")

    app.state.clock = clock or Clock()
    app.state.tracker = SessionTracker(
        cooldown_s=cooldown_s,
        max_per_puzzle=max_per_puzzle,
        penalty_per_level=penalty_per_level,
        clock=app.state.clock,
    )

    def _resolve_static(puzzle_id: str, level: int) -> str:
        puzzle = app.state.phrase_bank.get(puzzle_id)
        if puzzle is None:
            raise HTTPException(
                status_code=404,
                detail=f"unknown puzzle_id: {puzzle_id!r}",
            )
        level_key = f"level_{level}"
        phrases = puzzle.get(level_key) or []
        if not phrases:
            raise HTTPException(
                status_code=404,
                detail=(
                    f"no phrases for {puzzle_id} at {level_key} "
                    f"(available: {[k for k, v in puzzle.items() if v]})"
                ),
            )
        return random.choice(phrases)

    async def _try_rewrite(
        hint_static: str,
        puzzle_id: str,
        level: int,
        *,
        max_tokens: int = DEFAULT_LLM_MAX_TOKENS,
        temperature: float = DEFAULT_LLM_TEMPERATURE,
    ) -> Tuple[Optional[str], str, Optional[str], Optional[int]]:
        """Best-effort LLM rewrite.

        Returns (rewritten_text_or_none, source_tag, safety_trigger_word_or_none,
        tokens_used_or_none). `source_tag` is "llm_rewritten" on success and
        "llm_fallback_static" on any failure (timeout / error / safety trip).
        """
        messages = _build_messages(hint_static, puzzle_id, level)
        try:
            content, raw = await asyncio.wait_for(
                _call_litellm(
                    base_url=app.state.litellm_url,
                    api_key=app.state.litellm_key,
                    model=app.state.llm_model,
                    messages=messages,
                    timeout_s=app.state.llm_timeout_s,
                    max_tokens=max_tokens,
                    temperature=temperature,
                ),
                timeout=app.state.llm_timeout_s,
            )
        except asyncio.TimeoutError:
            LOG.warning("hints rewrite timeout for %s/level_%d", puzzle_id, level)
            return None, "llm_fallback_static", None, None
        except Exception as exc:  # noqa: BLE001  fallback covers all backend issues
            LOG.warning("hints rewrite error for %s/level_%d: %s", puzzle_id, level, exc)
            return None, "llm_fallback_static", None, None

        tokens_used = None
        usage = (raw or {}).get("usage") if isinstance(raw, dict) else None
        if isinstance(usage, dict):
            tokens_used = usage.get("total_tokens")

        banned = app.state.safety_bank.get(puzzle_id, [])
        trigger = safety_check(content, banned)
        if trigger:
            LOG.warning(
                "hints safety filter triggered for %s/level_%d on word=%r",
                puzzle_id, level, trigger,
            )
            return None, "llm_fallback_static", trigger, tokens_used

        return content, "llm_rewritten", None, tokens_used

    @app.get("/healthz")
    def healthz() -> Dict[str, Any]:
        adaptive = app.state.adaptive
        return {
            "status": "ok",
            "phrases_loaded": app.state.phrase_total,
            "puzzles_loaded": len(app.state.phrase_bank),
            "phrases_path": app.state.phrases_path,
            "safety_puzzles_loaded": len(app.state.safety_bank),
            "safety_path": app.state.safety_path,
            "litellm_url": app.state.litellm_url,
            "llm_model": app.state.llm_model,
            "adaptive_enabled": bool(adaptive.get("enabled")),
            "adaptive_path": app.state.adaptive_path,
            "adaptive_profiles": sorted((adaptive.get("profiles") or {}).keys()),
        }

    @app.get("/hints/coverage")
    def coverage() -> Dict[str, Any]:
        return app.state.coverage

    def _require_admin(request: Request) -> None:
        configured = app.state.admin_key
        if not configured:
            return  # no auth configured (dev mode)
        provided = request.headers.get("X-Admin-Key")
        if provided != configured:
            raise HTTPException(status_code=401, detail="invalid or missing X-Admin-Key")

    def _resolve_group_profile(raw: Optional[str]) -> str:
        """Validate group_profile, fallback to MIXED with a debug log."""
        if not raw:
            return DEFAULT_GROUP_PROFILE
        normalized = raw.strip().upper()
        if normalized in GROUP_PROFILES:
            return normalized
        LOG.warning(
            "unknown group_profile=%r received, falling back to %s",
            raw, DEFAULT_GROUP_PROFILE,
        )
        return DEFAULT_GROUP_PROFILE

    @app.post("/hints/ask", response_model=HintAskResponse)
    async def hints_ask(
        payload: HintAskRequest,
        request: Request,
        rewrite: bool = Query(True, description="Set false to bypass LLM and return verbatim static phrase"),
    ) -> HintAskResponse:
        started = time.perf_counter()
        status_code = 200
        source = "static"
        model_used = "none"
        safety_triggered: Optional[str] = None
        tokens_used: Optional[int] = None
        hint_static: Optional[str] = None
        hint_rewritten: Optional[str] = None
        decision: Dict[str, Any] = {}
        committed: Dict[str, int] = {}
        # P4 — adaptive surface (initialised early so the `finally` log sees them)
        group_profile_used = _resolve_group_profile(payload.group_profile)
        level_floor_adaptive = 0
        stuck_minutes = 0
        failed_attempts = 0
        try:
            # session_id is logically required for anti-cheat accounting.
            if not payload.session_id:
                raise HTTPException(
                    status_code=400,
                    detail="session_id is required for /hints/ask (anti-cheat)",
                )

            tracker: SessionTracker = app.state.tracker

            # ---- P4 adaptive floor -------------------------------------
            # Read existing puzzle state BEFORE check() so the stuck-timer is
            # measured against the (possibly already-recorded) puzzle start.
            entry_pre = tracker.get(payload.session_id, payload.puzzle_id)
            puzzle_started_ms = int(entry_pre.get("puzzle_started_at_ms", 0))
            failed_attempts = int(entry_pre.get("failed_attempts_for_puzzle", 0))
            now_ms = app.state.clock.now_ms()
            stuck_ms = (
                max(0, now_ms - puzzle_started_ms) if puzzle_started_ms else 0
            )
            stuck_minutes = stuck_ms // 60_000
            level_floor_adaptive = compute_level_floor(
                config=app.state.adaptive,
                profile=group_profile_used,
                stuck_ms=stuck_ms,
                failed_attempts=failed_attempts,
            )

            # Effective requested level seen by the cap/cooldown logic — the
            # adaptive floor lifts the requested level so the existing P3
            # "level = max(requested, count+1)" rule combines naturally with
            # the new floor without duplicating clamp logic.
            effective_requested = max(int(payload.level), int(level_floor_adaptive))
            if effective_requested > NPC_MAX_HINT_LEVEL:
                effective_requested = NPC_MAX_HINT_LEVEL

            decision = tracker.check(
                payload.session_id, payload.puzzle_id, effective_requested,
            )

            # ---- Refusal envelope (HTTP 200 + refused=true) -------------
            if not decision["allow"]:
                source = "refused"
                entry = tracker.get(payload.session_id, payload.puzzle_id)
                latency_ms = round((time.perf_counter() - started) * 1000, 2)
                if decision["reason"] == "rate_limit":
                    details = (
                        f"max {tracker.max_per_puzzle} hints per puzzle reached"
                    )
                else:  # cooldown
                    details = (
                        f"cooldown active, retry in {decision['retry_in_s']}s"
                    )
                # P5 SSE: refusals don't mutate tracker state, so they're
                # broadcast from the handler rather than from a mutator.
                _broadcast_event(
                    "hint_refused",
                    {
                        "session_id": payload.session_id,
                        "puzzle_id": payload.puzzle_id,
                        "reason": decision["reason"],
                        "level_requested": payload.level,
                        "level_served": decision.get("level_served"),
                        "count": int(entry.get("count", 0)),
                        "retry_in_s": int(decision.get("retry_in_s", 0)),
                        "cooldown_until_ms": int(
                            decision.get("cooldown_until_ms", 0)
                        ),
                    },
                )
                return HintAskResponse(
                    refused=True,
                    reason=decision["reason"],
                    puzzle_id=payload.puzzle_id,
                    level=None,
                    level_requested=payload.level,
                    level_served=decision.get("level_served"),
                    source=source,
                    model_used="none",
                    latency_ms=latency_ms,
                    count=int(entry.get("count", 0)),
                    score_penalty=0,
                    total_penalty=int(entry.get("total_penalty", 0)),
                    cooldown_until_ms=int(decision.get("cooldown_until_ms", 0)),
                    retry_in_s=int(decision.get("retry_in_s", 0)),
                    details=details,
                    level_floor_adaptive=int(level_floor_adaptive),
                    stuck_minutes=int(stuck_minutes),
                    failed_attempts=int(failed_attempts),
                    group_profile_used=group_profile_used,
                )

            # ---- Served path -------------------------------------------
            level_served = int(decision["level_served"])
            try:
                hint_static = _resolve_static(payload.puzzle_id, level_served)
            except HTTPException as resolve_exc:
                # Don't account for an unresolvable hint: counters stay clean.
                # Re-raise as-is (404 / etc) for transport-layer fidelity.
                raise

            if not rewrite:
                source = "static"
                model_used = "none"
                final_hint = hint_static
            else:
                rewritten, source, safety_triggered, tokens_used = await _try_rewrite(
                    hint_static, payload.puzzle_id, level_served
                )
                if rewritten is not None:
                    hint_rewritten = rewritten
                    final_hint = rewritten
                    model_used = app.state.llm_model
                else:
                    final_hint = hint_static
                    model_used = "none"

            # Commit the served hint to the tracker (after we know we have a phrase).
            committed = tracker.commit(
                payload.session_id, payload.puzzle_id, level_served,
            )

            latency_ms = round((time.perf_counter() - started) * 1000, 2)
            return HintAskResponse(
                refused=False,
                reason=None,
                hint=final_hint,
                hint_static=hint_static,
                hint_rewritten=hint_rewritten,
                level=level_served,
                level_requested=payload.level,
                level_served=level_served,
                puzzle_id=payload.puzzle_id,
                source=source,
                model_used=model_used,
                latency_ms=latency_ms,
                count=int(committed.get("count", 0)),
                score_penalty=tracker.penalty_for_level(level_served),
                total_penalty=int(committed.get("total_penalty", 0)),
                cooldown_until_ms=tracker.cooldown_until_ms(committed),
                retry_in_s=0,
                level_floor_adaptive=int(level_floor_adaptive),
                stuck_minutes=int(stuck_minutes),
                failed_attempts=int(failed_attempts),
                group_profile_used=group_profile_used,
            )
        except HTTPException as exc:
            status_code = exc.status_code
            raise
        finally:
            _emit_log({
                "ts": time.time(),
                "event": "hints_ask",
                "puzzle_id": payload.puzzle_id,
                "level_requested": payload.level,
                "level_served": decision.get("level_served"),
                "session_id": payload.session_id,
                "client": request.client.host if request.client else None,
                "rewrite_requested": rewrite,
                "source": source,
                "model_used": model_used,
                "tokens_used": tokens_used,
                "safety_triggered": safety_triggered,
                "refused": (not decision.get("allow", True)) if decision else False,
                "refused_reason": decision.get("reason") if decision else None,
                "count": int(committed.get("count", 0)) if committed else (
                    int(decision.get("count_before", 0)) if decision else 0
                ),
                "total_penalty": int(committed.get("total_penalty", 0)) if committed else 0,
                "group_profile_used": group_profile_used,
                "stuck_minutes": int(stuck_minutes),
                "failed_attempts": int(failed_attempts),
                "level_floor_adaptive": int(level_floor_adaptive),
                "adaptive_enabled": bool(app.state.adaptive.get("enabled")),
                "latency_ms": round((time.perf_counter() - started) * 1000, 2),
                "status": status_code,
            })

    @app.post("/hints/rewrite", response_model=HintRewriteResponse)
    async def hints_rewrite(
        payload: HintRewriteRequest, request: Request,
    ) -> HintRewriteResponse:
        """Debug-only path: forces the LLM rewrite and returns it.

        Differs from `/hints/ask` in that it surfaces a 502 if the rewrite
        fails (no silent verbatim fallback). Useful for prompt tuning.
        """
        started = time.perf_counter()
        status_code = 200
        source = "static"
        safety_triggered: Optional[str] = None
        tokens_used: Optional[int] = None
        try:
            hint_static = _resolve_static(payload.puzzle_id, payload.level)
            rewritten, source, safety_triggered, tokens_used = await _try_rewrite(
                hint_static,
                payload.puzzle_id,
                payload.level,
                max_tokens=payload.max_tokens or DEFAULT_LLM_MAX_TOKENS,
                temperature=(
                    payload.temperature
                    if payload.temperature is not None
                    else DEFAULT_LLM_TEMPERATURE
                ),
            )
            if rewritten is None:
                status_code = 502
                detail = (
                    "rewrite blocked by safety filter"
                    if safety_triggered
                    else "rewrite failed (timeout/backend error)"
                )
                raise HTTPException(status_code=502, detail=detail)
            latency_ms = round((time.perf_counter() - started) * 1000, 2)
            return HintRewriteResponse(
                hint_static=hint_static,
                hint_rewritten=rewritten,
                level=payload.level,
                puzzle_id=payload.puzzle_id,
                model_used=app.state.llm_model,
                latency_ms=latency_ms,
                tokens_used=tokens_used,
            )
        except HTTPException as exc:
            status_code = exc.status_code
            raise
        finally:
            _emit_log({
                "ts": time.time(),
                "event": "hints_rewrite",
                "puzzle_id": payload.puzzle_id,
                "level": payload.level,
                "session_id": payload.session_id,
                "client": request.client.host if request.client else None,
                "source": source,
                "model_used": app.state.llm_model,
                "tokens_used": tokens_used,
                "safety_triggered": safety_triggered,
                "latency_ms": round((time.perf_counter() - started) * 1000, 2),
                "status": status_code,
            })

    @app.post("/hints/puzzle_start")
    def hints_puzzle_start(payload: PuzzleStartRequest) -> Dict[str, Any]:
        """Mark `puzzle_started_at_ms` so the stuck-timer can run.

        Idempotent: subsequent calls without `force=true` are no-ops on the
        timestamp (firmware can spam this on every retry-attach).
        """
        tracker: SessionTracker = app.state.tracker
        started_ms = tracker.mark_puzzle_started(
            payload.session_id, payload.puzzle_id, force=bool(payload.force),
        )
        entry = tracker.get(payload.session_id, payload.puzzle_id)
        _emit_log({
            "ts": time.time(),
            "event": "hints_puzzle_start",
            "session_id": payload.session_id,
            "puzzle_id": payload.puzzle_id,
            "force": bool(payload.force),
            "puzzle_started_at_ms": started_ms,
        })
        return {
            "ok": True,
            "session_id": payload.session_id,
            "puzzle_id": payload.puzzle_id,
            "puzzle_started_at_ms": started_ms,
            "failed_attempts_for_puzzle": int(
                entry.get("failed_attempts_for_puzzle", 0)
            ),
            "now_ms": app.state.clock.now_ms(),
        }

    @app.post("/hints/attempt_failed")
    def hints_attempt_failed(payload: AttemptFailedRequest) -> Dict[str, Any]:
        """Increment failed_attempts_for_puzzle (firmware-driven).

        Returns the new count. Returns 404 only when the puzzle_id is unknown
        to the phrase bank — otherwise the entry is created lazily.
        """
        if payload.puzzle_id not in app.state.phrase_bank:
            raise HTTPException(
                status_code=404,
                detail=f"unknown puzzle_id: {payload.puzzle_id!r}",
            )
        tracker: SessionTracker = app.state.tracker
        new_count = tracker.increment_failed_attempts(
            payload.session_id, payload.puzzle_id,
        )
        entry = tracker.get(payload.session_id, payload.puzzle_id)
        _emit_log({
            "ts": time.time(),
            "event": "hints_attempt_failed",
            "session_id": payload.session_id,
            "puzzle_id": payload.puzzle_id,
            "failed_attempts_for_puzzle": new_count,
        })
        return {
            "ok": True,
            "session_id": payload.session_id,
            "puzzle_id": payload.puzzle_id,
            "failed_attempts_for_puzzle": new_count,
            "puzzle_started_at_ms": int(entry.get("puzzle_started_at_ms", 0)),
            "now_ms": app.state.clock.now_ms(),
        }

    @app.get("/hints/sessions")
    def admin_list_sessions(request: Request) -> Dict[str, Any]:
        _require_admin(request)
        tracker: SessionTracker = app.state.tracker
        sessions = tracker.list_sessions()
        return {
            "sessions": sessions,
            "total_sessions": len(sessions),
            "config": {
                "cooldown_s": tracker.cooldown_s,
                "max_per_puzzle": tracker.max_per_puzzle,
                "penalty_per_level": tracker.penalty_per_level,
                "adaptive": app.state.adaptive,
            },
            "now_ms": app.state.clock.now_ms(),
        }

    @app.delete("/hints/sessions/{session_id}")
    def admin_reset_session(session_id: str, request: Request) -> Dict[str, Any]:
        _require_admin(request)
        tracker: SessionTracker = app.state.tracker
        removed = tracker.reset_session(session_id)
        return {"session_id": session_id, "entries_removed": removed, "ok": True}

    # ------------------------------------------------------------------
    # P5 — SSE live event stream (GET /hints/events)
    # ------------------------------------------------------------------

    @app.get("/hints/events")
    async def hints_events(request: Request) -> EventSourceResponse:
        """Server-Sent Events stream for the dashboard.

        Each subscriber gets its own bounded asyncio.Queue (capacity
        SSE_QUEUE_MAXSIZE). The generator yields envelopes pushed by
        `_broadcast_event`. On client disconnect (CancelledError or the
        ASGI receive channel reporting `http.disconnect`), the queue is
        removed from the global subscriber list. `EventSourceResponse`'s
        own `ping` keep-alives every SSE_HEARTBEAT_S seconds keep the
        socket open through proxies.

        The dashboard is expected to consume this with the native
        EventSource API (no extra JS lib needed).
        """
        queue: "asyncio.Queue[Dict[str, Any]]" = asyncio.Queue(
            maxsize=SSE_QUEUE_MAXSIZE,
        )
        _subscribers.append(queue)
        LOG.info(
            "SSE subscriber connected (total=%d) from %s",
            len(_subscribers),
            request.client.host if request.client else "?",
        )

        async def event_stream():
            try:
                while True:
                    if await request.is_disconnected():
                        break
                    try:
                        envelope = await asyncio.wait_for(
                            queue.get(), timeout=SSE_HEARTBEAT_S,
                        )
                    except asyncio.TimeoutError:
                        # Let sse-starlette's built-in ping keep the socket
                        # warm; we just loop and re-check disconnect.
                        continue
                    yield {
                        "event": envelope["type"],
                        "data": json.dumps(envelope, ensure_ascii=False),
                    }
            except asyncio.CancelledError:
                # Client closed cleanly; just propagate after cleanup below.
                raise
            finally:
                try:
                    _subscribers.remove(queue)
                except ValueError:
                    pass
                LOG.info(
                    "SSE subscriber disconnected (remaining=%d)",
                    len(_subscribers),
                )

        return EventSourceResponse(event_stream(), ping=SSE_HEARTBEAT_S)

    @app.get("/hints/events/test")
    def hints_events_test(request: Request) -> Dict[str, Any]:
        """Trigger a synthetic broadcast — handy when wiring the dashboard."""
        _require_admin(request)
        delivered = _broadcast_event(
            "test",
            {
                "message": "Hello depuis Zacus — événement de test SSE.",
                "now_ms": app.state.clock.now_ms(),
            },
        )
        return {
            "ok": True,
            "subscribers": len(_subscribers),
            "delivered": delivered,
        }

    return app


# Module-level app for `uvicorn tools.hints.server:app`
app = create_app()
