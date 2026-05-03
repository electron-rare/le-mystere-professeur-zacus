"""Hints engine — Phase P2 FastAPI server.

Static phrase lookup (P1) + LLM rewrite layer via LiteLLM `hints-deep` (P2).
The rewriter restyles the static base phrase in the Professor Zacus voice
without inventing new content. Safety filter blocks rewrites that leak
solution tokens. On timeout / error / safety trip, the engine falls back
to the verbatim static phrase so the game never blocks.

Endpoints:
  - GET  /healthz            — liveness probe + phrases-loaded count
  - POST /hints/ask          — return base phrase, optionally LLM-rewritten
                               (?rewrite=false to bypass LLM)
  - POST /hints/rewrite      — debug: rewrite-only path, no static fallback
  - GET  /hints/coverage     — per-puzzle level coverage audit

Run:
  uv run --with fastapi --with uvicorn --with pyyaml --with pydantic \
         --with httpx \
      uvicorn tools.hints.server:app --reload --port 8300

Env:
  LITELLM_URL          (default http://192.168.0.120:4000)
  LITELLM_MASTER_KEY   (default sk-zacus-local-dev-do-not-share)
  HINTS_LLM_MODEL      (default hints-deep)
  HINTS_LLM_TIMEOUT_S  (default 8.0)
  HINTS_SAFETY_PATH    (default game/scenarios/hints_safety.yaml)
  ZACUS_NPC_PHRASES_PATH (default game/scenarios/npc_phrases.yaml)

Spec: docs/superpowers/specs/2026-05-03-hints-engine-design.md §4
P3 (anti-cheat) and P4 (adaptive level) are intentionally NOT in scope here.
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

LOG = logging.getLogger("hints.server")


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
    session_id: Optional[str] = None


class HintAskResponse(BaseModel):
    hint: str
    hint_static: str
    hint_rewritten: Optional[str] = None
    level: int
    puzzle_id: str
    source: str  # "static" | "llm_rewritten" | "llm_fallback_static"
    model_used: str  # "hints-deep" | "none"
    latency_ms: float


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
) -> FastAPI:
    """Build the FastAPI app. Pass paths for tests, defaults for prod."""
    p_path = phrases_path or _resolve_phrases_path()
    s_path = safety_path or _resolve_safety_path()
    bank = load_phrase_bank(p_path)
    safety = load_safety_bank(s_path)
    coverage_cache = compute_coverage(bank)
    phrase_total = count_phrases(bank)

    app = FastAPI(
        title="Zacus Hints Engine",
        version="0.2.0-P2",
        description=(
            "Static NPC hint phrase lookup + LLM rewrite via LiteLLM hints-deep. "
            "Phase P2 of the hints engine spec."
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
        hint_static = ""
        hint_rewritten: Optional[str] = None
        try:
            hint_static = _resolve_static(payload.puzzle_id, payload.level)
            if not rewrite:
                source = "static"
                model_used = "none"
                final_hint = hint_static
            else:
                rewritten, source, safety_triggered, tokens_used = await _try_rewrite(
                    hint_static, payload.puzzle_id, payload.level
                )
                if rewritten is not None:
                    hint_rewritten = rewritten
                    final_hint = rewritten
                    model_used = app.state.llm_model
                else:
                    final_hint = hint_static
                    model_used = "none"

            latency_ms = round((time.perf_counter() - started) * 1000, 2)
            return HintAskResponse(
                hint=final_hint,
                hint_static=hint_static,
                hint_rewritten=hint_rewritten,
                level=payload.level,
                puzzle_id=payload.puzzle_id,
                source=source,
                model_used=model_used,
                latency_ms=latency_ms,
            )
        except HTTPException as exc:
            status_code = exc.status_code
            raise
        finally:
            _emit_log({
                "ts": time.time(),
                "event": "hints_ask",
                "puzzle_id": payload.puzzle_id,
                "level": payload.level,
                "session_id": payload.session_id,
                "client": request.client.host if request.client else None,
                "rewrite_requested": rewrite,
                "source": source,
                "model_used": model_used,
                "tokens_used": tokens_used,
                "safety_triggered": safety_triggered,
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

    return app


# Module-level app for `uvicorn tools.hints.server:app`
app = create_app()
