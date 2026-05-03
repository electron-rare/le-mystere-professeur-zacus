"""Hints engine — Phase P1 FastAPI server.

Stateless static phrase lookup from `game/scenarios/npc_phrases.yaml`.
Endpoints:
  - GET  /healthz        — liveness probe + phrases-loaded count
  - POST /hints/ask      — return base phrase for {puzzle_id, level}
  - GET  /hints/coverage — per-puzzle level coverage audit

Run:
  uv run --with fastapi --with uvicorn --with pyyaml --with pydantic \
      uvicorn tools.hints.server:app --reload --port 8300

Spec: docs/superpowers/specs/2026-05-03-hints-engine-design.md
P1 scope is intentionally narrow: no LLM rewrite, no anti-cheat,
no adaptive level selection, no auto-trigger. Those land in P2-P6.
"""
from __future__ import annotations

import json
import logging
import os
import random
import re
import sys
import time
from pathlib import Path
from typing import Any, Dict, List, Optional

import yaml
from fastapi import FastAPI, HTTPException, Request
from pydantic import BaseModel, Field, conint


# ---------------------------------------------------------------------------
# Paths and constants
# ---------------------------------------------------------------------------

REPO_ROOT = Path(__file__).resolve().parents[2]
DEFAULT_PHRASES_PATH = REPO_ROOT / "game" / "scenarios" / "npc_phrases.yaml"
PHRASES_PATH_ENV = "ZACUS_NPC_PHRASES_PATH"

# Puzzle IDs that look like V3 numbered puzzles (P1_FOO, P12_BAR, ...).
# Authoring drift: `P1_SON`..`P7_COFFRE` are nested under `endings:` in the
# current YAML (indentation bug — see audit). We surface them anyway because
# their internal `key:` values advertise `hints.<id>.level_N`. Fix later in
# `npc_phrases.yaml`; do NOT fix here (P1 instructions forbid YAML edits).
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
        # Tolerate `{text: "..."}` at the level position
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

    Raises:
      FileNotFoundError if `path` doesn't exist.
      ValueError if the YAML lacks a top-level `hints` mapping or no usable
      puzzle entries are found.
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

    # 1. Canonical hints.* puzzles
    for puzzle_id, levels in hints_section.items():
        if not isinstance(levels, dict):
            continue
        bank[puzzle_id] = {
            f"level_{n}": _normalize_level_block(levels.get(f"level_{n}"))
            for n in ALL_LEVELS
        }

    # 2. Mis-nested V3 puzzles under endings.*
    endings_section = data.get("endings")
    if isinstance(endings_section, dict):
        for puzzle_id, levels in endings_section.items():
            if not PUZZLE_ID_PATTERN.match(puzzle_id):
                continue
            if not isinstance(levels, dict):
                continue
            if puzzle_id in bank:
                continue  # canonical hints.* wins
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
# Pydantic request/response models
# ---------------------------------------------------------------------------


class HintAskRequest(BaseModel):
    puzzle_id: str = Field(..., min_length=1)
    level: conint(ge=1, le=3)  # type: ignore[valid-type]
    session_id: Optional[str] = None


class HintAskResponse(BaseModel):
    hint: str
    level: int
    puzzle_id: str
    source: str = "static"


# ---------------------------------------------------------------------------
# FastAPI app factory
# ---------------------------------------------------------------------------


def _resolve_phrases_path() -> Path:
    override = os.environ.get(PHRASES_PATH_ENV)
    return Path(override).resolve() if override else DEFAULT_PHRASES_PATH


def _emit_log(record: Dict[str, Any]) -> None:
    """Structured JSON log line on stdout (one dict per request)."""
    sys.stdout.write(json.dumps(record, ensure_ascii=False) + "\n")
    sys.stdout.flush()


def create_app(phrases_path: Optional[Path] = None) -> FastAPI:
    """Build the FastAPI app. Pass `phrases_path` for tests, default for prod."""
    path = phrases_path or _resolve_phrases_path()
    bank = load_phrase_bank(path)  # raises if cracked → uvicorn refuses to start
    coverage_cache = compute_coverage(bank)
    phrase_total = count_phrases(bank)

    app = FastAPI(
        title="Zacus Hints Engine",
        version="0.1.0-P1",
        description="Static NPC hint phrase lookup. Phase P1 of the hints engine spec.",
    )

    # Stash for handlers + tests
    app.state.phrase_bank = bank
    app.state.coverage = coverage_cache
    app.state.phrase_total = phrase_total
    app.state.phrases_path = str(path)

    @app.get("/healthz")
    def healthz() -> Dict[str, Any]:
        return {
            "status": "ok",
            "phrases_loaded": app.state.phrase_total,
            "puzzles_loaded": len(app.state.phrase_bank),
            "phrases_path": app.state.phrases_path,
        }

    @app.get("/hints/coverage")
    def coverage() -> Dict[str, Any]:
        return app.state.coverage

    @app.post("/hints/ask", response_model=HintAskResponse)
    def hints_ask(payload: HintAskRequest, request: Request) -> HintAskResponse:
        started = time.perf_counter()
        status_code = 200
        try:
            puzzle = app.state.phrase_bank.get(payload.puzzle_id)
            if puzzle is None:
                status_code = 404
                raise HTTPException(
                    status_code=404,
                    detail=f"unknown puzzle_id: {payload.puzzle_id!r}",
                )
            level_key = f"level_{payload.level}"
            phrases = puzzle.get(level_key) or []
            if not phrases:
                status_code = 404
                raise HTTPException(
                    status_code=404,
                    detail=(
                        f"no phrases for {payload.puzzle_id} at {level_key} "
                        f"(available: {[k for k, v in puzzle.items() if v]})"
                    ),
                )
            hint = random.choice(phrases)
            return HintAskResponse(
                hint=hint,
                level=payload.level,
                puzzle_id=payload.puzzle_id,
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
                "latency_ms": round((time.perf_counter() - started) * 1000, 2),
                "status": status_code,
            })

    return app


# Module-level app for `uvicorn tools.hints.server:app`
app = create_app()
