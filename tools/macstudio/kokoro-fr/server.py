"""Kokoro-82M FR HTTP TTS server — minimal /synthesize endpoint.

Designed as the second-tier TTS fallback in the voice-bridge chain:

    cache → F5-TTS (primary, voice-cloned Zacus persona)
          → Piper (Tower :8001, EN-only today)
          → Kokoro-FR (this server, MacStudio :8002, fast FR neutral voice)

Surface mirrors the Tower Piper server so the voice-bridge
``_piper_fallback`` helper can be reused with just a URL change:

    POST /synthesize  body: {"text": "...", "speaker_id": "ff_siwis"}
                      returns: audio/wav PCM16 mono 24 kHz

Voice ``ff_siwis`` is Kokoro's only French voice as of 2026-05;
B- grade on the Kokoro authors' scale (cf. VOICES.md). The
underlying model is ``prince-canuma/Kokoro-82M`` which ships the
MLX weights ; the alternative ``hexgrad/Kokoro-82M`` works through
the regular kokoro PyPI package but is non-MLX.

Configuration:
    KOKORO_MODEL    HuggingFace repo (default prince-canuma/Kokoro-82M)
    KOKORO_VOICE    voice id (default ff_siwis)
    KOKORO_LANG     language code (default 'f' for French)
    KOKORO_PORT     listen port (default 8002)
"""
from __future__ import annotations

import io
import logging
import os
import time
import uuid
from pathlib import Path

import numpy as np
import soundfile as sf
from fastapi import FastAPI, HTTPException
from fastapi.responses import Response
from mlx_audio.tts.generate import generate_audio
from pydantic import BaseModel, Field


KOKORO_MODEL = os.getenv("KOKORO_MODEL", "prince-canuma/Kokoro-82M")
KOKORO_VOICE_DEFAULT = os.getenv("KOKORO_VOICE", "ff_siwis")
KOKORO_LANG = os.getenv("KOKORO_LANG", "f")
KOKORO_SR = 24_000
KOKORO_TMP = Path(os.getenv("KOKORO_TMP", "/tmp/kokoro-tts"))
KOKORO_TMP.mkdir(parents=True, exist_ok=True)

logging.basicConfig(level=logging.INFO, format="%(asctime)s %(levelname)s %(message)s")
LOG = logging.getLogger("kokoro-fr")

app = FastAPI(title="kokoro-fr", version="0.1.0")


class SynthesizeRequest(BaseModel):
    text: str = Field(min_length=1, max_length=2000)
    # speaker_id is the field name Piper Tower uses — kept for drop-in
    # compatibility with the voice-bridge _piper_fallback helper.
    speaker_id: str | None = None


@app.get("/health")
def health() -> dict:
    return {
        "status": "ok",
        "model": KOKORO_MODEL,
        "default_voice": KOKORO_VOICE_DEFAULT,
        "lang": KOKORO_LANG,
        "sample_rate": KOKORO_SR,
    }


@app.post("/synthesize")
def synthesize(req: SynthesizeRequest) -> Response:
    voice = req.speaker_id or KOKORO_VOICE_DEFAULT
    # generate_audio writes to disk and we read it back — not pretty,
    # but mlx-audio 0.4.3 has no in-memory API yet. Use a unique
    # prefix so concurrent requests don't race.
    rid = uuid.uuid4().hex[:12]
    prefix = KOKORO_TMP / f"kk_{rid}"
    t0 = time.monotonic()
    try:
        generate_audio(
            text=req.text,
            model=KOKORO_MODEL,
            voice=voice,
            lang_code=KOKORO_LANG,
            file_prefix=str(prefix),
            audio_format="wav",
            verbose=False,
        )
    except Exception as exc:  # noqa: BLE001
        LOG.exception("generate_audio failed")
        raise HTTPException(status_code=500, detail=f"kokoro: {exc}") from exc
    latency_ms = int((time.monotonic() - t0) * 1000)

    # generate_audio appends "_000.wav" to the prefix; for very long
    # inputs it may emit several files. We concatenate them in order.
    parts = sorted(KOKORO_TMP.glob(f"kk_{rid}_*.wav"))
    if not parts:
        raise HTTPException(status_code=500, detail="kokoro produced no audio")
    try:
        chunks = []
        sr_seen = None
        for p in parts:
            data, sr = sf.read(p, dtype="int16", always_2d=False)
            if sr_seen is None:
                sr_seen = sr
            chunks.append(data)
        merged = np.concatenate(chunks).astype(np.int16)
        buf = io.BytesIO()
        sf.write(buf, merged, sr_seen or KOKORO_SR, format="WAV", subtype="PCM_16")
        wav_bytes = buf.getvalue()
    finally:
        for p in parts:
            try:
                p.unlink()
            except OSError:
                pass

    LOG.info("synth ok text=%d voice=%s latency_ms=%d bytes=%d",
             len(req.text), voice, latency_ms, len(wav_bytes))
    return Response(
        content=wav_bytes,
        media_type="audio/wav",
        headers={"X-Kokoro-Latency-Ms": str(latency_ms), "X-Kokoro-Voice": voice},
    )
