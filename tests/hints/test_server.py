"""Tests for tools.hints.server (P1 static lookup + P2 LLM rewrite).

Run via:
    make hints-test
or:
    uv run --with fastapi --with uvicorn --with pyyaml --with pydantic \
           --with pytest --with httpx --with pytest-asyncio \
           pytest tests/hints/ -v
"""
from __future__ import annotations

import asyncio
import sys
from pathlib import Path
from typing import Any, Dict, List, Tuple

import pytest
from fastapi.testclient import TestClient

REPO_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO_ROOT))

from tools.hints import server as server_module  # noqa: E402
from tools.hints.server import (  # noqa: E402
    DEFAULT_PHRASES_PATH,
    DEFAULT_SAFETY_PATH,
    create_app,
    load_phrase_bank,
    safety_check,
)


# ---------------------------------------------------------------------------
# Shared fixtures
# ---------------------------------------------------------------------------


@pytest.fixture(scope="module")
def app():
    return create_app(DEFAULT_PHRASES_PATH, DEFAULT_SAFETY_PATH)


@pytest.fixture(scope="module")
def client(app):
    return TestClient(app)


@pytest.fixture(scope="module")
def real_puzzle_id():
    """First puzzle in the bank that has a non-empty level_1.

    Using a real puzzle from the actual YAML keeps the test honest.
    """
    bank = load_phrase_bank(DEFAULT_PHRASES_PATH)
    for pid, levels in bank.items():
        if levels.get("level_1"):
            return pid
    pytest.fail("no puzzle with level_1 found in npc_phrases.yaml")


def _make_fake_call(
    content: str = "Mon cher, écoutez le rythme et comptez les battements.",
    *,
    raise_exc: Exception | None = None,
    delay_s: float = 0.0,
    tokens_used: int = 42,
):
    """Build a coroutine that mimics `_call_litellm` for monkeypatching."""

    async def fake(**kwargs: Any) -> Tuple[str, Dict[str, Any]]:
        if delay_s:
            await asyncio.sleep(delay_s)
        if raise_exc is not None:
            raise raise_exc
        raw = {
            "choices": [{"message": {"content": content}}],
            "usage": {"total_tokens": tokens_used},
        }
        return content, raw

    return fake


# ---------------------------------------------------------------------------
# Health + coverage (P1 + P2 health surface)
# ---------------------------------------------------------------------------


def test_healthz_returns_ok_with_counts(client):
    resp = client.get("/healthz")
    assert resp.status_code == 200
    body = resp.json()
    assert body["status"] == "ok"
    assert body["phrases_loaded"] > 0
    assert body["puzzles_loaded"] > 0
    assert body["phrases_path"].endswith("npc_phrases.yaml")
    # P2 fields
    assert "safety_puzzles_loaded" in body
    assert "litellm_url" in body
    assert body["llm_model"]


def test_coverage_summary_shape_is_complete(client):
    resp = client.get("/hints/coverage")
    assert resp.status_code == 200
    body = resp.json()

    for key in ("total_puzzles", "puzzles_by_level_count",
                "percent_by_level_count", "per_puzzle"):
        assert key in body, f"coverage missing field {key!r}"

    bucket_sum = sum(body["puzzles_by_level_count"].values())
    assert bucket_sum == body["total_puzzles"]
    assert body["total_puzzles"] >= 1

    for row in body["per_puzzle"]:
        assert "puzzle_id" in row
        assert "levels_present" in row
        assert "levels_missing" in row
        assert "phrase_counts" in row
        assert set(row["phrase_counts"].keys()) == {"level_1", "level_2", "level_3"}


# ---------------------------------------------------------------------------
# /hints/ask happy path + errors  (verbatim path with ?rewrite=false)
# ---------------------------------------------------------------------------


def test_hints_ask_happy_path_returns_real_phrase(client, real_puzzle_id):
    resp = client.post(
        "/hints/ask?rewrite=false",
        json={"puzzle_id": real_puzzle_id, "level": 1, "session_id": "test-session"},
    )
    assert resp.status_code == 200, resp.text
    body = resp.json()
    assert body["puzzle_id"] == real_puzzle_id
    assert body["level"] == 1
    assert body["source"] == "static"
    assert body["model_used"] == "none"
    assert body["hint_rewritten"] is None
    assert isinstance(body["hint"], str)
    assert len(body["hint"]) > 0
    assert body["hint"] == body["hint_static"]

    bank = load_phrase_bank(DEFAULT_PHRASES_PATH)
    assert body["hint"] in bank[real_puzzle_id]["level_1"]


def test_hints_ask_unknown_puzzle_returns_404(client):
    resp = client.post(
        "/hints/ask?rewrite=false",
        json={"puzzle_id": "P999_DOES_NOT_EXIST", "level": 1},
    )
    assert resp.status_code == 404
    assert "unknown puzzle_id" in resp.json()["detail"]


def test_hints_ask_invalid_level_returns_422(client, real_puzzle_id):
    resp = client.post(
        "/hints/ask?rewrite=false",
        json={"puzzle_id": real_puzzle_id, "level": 7},
    )
    assert resp.status_code == 422


def test_hints_ask_preserves_french_diacritics(client, real_puzzle_id):
    """UTF-8 round-trip must keep accented characters intact (verbatim path)."""
    bank = load_phrase_bank(DEFAULT_PHRASES_PATH)
    has_accent = any(
        any(ch in phrase for ch in "éèêàùôîçÉÈÊÀÙÔÎÇ")
        for level in bank[real_puzzle_id].values()
        for phrase in level
    )
    if not has_accent:
        pytest.skip(f"{real_puzzle_id} has no accented phrases — skipping diacritic check")

    found_accent = False
    for _ in range(20):
        resp = client.post(
            "/hints/ask?rewrite=false", json={"puzzle_id": real_puzzle_id, "level": 1}
        )
        assert resp.status_code == 200
        if any(ch in resp.json()["hint"] for ch in "éèêàùôîçÉÈÊÀÙÔÎÇ"):
            found_accent = True
            break
    assert found_accent, "diacritics never round-tripped — encoding bug?"


# ---------------------------------------------------------------------------
# Safety filter unit
# ---------------------------------------------------------------------------


def test_safety_check_is_accent_and_case_insensitive():
    banned = ["Étagère gauche", "1337"]
    assert safety_check("rien à signaler", banned) is None
    assert safety_check("vise l'ETAGERE GAUCHE rapidement", banned) == "Étagère gauche"
    assert safety_check("la fréquence est 1337 hertz", banned) == "1337"
    assert safety_check("anything", []) is None


# ---------------------------------------------------------------------------
# /hints/ask P2 — LLM rewrite paths (mocked LiteLLM)
# ---------------------------------------------------------------------------


def test_ask_with_rewrite_uses_llm(client, real_puzzle_id, monkeypatch):
    """Rewrite path: mocked LLM returns clean content → source=llm_rewritten."""
    fake = _make_fake_call(
        content="Ah, mon cher disciple ! Écoutez ce rythme avec ferveur.",
        tokens_used=37,
    )
    monkeypatch.setattr(server_module, "_call_litellm", fake)

    resp = client.post(
        "/hints/ask",
        json={"puzzle_id": real_puzzle_id, "level": 1, "session_id": "rw-test"},
    )
    assert resp.status_code == 200, resp.text
    body = resp.json()
    assert body["source"] == "llm_rewritten"
    assert body["model_used"] == client.app.state.llm_model
    assert body["hint_rewritten"] == "Ah, mon cher disciple ! Écoutez ce rythme avec ferveur."
    assert body["hint"] == body["hint_rewritten"]
    assert isinstance(body["hint_static"], str) and len(body["hint_static"]) > 0
    assert body["hint_static"] != body["hint_rewritten"]


def test_ask_rewrite_fallback_on_timeout(client, real_puzzle_id, monkeypatch):
    """asyncio.TimeoutError from `_call_litellm` → verbatim fallback."""
    fake = _make_fake_call(raise_exc=asyncio.TimeoutError())
    monkeypatch.setattr(server_module, "_call_litellm", fake)

    resp = client.post(
        "/hints/ask",
        json={"puzzle_id": real_puzzle_id, "level": 1},
    )
    assert resp.status_code == 200
    body = resp.json()
    assert body["source"] == "llm_fallback_static"
    assert body["model_used"] == "none"
    assert body["hint_rewritten"] is None
    assert body["hint"] == body["hint_static"]


def test_ask_rewrite_fallback_on_safety_trigger(client, monkeypatch):
    """LLM emits a banned word → safety filter trips → verbatim fallback."""
    # P4_RADIO bans "1337" — make the LLM "leak" it.
    fake = _make_fake_call(
        content="Tournez le bouton jusqu'à ce que l'écran affiche 1337 hertz précisément.",
    )
    monkeypatch.setattr(server_module, "_call_litellm", fake)

    resp = client.post(
        "/hints/ask",
        json={"puzzle_id": "P4_RADIO", "level": 1},
    )
    assert resp.status_code == 200
    body = resp.json()
    assert body["source"] == "llm_fallback_static"
    assert body["model_used"] == "none"
    assert body["hint_rewritten"] is None
    assert body["hint"] == body["hint_static"]
    # Sanity: the static phrase itself isn't the leaky one (it stays in bank as-is)
    assert isinstance(body["hint_static"], str) and len(body["hint_static"]) > 0


def test_ask_query_rewrite_false_bypasses_llm(client, real_puzzle_id, monkeypatch):
    """`?rewrite=false` must short-circuit and never call _call_litellm."""
    called = {"n": 0}

    async def explode(**kwargs: Any):
        called["n"] += 1
        raise AssertionError("LLM was called despite rewrite=false")

    monkeypatch.setattr(server_module, "_call_litellm", explode)

    resp = client.post(
        "/hints/ask?rewrite=false",
        json={"puzzle_id": real_puzzle_id, "level": 1},
    )
    assert resp.status_code == 200
    body = resp.json()
    assert called["n"] == 0
    assert body["source"] == "static"
    assert body["model_used"] == "none"
    assert body["hint_rewritten"] is None


def test_ask_rewrite_fallback_on_backend_error(client, real_puzzle_id, monkeypatch):
    """Generic backend RuntimeError (HTTP 500, malformed JSON…) → fallback."""
    fake = _make_fake_call(raise_exc=RuntimeError("litellm http 500: oops"))
    monkeypatch.setattr(server_module, "_call_litellm", fake)

    resp = client.post(
        "/hints/ask",
        json={"puzzle_id": real_puzzle_id, "level": 1},
    )
    assert resp.status_code == 200
    body = resp.json()
    assert body["source"] == "llm_fallback_static"
    assert body["hint"] == body["hint_static"]


# ---------------------------------------------------------------------------
# /hints/rewrite debug endpoint
# ---------------------------------------------------------------------------


def test_rewrite_endpoint_works(client, real_puzzle_id, monkeypatch):
    fake = _make_fake_call(
        content="Mon cher, ne sous-estimez jamais la mémoire d'une mélodie ancienne.",
        tokens_used=29,
    )
    monkeypatch.setattr(server_module, "_call_litellm", fake)

    resp = client.post(
        "/hints/rewrite",
        json={
            "puzzle_id": real_puzzle_id,
            "level": 1,
            "max_tokens": 64,
            "temperature": 0.5,
        },
    )
    assert resp.status_code == 200, resp.text
    body = resp.json()
    assert body["puzzle_id"] == real_puzzle_id
    assert body["level"] == 1
    assert body["model_used"] == client.app.state.llm_model
    assert body["hint_rewritten"].startswith("Mon cher")
    assert body["tokens_used"] == 29
    assert isinstance(body["hint_static"], str) and len(body["hint_static"]) > 0


def test_rewrite_endpoint_502_on_safety_block(client, monkeypatch):
    fake = _make_fake_call(content="La fréquence est 1337 hertz, sans hésitation.")
    monkeypatch.setattr(server_module, "_call_litellm", fake)

    resp = client.post(
        "/hints/rewrite",
        json={"puzzle_id": "P4_RADIO", "level": 1},
    )
    assert resp.status_code == 502
    assert "safety" in resp.json()["detail"].lower()
