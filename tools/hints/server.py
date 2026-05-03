"""Hints engine — Phase P3 FastAPI server.

Static phrase lookup (P1) + LLM rewrite layer via LiteLLM `hints-deep` (P2)
+ in-memory anti-cheat (cap 3 / cooldown / progressive penalties) (P3).

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

Endpoints:
  - GET  /healthz                       — liveness + counts
  - POST /hints/ask                     — base phrase, optionally LLM-rewritten,
                                          with anti-cheat (cap/cooldown/penalty).
                                          ?rewrite=false bypasses LLM.
  - POST /hints/rewrite                 — debug: rewrite-only, no static fallback,
                                          no anti-cheat accounting.
  - GET  /hints/coverage                — per-puzzle level coverage audit
  - GET  /hints/sessions                — admin: list active sessions/state
  - DELETE /hints/sessions/{session_id} — admin: reset session state

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
  ZACUS_NPC_PHRASES_PATH (default game/scenarios/npc_phrases.yaml)
  HINTS_COOLDOWN_S       (default 60)   — minimum gap between hints per puzzle
  HINTS_MAX_PER_PUZZLE   (default 3)    — hard cap per (session, puzzle)
  HINTS_PENALTY_L1       (default 50)   — score penalty for level_1 hint
  HINTS_PENALTY_L2       (default 100)  — score penalty for level_2 hint
  HINTS_PENALTY_L3       (default 200)  — score penalty for level_3 hint
  HINTS_ADMIN_KEY        (default unset) — when set, admin endpoints require
                                           matching `X-Admin-Key` header.

Spec: docs/superpowers/specs/2026-05-03-hints-engine-design.md §4-§5
P4 (adaptive level) and P6 (auto-trigger) are NOT yet implemented here.
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


# ---------------------------------------------------------------------------
# Paths and constants
# ---------------------------------------------------------------------------

REPO_ROOT = Path(__file__).resolve().parents[2]
DEFAULT_PHRASES_PATH = REPO_ROOT / "game" / "scenarios" / "npc_phrases.yaml"
DEFAULT_SAFETY_PATH = REPO_ROOT / "game" / "scenarios" / "hints_safety.yaml"
PHRASES_PATH_ENV = "ZACUS_NPC_PHRASES_PATH"
SAFETY_PATH_ENV = "HINTS_SAFETY_PATH"

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

# P3 anti-cheat defaults (overridable via env at app startup)
DEFAULT_COOLDOWN_S = 60
DEFAULT_MAX_PER_PUZZLE = 3
DEFAULT_PENALTY_L1 = 50
DEFAULT_PENALTY_L2 = 100
DEFAULT_PENALTY_L3 = 200

LOG = logging.getLogger("hints.server")


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
    """

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
        # (session_id, puzzle_id) -> {count, last_at_ms, total_penalty}
        self._state: Dict[Tuple[str, str], Dict[str, int]] = {}

    # -- read helpers -------------------------------------------------------

    def get(self, session_id: str, puzzle_id: str) -> Dict[str, int]:
        return self._state.get((session_id, puzzle_id), {
            "count": 0, "last_at_ms": 0, "total_penalty": 0,
        })

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
        """Account for a served hint — bump counters, return updated entry."""
        now_ms = self.clock.now_ms()
        key = (session_id, puzzle_id)
        entry = self._state.get(key, {
            "count": 0, "last_at_ms": 0, "total_penalty": 0,
        })
        entry["count"] = int(entry.get("count", 0)) + 1
        entry["last_at_ms"] = now_ms
        entry["total_penalty"] = (
            int(entry.get("total_penalty", 0)) + self.penalty_for_level(level_served)
        )
        self._state[key] = entry
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


# ---------------------------------------------------------------------------
# FastAPI app factory
# ---------------------------------------------------------------------------


def _resolve_phrases_path() -> Path:
    override = os.environ.get(PHRASES_PATH_ENV)
    return Path(override).resolve() if override else DEFAULT_PHRASES_PATH


def _resolve_safety_path() -> Path:
    override = os.environ.get(SAFETY_PATH_ENV)
    return Path(override).resolve() if override else DEFAULT_SAFETY_PATH


def _emit_log(record: Dict[str, Any]) -> None:
    """Structured JSON log line on stdout (one dict per request)."""
    sys.stdout.write(json.dumps(record, ensure_ascii=False) + "\n")
    sys.stdout.flush()


def create_app(
    phrases_path: Optional[Path] = None,
    safety_path: Optional[Path] = None,
    *,
    clock: Optional[Clock] = None,
) -> FastAPI:
    """Build the FastAPI app. Pass paths/clock for tests, defaults for prod."""
    p_path = phrases_path or _resolve_phrases_path()
    s_path = safety_path or _resolve_safety_path()
    bank = load_phrase_bank(p_path)
    safety = load_safety_bank(s_path)
    coverage_cache = compute_coverage(bank)
    phrase_total = count_phrases(bank)

    app = FastAPI(
        title="Zacus Hints Engine",
        version="0.3.0-P3",
        description=(
            "Static NPC hint phrase lookup + LLM rewrite via LiteLLM hints-deep "
            "+ in-memory anti-cheat (cap/cooldown/penalty). "
            "Phase P3 of the hints engine spec."
        ),
    )

    app.state.phrase_bank = bank
    app.state.safety_bank = safety
    app.state.coverage = coverage_cache
    app.state.phrase_total = phrase_total
    app.state.phrases_path = str(p_path)
    app.state.safety_path = str(s_path)
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
        return {
            "status": "ok",
            "phrases_loaded": app.state.phrase_total,
            "puzzles_loaded": len(app.state.phrase_bank),
            "phrases_path": app.state.phrases_path,
            "safety_puzzles_loaded": len(app.state.safety_bank),
            "safety_path": app.state.safety_path,
            "litellm_url": app.state.litellm_url,
            "llm_model": app.state.llm_model,
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
        try:
            # session_id is logically required for anti-cheat accounting.
            if not payload.session_id:
                raise HTTPException(
                    status_code=400,
                    detail="session_id is required for /hints/ask (anti-cheat)",
                )

            tracker: SessionTracker = app.state.tracker
            decision = tracker.check(
                payload.session_id, payload.puzzle_id, payload.level,
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
            },
            "now_ms": app.state.clock.now_ms(),
        }

    @app.delete("/hints/sessions/{session_id}")
    def admin_reset_session(session_id: str, request: Request) -> Dict[str, Any]:
        _require_admin(request)
        tracker: SessionTracker = app.state.tracker
        removed = tracker.reset_session(session_id)
        return {"session_id": session_id, "entries_removed": removed, "ok": True}

    return app


# Module-level app for `uvicorn tools.hints.server:app`
app = create_app()
