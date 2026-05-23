"""Kyutai STT streaming client for voice-bridge.

Replaces the batch ``_whisper_transcribe_pcm`` path with a true streaming
WebSocket call to ``moshi-server`` (Rust, Candle/Metal) at
``$KYUTAI_STT_URL``. Each ``Word`` event from the upstream produces a
partial ``{"type":"stt","final":false,"text":...}`` that the caller
forwards to the firmware WebSocket, and the final cumulative transcript
is returned for the existing intent/TTS chain to consume.

The function is intentionally tolerant: on any websocket / network /
protocol failure it raises ``KyutaiSttError`` so the caller can fall
back to the legacy whisper.cpp batch path without leaking partial state.

Config (all optional):
    KYUTAI_STT_URL   ws:// URL of moshi-server /api/asr-streaming
                     (default ws://localhost:8304/api/asr-streaming)
    KYUTAI_STT_KEY   api key sent in the ``kyutai-api-key`` header
                     (default ``zacus-lan-stt``, matching MOSHI_STT_DEPLOY.md)
"""
from __future__ import annotations

import asyncio
import logging
import os
from typing import Awaitable, Callable, Optional

import msgpack
import numpy as np
import websockets
from scipy.signal import resample_poly


LOG = logging.getLogger("voice-bridge.kyutai")

KYUTAI_STT_URL = os.getenv(
    "KYUTAI_STT_URL", "ws://localhost:8304/api/asr-streaming"
)
KYUTAI_STT_KEY = os.getenv("KYUTAI_STT_KEY", "zacus-lan-stt")

# Kyutai's Mimi audio tokenizer expects 24 kHz mono float32.
KYUTAI_SR = 24_000
# 80 ms frames — same as the official Kyutai sample client. Lower frame
# sizes drive up WS overhead without helping latency (Mimi codec runs at
# 12.5 Hz so it consumes one token per 80 ms regardless of how we slice).
FRAME_SAMPLES = 1920
# How many seconds of silence to send before / after the real audio. The
# 1B en_fr model needs a short pre-roll to warm up, and a post-roll long
# enough for the model to emit trailing words + the Marker echo (the
# model delays output relative to input by ~6 audio tokens = ~0.5 s).
SILENCE_PREROLL_S = 1.0
SILENCE_POSTROLL_S = 3.0
SILENCE_MARKER_FLUSH_S = 4.0

# Per-call hard ceiling so a hung Kyutai never blocks the firmware WS
# indefinitely. 30 s of audio at RTF=1 + protocol overhead → 45 s is safe.
DEFAULT_TIMEOUT_S = 45.0


class KyutaiSttError(RuntimeError):
    """Raised on any transport / protocol failure with Kyutai STT.

    Caller is expected to log + fall back to the legacy batch path.
    """


def _pcm16_to_float32(pcm: bytes) -> np.ndarray:
    """Decode little-endian PCM16 mono bytes to float32 in ``[-1, 1]``."""
    if len(pcm) % 2 != 0:
        # Drop the trailing odd byte rather than crashing — firmware
        # occasionally sends an unaligned final frame on reconnects.
        pcm = pcm[:-1]
    return np.frombuffer(pcm, dtype=np.int16).astype(np.float32) / 32768.0


def _resample_16k_to_24k(samples: np.ndarray) -> np.ndarray:
    """Mono 16 kHz float32 → 24 kHz float32 via polyphase 3/2 upsample.

    scipy ``resample_poly`` applies an anti-alias FIR before decimation,
    which is what we want for clean STT input. ~1 ms per second of audio
    on M3 Ultra, negligible vs the 80 ms Mimi frame budget.
    """
    return resample_poly(samples, up=3, down=2).astype(np.float32)


def _audio_msg(samples: np.ndarray) -> bytes:
    """msgpack-pack a float32 frame as the upstream ``Audio`` event."""
    # use_single_float=True keeps the wire payload at 4 bytes per sample.
    return msgpack.packb(
        {"type": "Audio", "pcm": [float(x) for x in samples]},
        use_single_float=True,
    )


def _silence(seconds: float) -> np.ndarray:
    return np.zeros(int(KYUTAI_SR * seconds), dtype=np.float32)


async def kyutai_transcribe_streaming(
    pcm16_16k: bytes,
    on_partial: Optional[Callable[[str], Awaitable[None]]] = None,
    *,
    timeout_s: float = DEFAULT_TIMEOUT_S,
    url: str = KYUTAI_STT_URL,
    api_key: str = KYUTAI_STT_KEY,
) -> str:
    """Stream PCM16 16 kHz mono to Kyutai STT, emitting partials.

    Args:
        pcm16_16k: raw PCM16 little-endian mono samples at 16 kHz (the
            format the firmware sends over ``/voice/ws``).
        on_partial: optional async callback invoked with the cumulative
            transcript (space-joined words so far) whenever a new
            ``Word`` event arrives. The caller typically forwards this
            as ``{"type":"stt","final":false,"text":...}`` to its own
            downstream WebSocket.
        timeout_s: hard ceiling for the whole exchange (sender +
            receiver). Includes the post-roll silence + Marker echo.
        url, api_key: override the env-derived defaults if needed.

    Returns:
        The final cumulative transcript (same shape as a ``Word`` text
        join, lightly stripped). Empty string is a valid return value
        when Kyutai emits no words (silence / non-speech input).

    Raises:
        KyutaiSttError on websocket / network / msgpack failure. Caller
        should fall back to whisper.cpp batch transcription.
    """
    samples_24k = _resample_16k_to_24k(_pcm16_to_float32(pcm16_16k))
    headers = {"kyutai-api-key": api_key}
    words: list[str] = []

    async def receiver(ws: "websockets.WebSocketClientProtocol") -> None:
        async for raw in ws:
            try:
                evt = msgpack.unpackb(raw, raw=False)
            except (msgpack.exceptions.UnpackException, ValueError) as exc:
                raise KyutaiSttError(f"bad msgpack from upstream: {exc}") from exc
            etype = evt.get("type")
            if etype == "Word":
                txt = (evt.get("text") or "").strip()
                if not txt:
                    continue
                words.append(txt)
                if on_partial is not None:
                    try:
                        await on_partial(" ".join(words))
                    except Exception:
                        # A failing partial-forward should not kill the
                        # whole transcription — log and continue.
                        LOG.exception("on_partial callback raised")
            elif etype == "Marker":
                # Server confirmed end-of-stream → done.
                return
            # Step / EndWord intentionally ignored: Step is the semantic
            # VAD signal (useful later for barge-in) and EndWord just
            # refines a timestamp we already have.

    async def sender(ws: "websockets.WebSocketClientProtocol") -> None:
        # Pre-roll silence so the LM warms up before real audio.
        await ws.send(_audio_msg(_silence(SILENCE_PREROLL_S)))

        for i in range(0, len(samples_24k), FRAME_SAMPLES):
            await ws.send(_audio_msg(samples_24k[i : i + FRAME_SAMPLES]))

        # Post-roll so any trailing words make it through the LM delay.
        await ws.send(_audio_msg(_silence(SILENCE_POSTROLL_S)))

        # Send the Marker, then enough silence afterwards for the
        # server to actually emit its Marker echo (the model trails its
        # output by a few audio tokens).
        await ws.send(
            msgpack.packb({"type": "Marker", "id": 0}, use_single_float=True)
        )
        await ws.send(_audio_msg(_silence(SILENCE_MARKER_FLUSH_S)))

    try:
        async with websockets.connect(
            url, additional_headers=headers, max_size=None
        ) as ws:
            await asyncio.wait_for(
                asyncio.gather(sender(ws), receiver(ws)),
                timeout=timeout_s,
            )
    except KyutaiSttError:
        raise
    except asyncio.TimeoutError as exc:
        raise KyutaiSttError(f"timeout after {timeout_s}s") from exc
    except (
        websockets.WebSocketException,
        OSError,
        msgpack.exceptions.UnpackException,
    ) as exc:
        raise KyutaiSttError(f"{type(exc).__name__}: {exc}") from exc

    return " ".join(words).strip()
