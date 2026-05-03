"""voice-bridge — FastAPI scaffold (P1 part4 bonus, NOT a daemon yet).

Spec: docs/superpowers/specs/2026-05-03-tts-stt-llm-macstudio-design.md §2.5

Routes:
  POST /voice/transcribe → proxies whisper.cpp on :8300
  POST /voice/intent     → forwards to LiteLLM npc-fast
  POST /tts              → F5-TTS local primary, fallback Piper Tower:8001 on
                           timeout (3 s) or error; logs `tts_backend_used`

This file is a SCAFFOLD: routes return HTTP 501 when the underlying backend is
not yet provisioned (whisper.cpp part2 / F5-TTS part3). It is NOT to be
launched as a daemon during P1 part4 — see part5 / D_part2 for activation.
"""
from __future__ import annotations

import json
import logging
import os
import time
import uuid
from typing import Any

import httpx
from fastapi import FastAPI, HTTPException, Request, Response
from fastapi.responses import JSONResponse

# ── config (env-overridable) ─────────────────────────────────────────────────
WHISPER_URL = os.getenv("WHISPER_URL", "http://localhost:8300")
LITELLM_URL = os.getenv("LITELLM_URL", "http://localhost:4000")
LITELLM_KEY = os.getenv("LITELLM_KEY", "sk-zacus-local-dev-do-not-share")
F5_URL = os.getenv("F5_URL", "http://localhost:8400")  # F5-TTS daemon (part3)
PIPER_URL = os.getenv("PIPER_URL", "http://192.168.0.120:8001")
F5_TIMEOUT_S = float(os.getenv("F5_TIMEOUT_S", "3.0"))

logging.basicConfig(
    level=logging.INFO,
    format='{"ts":"%(asctime)s","lvl":"%(levelname)s","msg":%(message)s}',
)
log = logging.getLogger("voice-bridge")

app = FastAPI(title="voice-bridge", version="0.1.0-scaffold")


def _jlog(event: str, **fields: Any) -> None:
    """Emit a structured JSON log line."""
    payload = {"event": event, **fields}
    log.info(json.dumps(payload, ensure_ascii=False))


@app.get("/health")
async def health() -> dict[str, Any]:
    """Readiness probe — voice-bridge process is alive (does not check backends)."""
    return {
        "status": "scaffold",
        "version": app.version,
        "backends": {
            "whisper": WHISPER_URL,
            "f5": F5_URL,
            "piper_fallback": PIPER_URL,
            "litellm": LITELLM_URL,
        },
    }


@app.post("/voice/transcribe")
async def transcribe(request: Request) -> Response:
    """Forward audio to whisper.cpp /inference. Returns 501 if backend down."""
    request_id = str(uuid.uuid4())
    body = await request.body()
    try:
        async with httpx.AsyncClient(timeout=30.0) as client:
            resp = await client.post(
                f"{WHISPER_URL}/inference",
                content=body,
                headers={"Content-Type": request.headers.get("Content-Type", "audio/wav")},
            )
        _jlog("transcribe", request_id=request_id, status=resp.status_code, bytes=len(body))
        return Response(content=resp.content, status_code=resp.status_code,
                        media_type=resp.headers.get("Content-Type", "application/json"))
    except httpx.ConnectError:
        _jlog("transcribe_backend_down", request_id=request_id, url=WHISPER_URL)
        raise HTTPException(status_code=501, detail="whisper.cpp not provisioned (part2 pending)")


@app.post("/voice/intent")
async def intent(payload: dict[str, Any]) -> JSONResponse:
    """Forward natural-language utterance to LiteLLM npc-fast.

    Expected payload: {"text": "...", "session_id": "..."}
    """
    request_id = str(uuid.uuid4())
    text = payload.get("text", "").strip()
    if not text:
        raise HTTPException(status_code=400, detail="payload.text is required")
    try:
        async with httpx.AsyncClient(timeout=30.0) as client:
            resp = await client.post(
                f"{LITELLM_URL}/v1/chat/completions",
                headers={"Authorization": f"Bearer {LITELLM_KEY}"},
                json={
                    "model": "npc-fast",
                    "messages": [{"role": "user", "content": text}],
                },
            )
        _jlog("intent", request_id=request_id, status=resp.status_code, text_len=len(text))
        return JSONResponse(content=resp.json(), status_code=resp.status_code)
    except httpx.ConnectError as exc:
        _jlog("intent_backend_down", request_id=request_id, url=LITELLM_URL, err=str(exc))
        raise HTTPException(status_code=502, detail=f"LiteLLM unreachable: {exc}")


@app.post("/tts")
async def tts(payload: dict[str, Any]) -> Response:
    """Synthesize speech.

    Default route: F5-TTS local. Fallback to Piper Tower:8001 on timeout
    (>3 s to first byte) or hard error. Always emits `tts_backend_used` log.
    """
    request_id = str(uuid.uuid4())
    phrase = payload.get("input") or payload.get("text", "")
    if not phrase:
        raise HTTPException(status_code=400, detail="payload.input is required")
    phrase_len = len(phrase)
    backend_used = "unknown"
    started = time.monotonic()

    # ── try F5 primary ─────────────────────────────────────────────────────
    try:
        async with httpx.AsyncClient(timeout=F5_TIMEOUT_S) as client:
            resp = await client.post(f"{F5_URL}/tts", json=payload)
        if resp.status_code == 200:
            backend_used = "f5"
            latency_ms = int((time.monotonic() - started) * 1000)
            _jlog("tts", request_id=request_id, tts_backend_used=backend_used,
                  latency_ms=latency_ms, phrase_len=phrase_len)
            return Response(content=resp.content, status_code=200,
                            media_type=resp.headers.get("Content-Type", "audio/wav"))
        # F5 returned non-200 → fallback
        _jlog("tts_f5_nonok", request_id=request_id, status=resp.status_code)
    except (httpx.ConnectError, httpx.TimeoutException) as exc:
        _jlog("tts_f5_down", request_id=request_id, err=type(exc).__name__)

    # ── fallback: Piper Tower :8001 ─────────────────────────────────────────
    try:
        async with httpx.AsyncClient(timeout=10.0) as client:
            resp = await client.post(f"{PIPER_URL}/tts", json=payload)
        backend_used = "piper_fallback"
        latency_ms = int((time.monotonic() - started) * 1000)
        _jlog("tts", request_id=request_id, tts_backend_used=backend_used,
              latency_ms=latency_ms, phrase_len=phrase_len, status=resp.status_code)
        if resp.status_code != 200:
            raise HTTPException(status_code=resp.status_code, detail="piper fallback failed")
        return Response(content=resp.content, status_code=200,
                        media_type=resp.headers.get("Content-Type", "audio/wav"))
    except httpx.ConnectError:
        _jlog("tts_all_backends_down", request_id=request_id)
        raise HTTPException(status_code=501,
                            detail="F5-TTS (part3) and Piper fallback both unreachable")


if __name__ == "__main__":  # pragma: no cover
    # Local dev only. Production launch handled later (part5 / D_part2).
    import uvicorn

    uvicorn.run(app, host="0.0.0.0", port=8200)
