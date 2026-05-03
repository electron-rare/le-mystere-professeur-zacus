"""Tests for tools.hints.server (P1 static lookup + P2 LLM rewrite + P3 anti-cheat).

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
    MutableClock,
    create_app,
    load_phrase_bank,
    safety_check,
)


# ---------------------------------------------------------------------------
# Shared fixtures
# ---------------------------------------------------------------------------


@pytest.fixture
def clock() -> MutableClock:
    """Per-test deterministic clock starting at t=1_000_000 ms."""
    return MutableClock(start_ms=1_000_000)


@pytest.fixture
def app(clock):
    """Per-test app instance — guarantees fresh anti-cheat state + clock."""
    return create_app(DEFAULT_PHRASES_PATH, DEFAULT_SAFETY_PATH, clock=clock)


@pytest.fixture
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
        json={"puzzle_id": "P999_DOES_NOT_EXIST", "level": 1, "session_id": "s-404"},
    )
    assert resp.status_code == 404
    assert "unknown puzzle_id" in resp.json()["detail"]


def test_hints_ask_invalid_level_returns_422(client, real_puzzle_id):
    resp = client.post(
        "/hints/ask?rewrite=false",
        json={"puzzle_id": real_puzzle_id, "level": 7, "session_id": "s-422"},
    )
    assert resp.status_code == 422


def test_hints_ask_preserves_french_diacritics(client, real_puzzle_id):
    """UTF-8 round-trip must keep accented characters intact (verbatim path).

    Uses a unique session per iteration so the cap-3 anti-cheat doesn't trip.
    """
    bank = load_phrase_bank(DEFAULT_PHRASES_PATH)
    has_accent = any(
        any(ch in phrase for ch in "éèêàùôîçÉÈÊÀÙÔÎÇ")
        for level in bank[real_puzzle_id].values()
        for phrase in level
    )
    if not has_accent:
        pytest.skip(f"{real_puzzle_id} has no accented phrases — skipping diacritic check")

    found_accent = False
    for i in range(20):
        resp = client.post(
            "/hints/ask?rewrite=false",
            json={"puzzle_id": real_puzzle_id, "level": 1, "session_id": f"s-diacritic-{i}"},
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
        json={"puzzle_id": real_puzzle_id, "level": 1, "session_id": "s-timeout"},
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
        json={"puzzle_id": "P4_RADIO", "level": 1, "session_id": "s-safety"},
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
        json={"puzzle_id": real_puzzle_id, "level": 1, "session_id": "s-bypass"},
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
        json={"puzzle_id": real_puzzle_id, "level": 1, "session_id": "s-backend"},
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


# ---------------------------------------------------------------------------
# P3 — anti-cheat: counters, cap, cooldown, penalties, auto-bump
# ---------------------------------------------------------------------------


def _ask(client, *, puzzle_id, level, session_id, rewrite=False):
    return client.post(
        f"/hints/ask?rewrite={'true' if rewrite else 'false'}",
        json={"puzzle_id": puzzle_id, "level": level, "session_id": session_id},
    )


def test_ask_increments_count(client, real_puzzle_id, clock):
    r1 = _ask(client, puzzle_id=real_puzzle_id, level=1, session_id="sess-A")
    assert r1.status_code == 200
    body1 = r1.json()
    assert body1["refused"] is False
    assert body1["count"] == 1
    assert body1["level_served"] == 1
    assert body1["score_penalty"] == 50
    assert body1["total_penalty"] == 50

    # advance past the cooldown so the next call is allowed
    clock.advance(61_000)

    r2 = _ask(client, puzzle_id=real_puzzle_id, level=2, session_id="sess-A")
    body2 = r2.json()
    assert body2["refused"] is False
    assert body2["count"] == 2
    assert body2["level_served"] == 2
    assert body2["score_penalty"] == 100
    assert body2["total_penalty"] == 150


def test_ask_third_request_serves_level_3(client, real_puzzle_id, clock):
    for i in range(3):
        if i > 0:
            clock.advance(61_000)
        r = _ask(client, puzzle_id=real_puzzle_id, level=i + 1, session_id="sess-3rd")
        assert r.status_code == 200, r.text
        body = r.json()
        assert body["refused"] is False
        assert body["count"] == i + 1
        assert body["level_served"] == i + 1
    assert body["level_served"] == 3
    assert body["total_penalty"] == 50 + 100 + 200


def test_ask_fourth_request_refused_rate_limit(client, real_puzzle_id, clock):
    # Burn the 3 hints quickly (advancing past cooldown each time).
    for i in range(3):
        if i > 0:
            clock.advance(61_000)
        r = _ask(client, puzzle_id=real_puzzle_id, level=1, session_id="sess-cap")
        assert r.json()["refused"] is False

    clock.advance(61_000)
    r4 = _ask(client, puzzle_id=real_puzzle_id, level=1, session_id="sess-cap")
    assert r4.status_code == 200
    body = r4.json()
    assert body["refused"] is True
    assert body["reason"] == "rate_limit"
    assert "max 3 hints" in (body.get("details") or "")
    assert body["count"] == 3
    assert body["source"] == "refused"
    # No phrase served on refusal
    assert body["hint"] is None


def test_ask_within_cooldown_refused(client, real_puzzle_id, clock):
    r1 = _ask(client, puzzle_id=real_puzzle_id, level=1, session_id="sess-cd")
    assert r1.json()["refused"] is False
    cooldown_end_1 = r1.json()["cooldown_until_ms"]

    # Only 10s later — still inside the 60s cooldown window.
    clock.advance(10_000)
    r2 = _ask(client, puzzle_id=real_puzzle_id, level=2, session_id="sess-cd")
    assert r2.status_code == 200
    body = r2.json()
    assert body["refused"] is True
    assert body["reason"] == "cooldown"
    assert body["count"] == 1, "refused request must NOT bump count"
    assert body["retry_in_s"] >= 49 and body["retry_in_s"] <= 51
    assert body["cooldown_until_ms"] == cooldown_end_1


def test_ask_after_cooldown_succeeds(client, real_puzzle_id, clock):
    r1 = _ask(client, puzzle_id=real_puzzle_id, level=1, session_id="sess-cdok")
    assert r1.json()["refused"] is False

    clock.advance(60_001)  # 60s + 1ms : cooldown elapsed
    r2 = _ask(client, puzzle_id=real_puzzle_id, level=2, session_id="sess-cdok")
    body = r2.json()
    assert body["refused"] is False
    assert body["count"] == 2
    assert body["level_served"] == 2


def test_ask_auto_bumps_level_on_repeat_lower(client, real_puzzle_id, clock):
    """Player asks level 1, then level 1 again — server forces level 2."""
    r1 = _ask(client, puzzle_id=real_puzzle_id, level=1, session_id="sess-bump")
    assert r1.json()["level_served"] == 1

    clock.advance(61_000)
    r2 = _ask(client, puzzle_id=real_puzzle_id, level=1, session_id="sess-bump")
    body = r2.json()
    assert body["refused"] is False
    assert body["level_requested"] == 1
    assert body["level_served"] == 2, "auto-bump must force level=count+1"
    # The hint string itself comes from level_2 phrases (best-effort check)
    bank = load_phrase_bank(DEFAULT_PHRASES_PATH)
    if bank[real_puzzle_id].get("level_2"):
        assert body["hint_static"] in bank[real_puzzle_id]["level_2"]


def test_ask_missing_session_id_400(client, real_puzzle_id):
    resp = client.post(
        "/hints/ask?rewrite=false",
        json={"puzzle_id": real_puzzle_id, "level": 1},
    )
    assert resp.status_code == 400
    assert "session_id" in resp.json()["detail"]


def test_pipeline_count_does_not_increment_on_safety_block(
    client, monkeypatch, clock,
):
    """A safety-tripped LLM rewrite still serves the static fallback phrase,
    so the counter MUST advance — the player did receive a hint.

    This documents the contract: 'serve' includes the verbatim fallback path.
    """
    fake = _make_fake_call(
        content="Tournez jusqu'à 1337 hertz exactement.",  # leaks banned word
    )
    monkeypatch.setattr(server_module, "_call_litellm", fake)

    r = client.post(
        "/hints/ask",
        json={"puzzle_id": "P4_RADIO", "level": 1, "session_id": "sess-safety"},
    )
    assert r.status_code == 200
    body = r.json()
    assert body["refused"] is False
    assert body["source"] == "llm_fallback_static"
    # Player got a hint (the verbatim static one), so counters DID move.
    assert body["count"] == 1
    assert body["score_penalty"] == 50
    assert body["total_penalty"] == 50


def test_pipeline_count_does_not_increment_on_unknown_puzzle(client):
    """Unresolvable puzzle (404) must NOT consume a hint slot.

    Belt-and-braces: even if the firmware sends a typo'd puzzle_id, the
    player's per-puzzle budget for valid puzzles is preserved.
    """
    resp = client.post(
        "/hints/ask?rewrite=false",
        json={"puzzle_id": "P999_WHATEVER", "level": 1, "session_id": "sess-clean"},
    )
    assert resp.status_code == 404
    # Now check sessions admin view: nothing should be tracked under sess-clean.
    sessions = client.get("/hints/sessions").json()["sessions"]
    assert all(s["session_id"] != "sess-clean" for s in sessions)


# ---------------------------------------------------------------------------
# P3 — admin endpoints
# ---------------------------------------------------------------------------


def test_admin_sessions_lists_state(client, real_puzzle_id, clock):
    _ask(client, puzzle_id=real_puzzle_id, level=1, session_id="sess-admin1")
    clock.advance(61_000)
    _ask(client, puzzle_id=real_puzzle_id, level=2, session_id="sess-admin1")

    resp = client.get("/hints/sessions")
    assert resp.status_code == 200
    body = resp.json()
    assert body["total_sessions"] >= 1
    assert "config" in body
    assert body["config"]["max_per_puzzle"] == 3
    assert body["config"]["cooldown_s"] == 60
    assert body["config"]["penalty_per_level"] == {"1": 50, "2": 100, "3": 200}

    target = next(s for s in body["sessions"] if s["session_id"] == "sess-admin1")
    assert target["total_hints"] == 2
    assert target["total_penalty"] == 150
    assert len(target["puzzles"]) == 1
    assert target["puzzles"][0]["puzzle_id"] == real_puzzle_id
    assert target["puzzles"][0]["count"] == 2


def test_admin_delete_resets_session(client, real_puzzle_id):
    _ask(client, puzzle_id=real_puzzle_id, level=1, session_id="sess-del")

    # State is present before delete
    pre = client.get("/hints/sessions").json()
    assert any(s["session_id"] == "sess-del" for s in pre["sessions"])

    resp = client.delete("/hints/sessions/sess-del")
    assert resp.status_code == 200
    body = resp.json()
    assert body["ok"] is True
    assert body["entries_removed"] == 1

    post = client.get("/hints/sessions").json()
    assert all(s["session_id"] != "sess-del" for s in post["sessions"])

    # And after a reset, the player gets a fresh budget.
    r = _ask(client, puzzle_id=real_puzzle_id, level=1, session_id="sess-del")
    assert r.json()["refused"] is False
    assert r.json()["count"] == 1


def test_admin_endpoints_require_key_when_set(monkeypatch):
    monkeypatch.setenv("HINTS_ADMIN_KEY", "secret-zacus")
    app = create_app(DEFAULT_PHRASES_PATH, DEFAULT_SAFETY_PATH, clock=MutableClock())
    c = TestClient(app)

    # No header → 401
    assert c.get("/hints/sessions").status_code == 401
    assert c.delete("/hints/sessions/whatever").status_code == 401

    # Wrong key → 401
    assert c.get("/hints/sessions", headers={"X-Admin-Key": "nope"}).status_code == 401

    # Correct key → 200
    ok = c.get("/hints/sessions", headers={"X-Admin-Key": "secret-zacus"})
    assert ok.status_code == 200
    assert "sessions" in ok.json()
