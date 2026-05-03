"""voice-bridge — FastAPI daemon (P1 part7, in-process F5-TTS).

Spec: docs/superpowers/specs/2026-05-03-tts-stt-llm-macstudio-design.md §2.5

Routes
------
GET  /health              → readiness, F5 warm-up status, ref audio used
POST /tts                 → in-process F5-TTS-MLX, fallback Piper Tower:8001
POST /voice/transcribe    → multipart proxy → whisper.cpp :8300 /inference
POST /voice/intent        → forward → LiteLLM npc-fast (model="npc-fast")

Design notes
------------
* F5-TTS is loaded **once at boot** (single global F5TTS instance) and warmed
  up with a short dummy phrase; `generate()` from `f5_tts_mlx.generate` is
  bypassed because it (a) reloads the model on each call and (b) tries to
  open a sounddevice when `output_path` is None — both unwanted in a daemon.
* Reference audio default: `~/zacus_reference.wav`. If absent, `/tmp/ref.wav`
  is generated at boot via `say -v Thomas` + afconvert (24 kHz mono).
* TTS fallback: F5 wrapped in `asyncio.to_thread()` with 3 s timeout. On
  timeout / hard error, POST to Piper Tower :8001 /synthesize. If both fail,
  503.
* Master key for LiteLLM: `LITELLM_MASTER_KEY` env var (no hardcoded
  production secret in source).
"""
from __future__ import annotations

import asyncio
import hashlib
import io
import json
import logging
import os
import shutil
import subprocess
import time
import uuid
from pathlib import Path
from typing import Any, Optional

import httpx
import numpy as np
import soundfile as sf
from fastapi import FastAPI, File, HTTPException, Request, Response, UploadFile
from fastapi.responses import JSONResponse

# ── config (env-overridable) ────────────────────────────────────────────────
WHISPER_URL = os.getenv("WHISPER_URL", "http://localhost:8300")
LITELLM_URL = os.getenv("LITELLM_URL", "http://localhost:4000")
LITELLM_KEY = os.environ.get("LITELLM_MASTER_KEY", "sk-zacus-local-dev-do-not-share")
PIPER_URL = os.getenv("PIPER_URL", "http://192.168.0.120:8001")
F5_TIMEOUT_S = float(os.getenv("F5_TIMEOUT_S", "3.0"))
F5_MODEL = os.getenv("F5_MODEL", "lucasnewman/f5-tts-mlx")
F5_DEFAULT_STEPS = int(os.getenv("F5_DEFAULT_STEPS", "8"))
F5_SAMPLE_RATE = 24_000

REF_AUDIO_HOME = Path(os.path.expanduser("~/zacus_reference.wav"))
REF_AUDIO_FALLBACK = Path("/tmp/ref.wav")
REF_AUDIO_TEXT_DEFAULT = (
    "Bienvenue dans le mystère du Professeur Zacus."
)

# ── logging (single-line JSON) ──────────────────────────────────────────────
logging.basicConfig(
    level=logging.INFO,
    format='{"ts":"%(asctime)s","lvl":"%(levelname)s","msg":%(message)s}',
)
log = logging.getLogger("voice-bridge")


def _jlog(event: str, **fields: Any) -> None:
    payload = {"event": event, **fields}
    log.info(json.dumps(payload, ensure_ascii=False))


def _hash8(text: str) -> str:
    return hashlib.sha256(text.encode("utf-8")).hexdigest()[:8]


# ── globals (filled at startup) ─────────────────────────────────────────────
_F5_MODEL_OBJ: Any = None
_REF_AUDIO_PATH: Optional[Path] = None
_REF_AUDIO_TEXT: str = REF_AUDIO_TEXT_DEFAULT
_WARMUP_MS: Optional[int] = None
_F5_LOAD_ERR: Optional[str] = None
# Serialization lock: F5/MLX operations must run in the asyncio main thread
# (MLX caches per-thread state at model-load time and breaks if reused from
# another thread, even with set_default_stream). The lock prevents two
# concurrent /tts calls from clobbering MLX state.
_F5_LOCK: Optional[asyncio.Lock] = None


# ── reference audio bootstrap ───────────────────────────────────────────────
def _ensure_ref_audio() -> Path:
    """Return a usable 24 kHz mono WAV reference audio path.

    Order of preference:
      1. ~/zacus_reference.wav (if exists; trust the operator)
      2. /tmp/ref.wav (regenerated each boot via `say -v Thomas` + afconvert)
    """
    if REF_AUDIO_HOME.exists():
        return REF_AUDIO_HOME

    aiff = REF_AUDIO_FALLBACK.with_suffix(".aiff")
    say_bin = shutil.which("say")
    afconvert_bin = shutil.which("afconvert")
    if not say_bin or not afconvert_bin:
        raise RuntimeError(
            "Cannot bootstrap fallback reference audio: 'say' or 'afconvert' missing"
        )

    # Generate AIFF then convert to WAV 24 kHz mono PCM 16-bit.
    subprocess.run(
        [say_bin, "-v", "Thomas", "-o", str(aiff), REF_AUDIO_TEXT_DEFAULT],
        check=True,
    )
    subprocess.run(
        [
            afconvert_bin,
            "-f", "WAVE",
            "-d", f"LEI16@{F5_SAMPLE_RATE}",
            "-c", "1",
            str(aiff),
            str(REF_AUDIO_FALLBACK),
        ],
        check=True,
    )
    return REF_AUDIO_FALLBACK


# ── F5-TTS in-process synthesizer ───────────────────────────────────────────
def _f5_synthesize_sync(text: str, steps: int) -> bytes:
    """Run F5 inference on the cached model; returns 24 kHz mono WAV bytes.

    Must execute on a thread that has a thread-local GPU stream (see
    ``_f5_thread_init``). The dedicated ``_F5_EXECUTOR`` guarantees this.
    """
    import mlx.core as mx  # noqa: WPS433 (deferred import for clarity)
    from f5_tts_mlx.utils import convert_char_to_pinyin

    audio_arr, sr = sf.read(str(_REF_AUDIO_PATH))
    if sr != F5_SAMPLE_RATE:
        raise RuntimeError(
            f"Reference audio sample rate {sr} ≠ {F5_SAMPLE_RATE} Hz"
        )
    audio = mx.array(audio_arr)

    # RMS normalization (mirrors generate.py defaults)
    target_rms = 0.1
    rms = mx.sqrt(mx.mean(mx.square(audio)))
    if float(rms) < target_rms:
        audio = audio * target_rms / rms

    prompt = convert_char_to_pinyin([_REF_AUDIO_TEXT + " " + text])
    wave, _ = _F5_MODEL_OBJ.sample(
        mx.expand_dims(audio, axis=0),
        text=prompt,
        duration=None,
        steps=steps,
        method="rk4",
        speed=1.0,
        cfg_strength=2.0,
        sway_sampling_coef=-1.0,
        seed=None,
    )
    wave = wave[audio.shape[0]:]
    mx.eval(wave)

    pcm = np.array(wave, dtype=np.float32)
    buf = io.BytesIO()
    sf.write(buf, pcm, F5_SAMPLE_RATE, format="WAV", subtype="PCM_16")
    return buf.getvalue()


async def _run_f5(text: str, steps: int) -> bytes:
    """Run synthesis serialized in the asyncio main thread.

    F5/MLX state is bound to the thread that loaded the model. Running from
    a worker thread triggers ``RuntimeError: There is no Stream(gpu, 0)`` or
    ``scaled_dot_product_attention(scale: array)`` even with explicit stream
    setup, so we keep everything on the event-loop thread and use an
    ``asyncio.Lock`` to serialize concurrent requests. This blocks the event
    loop for the duration of synthesis; that is acceptable for the escape
    room workload (max 2 concurrent voice clients).
    """
    assert _F5_LOCK is not None
    async with _F5_LOCK:
        return _f5_synthesize_sync(text, steps)


# ── FastAPI app ──────────────────────────────────────────────────────────────
app = FastAPI(title="voice-bridge", version="0.2.0")


@app.on_event("startup")
async def _boot() -> None:
    global _F5_MODEL_OBJ, _REF_AUDIO_PATH, _WARMUP_MS, _F5_LOAD_ERR
    global _F5_LOCK

    _F5_LOCK = asyncio.Lock()

    try:
        _REF_AUDIO_PATH = _ensure_ref_audio()
        _jlog("boot_ref_audio", path=str(_REF_AUDIO_PATH))
    except Exception as exc:  # pragma: no cover - defensive
        _F5_LOAD_ERR = f"ref_audio: {exc}"
        _jlog("boot_ref_audio_failed", err=str(exc))
        return

    try:
        from f5_tts_mlx.cfm import F5TTS

        t0 = time.monotonic()
        _F5_MODEL_OBJ = F5TTS.from_pretrained(F5_MODEL)
        _jlog("boot_f5_loaded", model=F5_MODEL,
              load_ms=int((time.monotonic() - t0) * 1000))

        # Warm-up: short dummy synth in the same thread that holds the model.
        t1 = time.monotonic()
        _ = _f5_synthesize_sync("Test.", steps=F5_DEFAULT_STEPS)
        _WARMUP_MS = int((time.monotonic() - t1) * 1000)
        _jlog("boot_f5_warmup_done", warmup_ms=_WARMUP_MS,
              steps=F5_DEFAULT_STEPS)
    except Exception as exc:  # pragma: no cover
        _F5_LOAD_ERR = f"f5: {exc}"
        _jlog("boot_f5_failed", err=str(exc))


@app.get("/health")
async def health() -> dict[str, Any]:
    return {
        "status": "ok" if _F5_MODEL_OBJ is not None else "degraded",
        "version": app.version,
        "f5_loaded": _F5_MODEL_OBJ is not None,
        "f5_load_error": _F5_LOAD_ERR,
        "ref_audio_used": str(_REF_AUDIO_PATH) if _REF_AUDIO_PATH else None,
        "model_warmup_ms": _WARMUP_MS,
        "backends": {
            "whisper": WHISPER_URL,
            "litellm": LITELLM_URL,
            "piper_fallback": PIPER_URL,
        },
    }


# ── /tts (F5 primary, Piper fallback) ───────────────────────────────────────
async def _piper_fallback(text: str) -> Optional[bytes]:
    """POST text to Piper Tower :8001/synthesize. Returns WAV bytes or None."""
    try:
        async with httpx.AsyncClient(timeout=10.0) as client:
            resp = await client.post(
                f"{PIPER_URL}/synthesize",
                json={"text": text},
            )
        if resp.status_code == 200:
            return resp.content
        _jlog("piper_fallback_nonok", status=resp.status_code)
    except (httpx.ConnectError, httpx.TimeoutException, httpx.HTTPError) as exc:
        _jlog("piper_fallback_unreachable", err=type(exc).__name__)
    return None


@app.post("/tts")
async def tts(payload: dict[str, Any]) -> Response:
    request_id = str(uuid.uuid4())
    text = (payload.get("text") or payload.get("input") or "").strip()
    if not text:
        raise HTTPException(status_code=400, detail="payload.text is required")
    steps = int(payload.get("steps", F5_DEFAULT_STEPS))
    voice_ref = payload.get("voice_ref")  # accepted but ignored (single-voice)

    started = time.monotonic()
    backend = "unknown"
    err_kind: Optional[str] = None

    if _F5_MODEL_OBJ is not None and _F5_LOCK is not None:
        try:
            wav_bytes = await asyncio.wait_for(
                _run_f5(text, steps),
                timeout=F5_TIMEOUT_S,
            )
            backend = "f5"
            latency_ms = int((time.monotonic() - started) * 1000)
            _jlog("tts", request_id=request_id, tts_backend_used=backend,
                  latency_ms=latency_ms, phrase_len=len(text),
                  text_hash=_hash8(text), voice_ref=voice_ref, steps=steps)
            return Response(
                content=wav_bytes,
                media_type="audio/wav",
                headers={
                    "X-TTS-Backend": "f5",
                    "X-TTS-Latency-Ms": str(latency_ms),
                    "X-Request-Id": request_id,
                },
            )
        except asyncio.TimeoutError:
            err_kind = "timeout"
            _jlog("tts_f5_timeout", request_id=request_id,
                  timeout_s=F5_TIMEOUT_S, phrase_len=len(text))
        except Exception as exc:  # noqa: BLE001
            err_kind = type(exc).__name__
            _jlog("tts_f5_error", request_id=request_id, err=err_kind, msg=str(exc))
    else:
        err_kind = "f5_not_loaded"
        _jlog("tts_f5_not_loaded", request_id=request_id, load_err=_F5_LOAD_ERR)

    # Fallback: Piper Tower
    wav_fallback = await _piper_fallback(text)
    if wav_fallback is not None:
        backend = "piper_fallback"
        latency_ms = int((time.monotonic() - started) * 1000)
        _jlog("tts", request_id=request_id, tts_backend_used=backend,
              latency_ms=latency_ms, phrase_len=len(text),
              text_hash=_hash8(text), error=err_kind)
        return Response(
            content=wav_fallback,
            media_type="audio/wav",
            headers={
                "X-TTS-Backend": "piper_fallback",
                "X-TTS-Latency-Ms": str(latency_ms),
                "X-Request-Id": request_id,
            },
        )

    _jlog("tts_all_backends_down", request_id=request_id,
          phrase_len=len(text), error=err_kind)
    raise HTTPException(
        status_code=503,
        detail=f"TTS unavailable: F5 ({err_kind}) and Piper fallback both failed",
    )


# ── /voice/transcribe (multipart proxy → whisper.cpp) ───────────────────────
@app.post("/voice/transcribe")
async def transcribe(request: Request) -> Response:
    request_id = str(uuid.uuid4())
    body = await request.body()
    started = time.monotonic()
    try:
        async with httpx.AsyncClient(timeout=60.0) as client:
            resp = await client.post(
                f"{WHISPER_URL}/inference",
                content=body,
                headers={
                    "Content-Type": request.headers.get(
                        "Content-Type", "multipart/form-data"
                    ),
                },
            )
        latency_ms = int((time.monotonic() - started) * 1000)
        _jlog("transcribe", request_id=request_id, status=resp.status_code,
              bytes=len(body), latency_ms=latency_ms)
        return Response(
            content=resp.content,
            status_code=resp.status_code,
            media_type=resp.headers.get("Content-Type", "application/json"),
            headers={"X-Request-Id": request_id,
                     "X-Latency-Ms": str(latency_ms)},
        )
    except (httpx.ConnectError, httpx.TimeoutException) as exc:
        _jlog("transcribe_backend_down", request_id=request_id,
              url=WHISPER_URL, err=type(exc).__name__)
        raise HTTPException(status_code=502, detail="whisper.cpp unreachable")


# ── /voice/intent (LiteLLM npc-fast) ────────────────────────────────────────
@app.post("/voice/intent")
async def intent(payload: dict[str, Any]) -> JSONResponse:
    request_id = str(uuid.uuid4())
    text = (payload.get("text") or "").strip()
    context = payload.get("context")
    if not text:
        raise HTTPException(status_code=400, detail="payload.text is required")

    messages: list[dict[str, str]] = []
    if context:
        messages.append({"role": "system", "content": str(context)})
    messages.append({"role": "user", "content": text})

    started = time.monotonic()
    try:
        async with httpx.AsyncClient(timeout=30.0) as client:
            resp = await client.post(
                f"{LITELLM_URL}/v1/chat/completions",
                headers={"Authorization": f"Bearer {LITELLM_KEY}"},
                json={"model": "npc-fast", "messages": messages},
            )
        latency_ms = int((time.monotonic() - started) * 1000)
        if resp.status_code != 200:
            _jlog("intent_upstream_err", request_id=request_id,
                  status=resp.status_code, latency_ms=latency_ms,
                  body=resp.text[:300])
            return JSONResponse(content=resp.json(), status_code=resp.status_code)
        body = resp.json()
        content = body.get("choices", [{}])[0].get("message", {}).get("content", "")
        _jlog("intent", request_id=request_id, latency_ms=latency_ms,
              text_len=len(text), reply_len=len(content),
              text_hash=_hash8(text))
        return JSONResponse(
            content={
                "request_id": request_id,
                "content": content,
                "model": body.get("model", "npc-fast"),
                "usage": body.get("usage"),
                "latency_ms": latency_ms,
            },
        )
    except (httpx.ConnectError, httpx.TimeoutException) as exc:
        _jlog("intent_backend_down", request_id=request_id,
              url=LITELLM_URL, err=type(exc).__name__)
        raise HTTPException(status_code=502, detail=f"LiteLLM unreachable: {exc}")


if __name__ == "__main__":  # pragma: no cover
    import uvicorn

    uvicorn.run(app, host="0.0.0.0", port=8200)
