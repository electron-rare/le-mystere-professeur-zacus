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

    Listen protocol (OPUS audio path):
    - Client -> {"type":"listen","state":"detect","text":"Hi ESP"}  (wake word)
    - Server -> {"type":"listen_ack","state":"ready"}               (ready to receive)
    - Client -> binary OPUS frames (20ms each, 16kHz mono)
    - Client -> {"type":"listen","state":"stop"}                    (end of speech)
    - Server -> {"type":"stt","text":"transcribed text"}            (STT result)
    - Server -> {"type":"tts","state":"start","text":"response"}
    - Server -> binary OPUS frames (TTS audio)
    - Server -> {"type":"tts","state":"stop"}

Requires: fastapi, uvicorn, httpx, websockets, opuslib, webrtcvad
Install: pip install fastapi uvicorn httpx websockets opuslib webrtcvad
"""

import argparse
import asyncio
import io
import json
import logging
import os
import re
import struct
import sys
import time
import wave
from typing import Optional

try:
    import httpx
    import uvicorn
    from fastapi import FastAPI, WebSocket, WebSocketDisconnect
    from starlette.websockets import WebSocketState
except ImportError:
    print("Missing core dependencies. Install with:")
    print("  pip install fastapi uvicorn httpx websockets")
    sys.exit(1)

# OPUS codec (optional — needed for audio path)
try:
    import opuslib
    import opuslib.api.encoder
    import opuslib.api.decoder
    HAS_OPUS = True
except ImportError:
    HAS_OPUS = False

# VAD (optional — needed for audio path)
try:
    import webrtcvad
    HAS_VAD = True
except ImportError:
    HAS_VAD = False

# ---------------------------------------------------------------------------
# Config
# ---------------------------------------------------------------------------

TTS_URL = os.environ.get("ZACUS_TTS_URL", "http://192.168.0.120:8001/v1/audio/speech")
TTS_VOICE = os.environ.get("ZACUS_TTS_VOICE", "alloy")
LLM_URL = os.environ.get("ZACUS_LLM_URL", "http://localhost:8100/v1/chat/completions")
LLM_MODEL = os.environ.get("ZACUS_LLM_MODEL", "default")
STT_URL = os.environ.get("ZACUS_STT_URL", "http://192.168.0.120:8901/v1/audio/transcriptions")
AUTH_TOKEN = os.environ.get("ZACUS_VOICE_TOKEN", "")
LOG_LEVEL = os.environ.get("ZACUS_VOICE_LOG", "INFO")

# Audio constants
OPUS_SAMPLE_RATE = 16000       # 16kHz mono
OPUS_CHANNELS = 1
OPUS_FRAME_MS = 20             # 20ms frames from ESP32
OPUS_FRAME_SAMPLES = OPUS_SAMPLE_RATE * OPUS_FRAME_MS // 1000  # 320 samples
OPUS_APPLICATION = "voip"      # opuslib application type
TTS_OPUS_BITRATE = 24000       # 24kbps for TTS back to ESP32
VAD_AGGRESSIVENESS = 2         # webrtcvad 0-3 (2 = balanced)
VAD_SILENCE_FRAMES = 30        # ~600ms of silence to trigger end-of-speech
MAX_HISTORY_TURNS = int(os.environ.get("ZACUS_MAX_HISTORY", "10"))  # conversation turns per session

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
# Audio Session Manager
# ---------------------------------------------------------------------------

class AudioSessionManager:
    """Manages per-device audio sessions for OPUS decode, VAD, and STT.

    Lifecycle:
        1. start() — called on listen/detect, creates decoder, resets buffers
        2. feed_opus(frame) — decode OPUS frame, run VAD, buffer PCM
        3. stop() -> str — finalize, send accumulated PCM to Whisper STT
    """

    def __init__(self, device_id: str):
        self.device_id = device_id
        self._pcm_buffer = bytearray()
        self._active = False
        self._decoder = None
        self._vad = None
        self._silence_count = 0
        self._speech_detected = False
        self.conversation_history: list[dict[str, str]] = []

    def start(self):
        """Start a new recording session."""
        self._pcm_buffer = bytearray()
        self._active = True
        self._silence_count = 0
        self._speech_detected = False

        if HAS_OPUS:
            self._decoder = opuslib.Decoder(OPUS_SAMPLE_RATE, OPUS_CHANNELS)
        else:
            self._decoder = None
            logger.warning("[%s] opuslib not available — raw PCM passthrough", self.device_id)

        if HAS_VAD:
            self._vad = webrtcvad.Vad(VAD_AGGRESSIVENESS)
        else:
            self._vad = None

        logger.info("[%s] Audio session started (opus=%s, vad=%s)",
                    self.device_id, HAS_OPUS, HAS_VAD)

    def feed_opus(self, frame: bytes) -> bool:
        """Decode one OPUS frame and buffer the PCM.

        Returns True if VAD detected end-of-speech (enough trailing silence).
        """
        if not self._active:
            return False

        # Decode OPUS -> 16-bit PCM
        if self._decoder and HAS_OPUS:
            try:
                pcm = self._decoder.decode(frame, OPUS_FRAME_SAMPLES)
            except Exception as exc:
                logger.debug("[%s] OPUS decode error: %s", self.device_id, exc)
                return False
        else:
            # Fallback: treat as raw 16-bit PCM
            pcm = frame

        self._pcm_buffer.extend(pcm)

        # VAD check
        if self._vad:
            try:
                # webrtcvad needs exactly 10/20/30ms of 16-bit PCM at supported rates
                is_speech = self._vad.is_speech(pcm, OPUS_SAMPLE_RATE)
            except Exception:
                is_speech = True  # assume speech on VAD error

            if is_speech:
                self._speech_detected = True
                self._silence_count = 0
            else:
                self._silence_count += 1
                if self._speech_detected and self._silence_count >= VAD_SILENCE_FRAMES:
                    logger.info("[%s] VAD: end-of-speech detected", self.device_id)
                    return True
        else:
            # No VAD: simple energy-based detection
            self._speech_detected = True
            if len(pcm) >= 2:
                energy = _rms_energy(pcm)
                if energy < 200:  # silence threshold
                    self._silence_count += 1
                    if self._silence_count >= VAD_SILENCE_FRAMES:
                        logger.info("[%s] Energy VAD: end-of-speech", self.device_id)
                        return True
                else:
                    self._silence_count = 0

        return False

    async def stop_and_transcribe(self) -> str:
        """Stop session and transcribe accumulated PCM via Whisper."""
        self._active = False
        pcm_bytes = bytes(self._pcm_buffer)

        if not pcm_bytes:
            logger.warning("[%s] Empty audio buffer, nothing to transcribe", self.device_id)
            return ""

        duration_s = len(pcm_bytes) / (OPUS_SAMPLE_RATE * 2)  # 16-bit = 2 bytes/sample
        logger.info("[%s] Transcribing %.1fs of audio (%d bytes PCM)",
                    self.device_id, duration_s, len(pcm_bytes))

        # Wrap PCM in WAV container for Whisper
        wav_bytes = _pcm_to_wav(pcm_bytes, OPUS_SAMPLE_RATE, OPUS_CHANNELS)

        # POST to Whisper STT endpoint (OpenAI-compatible)
        text = await _transcribe_audio(wav_bytes, self.device_id)
        return text

    @property
    def active(self) -> bool:
        return self._active


def _rms_energy(pcm: bytes) -> float:
    """Compute RMS energy of 16-bit PCM samples."""
    n_samples = len(pcm) // 2
    if n_samples == 0:
        return 0.0
    samples = struct.unpack(f"<{n_samples}h", pcm[:n_samples * 2])
    return (sum(s * s for s in samples) / n_samples) ** 0.5


def _pcm_to_wav(pcm: bytes, sample_rate: int, channels: int) -> bytes:
    """Wrap raw 16-bit PCM in a WAV container."""
    buf = io.BytesIO()
    with wave.open(buf, "wb") as wf:
        wf.setnchannels(channels)
        wf.setsampwidth(2)  # 16-bit
        wf.setframerate(sample_rate)
        wf.writeframes(pcm)
    return buf.getvalue()


def _wav_to_pcm(wav_data: bytes) -> tuple[bytes, int, int]:
    """Extract raw PCM, sample_rate, channels from WAV bytes."""
    buf = io.BytesIO(wav_data)
    with wave.open(buf, "rb") as wf:
        pcm = wf.readframes(wf.getnframes())
        return pcm, wf.getframerate(), wf.getnchannels()


def _encode_pcm_to_opus_frames(pcm: bytes, sample_rate: int, channels: int) -> list[bytes]:
    """Encode raw PCM to a list of OPUS frames suitable for streaming.

    Resamples to 16kHz mono if needed (simple nearest-neighbor).
    """
    if not HAS_OPUS:
        logger.warning("opuslib not available — cannot encode OPUS")
        return []

    # Resample to target rate if needed (simple decimation)
    if sample_rate != OPUS_SAMPLE_RATE or channels != OPUS_CHANNELS:
        pcm = _resample_pcm(pcm, sample_rate, channels, OPUS_SAMPLE_RATE, OPUS_CHANNELS)

    encoder = opuslib.Encoder(OPUS_SAMPLE_RATE, OPUS_CHANNELS, OPUS_APPLICATION)

    frame_size_bytes = OPUS_FRAME_SAMPLES * 2  # 16-bit mono
    frames = []
    offset = 0
    while offset + frame_size_bytes <= len(pcm):
        chunk = pcm[offset:offset + frame_size_bytes]
        try:
            opus_frame = encoder.encode(chunk, OPUS_FRAME_SAMPLES)
            frames.append(opus_frame)
        except Exception as exc:
            logger.debug("OPUS encode error at offset %d: %s", offset, exc)
        offset += frame_size_bytes

    logger.debug("Encoded %d OPUS frames from %d bytes PCM", len(frames), len(pcm))
    return frames


def _resample_pcm(pcm: bytes, src_rate: int, src_ch: int,
                   dst_rate: int, dst_ch: int) -> bytes:
    """Simple nearest-neighbor resample. Good enough for voice."""
    src_sample_size = 2 * src_ch  # 16-bit per channel
    n_src_samples = len(pcm) // src_sample_size
    ratio = dst_rate / src_rate
    n_dst_samples = int(n_src_samples * ratio)

    result = bytearray()
    for i in range(n_dst_samples):
        src_idx = int(i / ratio)
        if src_idx >= n_src_samples:
            src_idx = n_src_samples - 1
        offset = src_idx * src_sample_size
        if dst_ch == 1 and src_ch == 2:
            # Stereo to mono: average
            l = struct.unpack_from("<h", pcm, offset)[0]
            r = struct.unpack_from("<h", pcm, offset + 2)[0]
            result.extend(struct.pack("<h", (l + r) // 2))
        elif dst_ch == 1 and src_ch == 1:
            result.extend(pcm[offset:offset + 2])
        else:
            # Just take first channel
            result.extend(pcm[offset:offset + 2])

    return bytes(result)


# Per-device session registry
_audio_sessions: dict[str, AudioSessionManager] = {}


def _get_session(device_id: str) -> AudioSessionManager:
    """Get or create an AudioSessionManager for a device."""
    if device_id not in _audio_sessions:
        _audio_sessions[device_id] = AudioSessionManager(device_id)
    return _audio_sessions[device_id]


# ---------------------------------------------------------------------------
# App
# ---------------------------------------------------------------------------

app = FastAPI(title="Zacus Voice Bridge", version="1.0.0")


@app.get("/health")
async def health():
    """Health check."""
    return {
        "status": "ok",
        "service": "voice_bridge",
        "tts_url": TTS_URL,
        "stt_url": STT_URL,
        "opus": HAS_OPUS,
        "vad": HAS_VAD,
        "active_sessions": len(_audio_sessions),
    }


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
    device_id = None

    try:
        # --- Handshake ---
        raw = await asyncio.wait_for(websocket.receive_json(), timeout=5.0)
        if raw.get("type") != "hello":
            await websocket.close(code=4001, reason="expected hello")
            return

        device_id = raw.get("device_id", "unknown")
        logger.info("Handshake from device: %s", device_id)

        capabilities = ["tts", "llm", "text_query"]
        if HAS_OPUS:
            capabilities.append("opus_audio")
        if HAS_VAD:
            capabilities.append("vad")

        await websocket.send_json({
            "type": "hello_ack",
            "version": 2,
            "capabilities": capabilities,
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
        if device_id and device_id in _audio_sessions:
            del _audio_sessions[device_id]
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
            wake_text = msg.get("text", "")
            logger.info("[%s] Wake word detected: %s", device_id, wake_text)
            # Start audio session for this device
            session = _get_session(device_id)
            session.start()
            await ws.send_json({"type": "listen_ack", "state": "ready"})

        elif state == "stop":
            logger.info("[%s] Listen stop received — transcribing", device_id)
            session = _get_session(device_id)
            t0 = time.monotonic()
            transcription = await session.stop_and_transcribe()
            t_stt = time.monotonic() - t0

            if transcription:
                logger.info("[%s] STT (%.1fs): %s", device_id, t_stt, transcription[:80])
                await ws.send_json({"type": "stt", "text": transcription})

                # Feed transcription into the LLM pipeline
                # Detect hint routing: [HINT:puzzle_id:level] prefix
                hint_match = re.match(r'^\[HINT:(\w+):(\d)\]\s*(.*)', transcription)
                if hint_match:
                    puzzle_id = hint_match.group(1)
                    hint_level = int(hint_match.group(2))
                    question = hint_match.group(3) or "Give me a hint"
                    response = await _query_hints(puzzle_id, question, hint_level, device_id)
                else:
                    response = await _query_llm(transcription, device_id)

                # TTS response — encode to OPUS if available
                await _send_tts_response(ws, response, device_id)
            else:
                logger.warning("[%s] STT returned empty text", device_id)
                await ws.send_json({"type": "stt", "text": ""})

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
            response = await _query_llm(query, device_id)

        # TTS response (OPUS-encoded if available)
        await _send_tts_response(ws, response, device_id)

    elif msg_type == "abort":
        logger.info("[%s] Playback aborted", device_id)

    else:
        logger.debug("[%s] Unknown message type: %s", device_id, msg_type)


async def _handle_audio(ws: WebSocket, frame: bytes, device_id: str):
    """Handle binary OPUS audio frame from ESP32.

    Decodes OPUS, buffers PCM, runs VAD. On end-of-speech detection,
    automatically triggers transcription and LLM response.
    """
    session = _get_session(device_id)

    if not session.active:
        logger.debug("[%s] Audio frame received but no active session (%d bytes)",
                     device_id, len(frame))
        return

    end_of_speech = session.feed_opus(frame)

    if end_of_speech:
        # VAD triggered end-of-speech — transcribe automatically
        logger.info("[%s] VAD end-of-speech — auto-transcribing", device_id)
        t0 = time.monotonic()
        transcription = await session.stop_and_transcribe()
        t_stt = time.monotonic() - t0

        if transcription:
            logger.info("[%s] Auto-STT (%.1fs): %s", device_id, t_stt, transcription[:80])
            await ws.send_json({"type": "stt", "text": transcription})

            # Feed into LLM pipeline — detect hint routing
            hint_match = re.match(r'^\[HINT:(\w+):(\d)\]\s*(.*)', transcription)
            if hint_match:
                puzzle_id = hint_match.group(1)
                hint_level = int(hint_match.group(2))
                question = hint_match.group(3) or "Give me a hint"
                response = await _query_hints(puzzle_id, question, hint_level, device_id)
            else:
                response = await _query_llm(transcription, device_id)
            await _send_tts_response(ws, response, device_id)
        else:
            logger.warning("[%s] Auto-STT returned empty", device_id)
            await ws.send_json({"type": "stt", "text": ""})


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
    return await _query_llm(question, session_id)


async def _query_llm(text: str, device_id: str = "unknown") -> str:
    """Query LLM via OpenAI-compatible API with per-session conversation history."""
    session = _get_session(device_id)

    # Build messages: system + last N turns from history + current user message
    messages: list[dict[str, str]] = [{"role": "system", "content": PROFESSOR_ZACUS_PROMPT}]
    history_slice = session.conversation_history[-(MAX_HISTORY_TURNS * 2):]
    messages.extend(history_slice)
    messages.append({"role": "user", "content": text})

    try:
        async with httpx.AsyncClient(timeout=10.0) as client:
            resp = await client.post(
                LLM_URL,
                json={
                    "model": LLM_MODEL,
                    "messages": messages,
                    "max_tokens": 150,
                    "temperature": 0.8,
                },
            )
            if resp.status_code == 200:
                data = resp.json()
                choices = data.get("choices", [])
                if choices:
                    reply = choices[0].get("message", {}).get("content", "Hmm...")
                    # Store turn in history
                    session.conversation_history.append({"role": "user", "content": text})
                    session.conversation_history.append({"role": "assistant", "content": reply})
                    return reply
            logger.error("LLM HTTP %d: %s", resp.status_code, resp.text[:200])
    except Exception as exc:
        logger.error("LLM error: %s", exc)

    return "Mon laboratoire semble avoir un probleme technique... Reessaie."


async def _text_to_speech(text: str) -> Optional[bytes]:
    """Convert text to speech via Piper/OpenedAI-Speech API. Returns WAV bytes."""
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


async def _transcribe_audio(wav_bytes: bytes, device_id: str) -> str:
    """Send WAV audio to Whisper STT (OpenAI-compatible) and return text."""
    try:
        async with httpx.AsyncClient(timeout=30.0) as client:
            resp = await client.post(
                STT_URL,
                files={"file": ("audio.wav", wav_bytes, "audio/wav")},
                data={"model": "whisper-1", "language": "fr"},
            )
            if resp.status_code == 200:
                data = resp.json()
                return data.get("text", "").strip()
            logger.error("[%s] STT HTTP %d: %s", device_id, resp.status_code, resp.text[:200])
    except Exception as exc:
        logger.error("[%s] STT error: %s", device_id, exc)

    return ""


async def _send_tts_response(ws: WebSocket, text: str, device_id: str):
    """Get TTS audio, optionally encode to OPUS, and stream to client.

    Protocol:
        1. {"type":"tts","state":"start","text":"..."}
        2. Binary OPUS frames (or single WAV blob if OPUS unavailable)
        3. {"type":"tts","state":"stop"}
    """
    t0 = time.monotonic()
    await ws.send_json({"type": "tts", "state": "start", "text": text})

    wav_audio = await _text_to_speech(text)
    t_tts = time.monotonic() - t0

    if wav_audio:
        if HAS_OPUS:
            # Encode WAV -> OPUS frames and stream
            try:
                pcm, rate, channels = _wav_to_pcm(wav_audio)
                opus_frames = _encode_pcm_to_opus_frames(pcm, rate, channels)
                for frame in opus_frames:
                    await ws.send_bytes(frame)
                logger.info("[%s] TTS OPUS sent: %.1fs, %d frames",
                            device_id, t_tts, len(opus_frames))
            except Exception as exc:
                logger.error("[%s] OPUS encode failed, falling back to WAV: %s",
                             device_id, exc)
                await ws.send_bytes(wav_audio)
        else:
            # No OPUS encoder — send raw WAV blob
            await ws.send_bytes(wav_audio)
            logger.info("[%s] TTS WAV sent: %.1fs, %dKB",
                        device_id, t_tts, len(wav_audio) // 1024)
    else:
        logger.warning("[%s] TTS failed, text-only response", device_id)

    await ws.send_json({"type": "tts", "state": "stop"})


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(description="Zacus Voice Bridge")
    parser.add_argument("--port", type=int, default=8200, help="Listen port")
    parser.add_argument("--host", default="0.0.0.0", help="Bind address")
    parser.add_argument("--tts-url", default=None, help="TTS API URL")
    parser.add_argument("--llm-url", default=None, help="LLM API URL")
    parser.add_argument("--stt-url", default=None, help="Whisper STT API URL")
    args = parser.parse_args()

    global TTS_URL, LLM_URL, STT_URL
    if args.tts_url:
        TTS_URL = args.tts_url
    if args.llm_url:
        LLM_URL = args.llm_url
    if args.stt_url:
        STT_URL = args.stt_url

    logger.info("Starting Voice Bridge on %s:%d", args.host, args.port)
    logger.info("  TTS: %s", TTS_URL)
    logger.info("  LLM: %s", LLM_URL)
    logger.info("  STT: %s", STT_URL)
    logger.info("  OPUS: %s | VAD: %s", HAS_OPUS, HAS_VAD)

    uvicorn.run(app, host=args.host, port=args.port, log_level="info")


if __name__ == "__main__":
    main()
