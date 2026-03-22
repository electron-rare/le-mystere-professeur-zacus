#!/usr/bin/env python3
"""
Voice Bridge — ESP32 <-> LLM <-> TTS
Standalone FastAPI server bridging Zacus ESP32 voice pipeline to LLM + TTS.

Can run independently or be integrated into mascarade as a router.

Usage:
    python3 mascarade_voice_bridge.py [--port 8200] [--tts-url http://VM:8000]

Protocol (XiaoZhi-inspired):
    1. ESP32 connects via WebSocket /voice/ws
    2. JSON "hello" handshake
    3. Device sends text_query or binary OPUS audio
    4. Server: ASR -> LLM -> TTS -> stream audio back

Requires: fastapi, uvicorn, httpx, websockets
Install: pip install fastapi uvicorn httpx websockets
"""

import argparse
import asyncio
import json
import logging
import os
import re
import sys
import time
from typing import Optional

try:
    import httpx
    import uvicorn
    from fastapi import FastAPI, WebSocket, WebSocketDisconnect
    from starlette.websockets import WebSocketState
except ImportError:
    print("Missing dependencies. Install with:")
    print("  pip install fastapi uvicorn httpx websockets")
    sys.exit(1)

# ---------------------------------------------------------------------------
# Config
# ---------------------------------------------------------------------------

TTS_URL = os.environ.get("ZACUS_TTS_URL", "http://192.168.0.120:8001/v1/audio/speech")
TTS_VOICE = os.environ.get("ZACUS_TTS_VOICE", "alloy")
LLM_URL = os.environ.get("ZACUS_LLM_URL", "http://localhost:8100/v1/chat/completions")
LLM_MODEL = os.environ.get("ZACUS_LLM_MODEL", "default")
AUTH_TOKEN = os.environ.get("ZACUS_VOICE_TOKEN", "")
LOG_LEVEL = os.environ.get("ZACUS_VOICE_LOG", "INFO")

PROFESSOR_ZACUS_PROMPT = (
    "Tu es le Professeur Zacus, un scientifique excentrique et bienveillant. "
    "Tu parles en francais. Tu donnes des indices cryptiques pour aider les "
    "joueurs a resoudre les enigmes, sans jamais reveler les solutions directement. "
    "Tes reponses sont courtes (1-2 phrases), mysterieuses et encourageantes. "
    "Tu fais parfois reference a tes experiences de laboratoire."
)

# ---------------------------------------------------------------------------
# Logging
# ---------------------------------------------------------------------------

logging.basicConfig(
    level=getattr(logging, LOG_LEVEL.upper(), logging.INFO),
    format="%(asctime)s [%(levelname)s] %(name)s: %(message)s",
    datefmt="%H:%M:%S",
)
logger = logging.getLogger("voice_bridge")

# ---------------------------------------------------------------------------
# App
# ---------------------------------------------------------------------------

app = FastAPI(title="Zacus Voice Bridge", version="1.0.0")


@app.get("/health")
async def health():
    """Health check."""
    return {"status": "ok", "service": "voice_bridge", "tts_url": TTS_URL}


@app.websocket("/voice/ws")
async def voice_websocket(websocket: WebSocket, token: str = ""):
    """WebSocket endpoint for ESP32 voice pipeline."""
    # Auth check
    if AUTH_TOKEN and token != AUTH_TOKEN:
        await websocket.close(code=4003, reason="unauthorized")
        return

    await websocket.accept()
    client = f"{websocket.client.host}:{websocket.client.port}" if websocket.client else "unknown"
    logger.info("Client connected: %s", client)

    try:
        # --- Handshake ---
        raw = await asyncio.wait_for(websocket.receive_json(), timeout=5.0)
        if raw.get("type") != "hello":
            await websocket.close(code=4001, reason="expected hello")
            return

        device_id = raw.get("device_id", "unknown")
        logger.info("Handshake from device: %s", device_id)

        await websocket.send_json({
            "type": "hello_ack",
            "version": 1,
            "capabilities": ["tts", "llm", "text_query"],
        })

        # --- Main loop ---
        while True:
            data = await websocket.receive()

            if "text" in data:
                msg = json.loads(data["text"])
                await _handle_message(websocket, msg, device_id)
            elif "bytes" in data:
                await _handle_audio(websocket, data["bytes"], device_id)

    except WebSocketDisconnect:
        logger.info("Client disconnected: %s", client)
    except asyncio.TimeoutError:
        logger.warning("Handshake timeout: %s", client)
        if websocket.client_state == WebSocketState.CONNECTED:
            await websocket.close(code=4002, reason="timeout")
    except Exception as exc:
        logger.error("Error with %s: %s", client, exc)
        if websocket.client_state == WebSocketState.CONNECTED:
            await websocket.close(code=4000, reason="internal_error")


# ---------------------------------------------------------------------------
# Message handlers
# ---------------------------------------------------------------------------

async def _handle_message(ws: WebSocket, msg: dict, device_id: str):
    """Handle JSON control message."""
    msg_type = msg.get("type", "")

    if msg_type == "listen":
        state = msg.get("state", "")
        if state == "detect":
            logger.info("[%s] Wake word detected: %s", device_id, msg.get("text", ""))
            await ws.send_json({"type": "listen_ack", "state": "ready"})
        elif state == "stop":
            logger.info("[%s] Stopped listening", device_id)

    elif msg_type == "text_query":
        query = msg.get("text", "").strip()
        if not query:
            await ws.send_json({"type": "error", "message": "empty query"})
            return

        logger.info("[%s] Query: %s", device_id, query[:80])
        t0 = time.monotonic()

        # Detect hint routing: [HINT:puzzle_id:level] prefix
        hint_match = re.match(r'^\[HINT:(\w+):(\d)\]\s*(.*)', query)

        if hint_match:
            puzzle_id = hint_match.group(1)
            hint_level = int(hint_match.group(2))
            question = hint_match.group(3) or "Give me a hint"
            response = await _query_hints(puzzle_id, question, hint_level, device_id)
        else:
            response = await _query_llm(query)
        t_llm = time.monotonic() - t0

        # TTS
        await ws.send_json({"type": "tts", "state": "start", "text": response})
        audio = await _text_to_speech(response)
        t_total = time.monotonic() - t0

        if audio:
            await ws.send_bytes(audio)
            logger.info("[%s] Response sent: LLM=%.1fs, total=%.1fs, audio=%dKB",
                        device_id, t_llm, t_total, len(audio) // 1024)
        else:
            logger.warning("[%s] TTS failed, text-only response", device_id)

        await ws.send_json({"type": "tts", "state": "stop"})

    elif msg_type == "abort":
        logger.info("[%s] Playback aborted", device_id)

    else:
        logger.debug("[%s] Unknown message type: %s", device_id, msg_type)


async def _handle_audio(ws: WebSocket, frame: bytes, device_id: str):
    """Handle binary audio frame.

    MVP: Log frame size. Full implementation will:
    1. Decode OPUS
    2. Buffer until VAD end-of-speech
    3. Run ASR (faster-whisper)
    4. Query LLM
    5. TTS + stream back
    """
    logger.debug("[%s] Audio frame: %d bytes", device_id, len(frame))
    # TODO: OPUS decode + ASR pipeline


# ---------------------------------------------------------------------------
# LLM + TTS clients
# ---------------------------------------------------------------------------

async def _query_hints(puzzle_id: str, question: str, hint_level: int, session_id: str = "default") -> str:
    """Route to mascarade hints engine."""
    try:
        async with httpx.AsyncClient(timeout=10.0) as client:
            resp = await client.post(
                "http://localhost:8100/hints/ask",
                json={
                    "puzzle_id": puzzle_id,
                    "question": question,
                    "hint_level": hint_level,
                    "session_id": session_id,
                },
            )
            if resp.status_code == 200:
                data = resp.json()
                return data.get("hint", "Hmm...")
            logger.error("Hints HTTP %d: %s", resp.status_code, resp.text[:200])
    except Exception as exc:
        logger.error("Hints error: %s", exc)
    # Fallback to generic LLM
    return await _query_llm(question)


async def _query_llm(text: str) -> str:
    """Query LLM via OpenAI-compatible API."""
    try:
        async with httpx.AsyncClient(timeout=10.0) as client:
            resp = await client.post(
                LLM_URL,
                json={
                    "model": LLM_MODEL,
                    "messages": [
                        {"role": "system", "content": PROFESSOR_ZACUS_PROMPT},
                        {"role": "user", "content": text},
                    ],
                    "max_tokens": 150,
                    "temperature": 0.8,
                },
            )
            if resp.status_code == 200:
                data = resp.json()
                choices = data.get("choices", [])
                if choices:
                    return choices[0].get("message", {}).get("content", "Hmm...")
            logger.error("LLM HTTP %d: %s", resp.status_code, resp.text[:200])
    except Exception as exc:
        logger.error("LLM error: %s", exc)

    return "Mon laboratoire semble avoir un probleme technique... Reessaie."


async def _text_to_speech(text: str) -> Optional[bytes]:
    """Convert text to speech via Piper/OpenedAI-Speech API."""
    try:
        async with httpx.AsyncClient(timeout=15.0) as client:
            resp = await client.post(
                TTS_URL,
                json={
                    "model": "tts-1",
                    "input": text,
                    "voice": TTS_VOICE,
                    "response_format": "wav",
                },
            )
            if resp.status_code == 200:
                return resp.content
            logger.error("TTS HTTP %d: %s", resp.status_code, resp.text[:200])
    except Exception as exc:
        logger.error("TTS error: %s", exc)

    return None


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(description="Zacus Voice Bridge")
    parser.add_argument("--port", type=int, default=8200, help="Listen port")
    parser.add_argument("--host", default="0.0.0.0", help="Bind address")
    parser.add_argument("--tts-url", default=None, help="TTS API URL")
    parser.add_argument("--llm-url", default=None, help="LLM API URL")
    args = parser.parse_args()

    global TTS_URL, LLM_URL
    if args.tts_url:
        TTS_URL = args.tts_url
    if args.llm_url:
        LLM_URL = args.llm_url

    logger.info("Starting Voice Bridge on %s:%d", args.host, args.port)
    logger.info("  TTS: %s", TTS_URL)
    logger.info("  LLM: %s", LLM_URL)

    uvicorn.run(app, host=args.host, port=args.port, log_level="info")


if __name__ == "__main__":
    main()
