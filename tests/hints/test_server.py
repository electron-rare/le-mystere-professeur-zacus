"""Tests for tools.hints.server (P1 — static phrase lookup).

Run via:
    make hints-test
or:
    uv run --with fastapi --with uvicorn --with pyyaml --with pydantic \
           --with pytest --with httpx pytest tests/hints/ -v
"""
from __future__ import annotations

import sys
from pathlib import Path

import pytest
from fastapi.testclient import TestClient

REPO_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO_ROOT))

from tools.hints.server import (  # noqa: E402  (import after sys.path tweak)
    DEFAULT_PHRASES_PATH,
    create_app,
    load_phrase_bank,
)


# ---------------------------------------------------------------------------
# Shared fixtures
# ---------------------------------------------------------------------------


@pytest.fixture(scope="module")
def app():
    return create_app(DEFAULT_PHRASES_PATH)


@pytest.fixture(scope="module")
def client(app):
    return TestClient(app)


@pytest.fixture(scope="module")
def real_puzzle_id():
    """First puzzle in the bank that has a non-empty level_1.

    Using a real puzzle from the actual YAML keeps the test honest — if
    `npc_phrases.yaml` drifts, the suite catches it.
    """
    bank = load_phrase_bank(DEFAULT_PHRASES_PATH)
    for pid, levels in bank.items():
        if levels.get("level_1"):
            return pid
    pytest.fail("no puzzle with level_1 found in npc_phrases.yaml")


# ---------------------------------------------------------------------------
# Health + coverage
# ---------------------------------------------------------------------------


def test_healthz_returns_ok_with_counts(client):
    resp = client.get("/healthz")
    assert resp.status_code == 200
    body = resp.json()
    assert body["status"] == "ok"
    assert body["phrases_loaded"] > 0
    assert body["puzzles_loaded"] > 0
    assert body["phrases_path"].endswith("npc_phrases.yaml")


def test_coverage_summary_shape_is_complete(client):
    resp = client.get("/hints/coverage")
    assert resp.status_code == 200
    body = resp.json()

    # Required top-level fields
    for key in ("total_puzzles", "puzzles_by_level_count",
                "percent_by_level_count", "per_puzzle"):
        assert key in body, f"coverage missing field {key!r}"

    # Bucket counts must sum to total
    bucket_sum = sum(body["puzzles_by_level_count"].values())
    assert bucket_sum == body["total_puzzles"]
    assert body["total_puzzles"] >= 1

    # Each per_puzzle row has the expected sub-fields
    for row in body["per_puzzle"]:
        assert "puzzle_id" in row
        assert "levels_present" in row
        assert "levels_missing" in row
        assert "phrase_counts" in row
        assert set(row["phrase_counts"].keys()) == {"level_1", "level_2", "level_3"}


# ---------------------------------------------------------------------------
# /hints/ask happy path + errors
# ---------------------------------------------------------------------------


def test_hints_ask_happy_path_returns_real_phrase(client, real_puzzle_id):
    resp = client.post(
        "/hints/ask",
        json={"puzzle_id": real_puzzle_id, "level": 1, "session_id": "test-session"},
    )
    assert resp.status_code == 200, resp.text
    body = resp.json()
    assert body["puzzle_id"] == real_puzzle_id
    assert body["level"] == 1
    assert body["source"] == "static"
    assert isinstance(body["hint"], str)
    assert len(body["hint"]) > 0

    # The phrase must be one of the real phrases in the bank — guards
    # against accidental string mangling in the loader.
    bank = load_phrase_bank(DEFAULT_PHRASES_PATH)
    assert body["hint"] in bank[real_puzzle_id]["level_1"]


def test_hints_ask_unknown_puzzle_returns_404(client):
    resp = client.post(
        "/hints/ask",
        json={"puzzle_id": "P999_DOES_NOT_EXIST", "level": 1},
    )
    assert resp.status_code == 404
    assert "unknown puzzle_id" in resp.json()["detail"]


def test_hints_ask_invalid_level_returns_422(client, real_puzzle_id):
    # Pydantic validation rejects out-of-range level → 422 (Unprocessable)
    resp = client.post(
        "/hints/ask",
        json={"puzzle_id": real_puzzle_id, "level": 7},
    )
    assert resp.status_code == 422


def test_hints_ask_preserves_french_diacritics(client, real_puzzle_id):
    """UTF-8 round-trip must keep accented characters intact."""
    bank = load_phrase_bank(DEFAULT_PHRASES_PATH)
    has_accent = any(
        any(ch in phrase for ch in "éèêàùôîçÉÈÊÀÙÔÎÇ")
        for level in bank[real_puzzle_id].values()
        for phrase in level
    )
    if not has_accent:
        pytest.skip(f"{real_puzzle_id} has no accented phrases — skipping diacritic check")

    # Try several requests — at least one should land on an accented phrase
    found_accent = False
    for _ in range(20):
        resp = client.post(
            "/hints/ask", json={"puzzle_id": real_puzzle_id, "level": 1}
        )
        assert resp.status_code == 200
        if any(ch in resp.json()["hint"] for ch in "éèêàùôîçÉÈÊÀÙÔÎÇ"):
            found_accent = True
            break
    assert found_accent, "diacritics never round-tripped — encoding bug?"
