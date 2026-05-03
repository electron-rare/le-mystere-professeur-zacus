"""voice-bridge — FastAPI daemon (P1 part7 + part9b on-disk cache).

Spec: docs/superpowers/specs/2026-05-03-tts-stt-llm-macstudio-design.md §2.5

Routes
------
GET  /health              → readiness, F5 warm-up, ref audio, cache stats
POST /tts                 → on-disk cache → F5-TTS-MLX → Piper Tower fallback
GET  /tts/cache/stats     → cache count / size / hit rate (since boot)
DELETE /tts/cache         → clear the on-disk cache (admin key required)
POST /voice/transcribe    → multipart proxy → whisper.cpp :8300 /inference
POST /voice/intent        → forward → LiteLLM npc-fast (model="npc-fast")

Design notes
------------
* F5-TTS is loaded **once at boot** (single global F5TTS instance) and warmed
  up with a short dummy phrase.
* Reference audio default: ``~/zacus_reference.wav``. If absent, ``/tmp/ref.wav``
  is generated at boot via ``say -v Thomas`` + afconvert (24 kHz mono).
* On-disk cache (P1 part9b): ``~/voice-bridge/cache/<sha16>.wav`` keyed by
  ``sha256(text|ref_path|steps)``. The pool generator (tools/tts/generate_npc_pool.py)
  uses the same key format so pre-warming is straightforward.
* TTS fallback chain on cache miss: F5 → Piper Tower :8001 → ``service_down.wav``
  if both fail. F5 errors / timeouts are logged then bypassed.
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
from fastapi import FastAPI, File, Header, HTTPException, Request, Response, UploadFile
from fastapi.responses import JSONResponse

# ── config (env-overridable) ────────────────────────────────────────────────
WHISPER_URL = os.getenv("WHISPER_URL", "http://localhost:8300")
LITELLM_URL = os.getenv("LITELLM_URL", "http://localhost:4000")
LITELLM_KEY = os.environ.get("LITELLM_MASTER_KEY", "sk-zacus-local-dev-do-not-share")
PIPER_URL = os.getenv("PIPER_URL", "http://192.168.0.120:8001")
F5_TIMEOUT_S = float(os.getenv("F5_TIMEOUT_S", "8.0"))
F5_MODEL = os.getenv("F5_MODEL", "lucasnewman/f5-tts-mlx")
F5_DEFAULT_STEPS = int(os.getenv("F5_DEFAULT_STEPS", "4"))
# Hard bounds on F5 sampling steps: prevents trivial DoS via large `steps`
# values (each step is a forward pass through the diffusion network and
# blocks the asyncio loop because MLX runs in-thread). Override via env.
F5_STEPS_MIN = int(os.getenv("F5_STEPS_MIN", "1"))
F5_STEPS_MAX = int(os.getenv("F5_STEPS_MAX", "32"))
# Hard cap on `text` length to bound per-request synthesis time / memory.
# 2000 chars ≈ a long paragraph; well above any realistic NPC line.
TTS_TEXT_MAX_CHARS = int(os.getenv("TTS_TEXT_MAX_CHARS", "2000"))
F5_SAMPLE_RATE = 24_000

REF_AUDIO_HOME = Path(os.path.expanduser("~/zacus_reference.wav"))
REF_AUDIO_FALLBACK = Path("/tmp/ref.wav")
REF_AUDIO_TEXT_DEFAULT = (
    "Bienvenue dans le mystère du Professeur Zacus."
)

CACHE_DIR = Path(os.path.expanduser(os.getenv("CACHE_DIR", "~/voice-bridge/cache")))
SERVICE_DOWN_WAV = Path(os.path.expanduser(
    os.getenv("SERVICE_DOWN_WAV", "~/voice-bridge/service_down.wav")
))
ADMIN_KEY = os.getenv("VOICE_BRIDGE_ADMIN_KEY")  # None = no auth on DELETE

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
_REF_AUDIO_HASH: str = "default"
_WARMUP_MS: Optional[int] = None
_F5_LOAD_ERR: Optional[str] = None
# Serialization lock: F5/MLX operations must run in the asyncio main thread
# (MLX caches per-thread state at model-load time). The lock prevents two
# concurrent /tts calls from clobbering MLX state.
_F5_LOCK: Optional[asyncio.Lock] = None

# Cache index: cache_key → absolute path (warm in-memory after boot scan).
_CACHE_INDEX: dict[str, Path] = {}
_CACHE_LOCK: Optional[asyncio.Lock] = None
_CACHE_HITS: int = 0
_CACHE_MISSES: int = 0


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


def _ref_hash(ref_path: Optional[Path]) -> str:
    """Short hash of the reference WAV (file-content based).

    Used in /health to fingerprint the boot-time default reference; NOT used
    in cache key derivation (see ``_voice_ref_token`` for that), so the pool
    generator and the daemon agree on cache keys without sharing the ref file.
    """
    if ref_path is None or not ref_path.exists():
        return "default"
    h = hashlib.sha256()
    with open(ref_path, "rb") as fh:
        for chunk in iter(lambda: fh.read(65536), b""):
            h.update(chunk)
    return h.hexdigest()[:16]


def _voice_ref_token(voice_ref: Optional[str]) -> str:
    """Cache-key token derived from the request's ``voice_ref`` field.

    Contract (mirrored by ``tools/tts/generate_npc_pool.py``): when the client
    omits ``voice_ref`` it gets the daemon's boot-time default reference, so
    both sides hash the literal sentinel ``"default"``. Otherwise both sides
    hash the ``voice_ref`` string (path or identifier) verbatim — never the
    file contents — so the client can compute the cache_key locally without
    needing the actual reference WAV.
    """
    if not voice_ref:
        return "default"
    return hashlib.sha256(voice_ref.encode("utf-8")).hexdigest()[:16]


def _cache_key(text: str, ref_token: str, steps: int) -> str:
    payload = f"{text}|{ref_token}|{steps}".encode("utf-8")
    return hashlib.sha256(payload).hexdigest()[:16]


def _scan_cache_dir() -> dict[str, Path]:
    """Walk CACHE_DIR and build {cache_key: path} from existing .wav files."""
    index: dict[str, Path] = {}
    if not CACHE_DIR.exists():
        return index
    for wav in CACHE_DIR.glob("*.wav"):
        # Use the file stem as key (set when we wrote it).
        index[wav.stem] = wav
    return index


# ── F5-TTS in-process synthesizer ───────────────────────────────────────────
def _f5_synthesize_sync(text: str, steps: int) -> bytes:
    """Run F5 inference on the cached model; returns 24 kHz mono WAV bytes."""
    import mlx.core as mx  # noqa: WPS433
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
    """Run synthesis serialized in the asyncio main thread."""
    assert _F5_LOCK is not None
    async with _F5_LOCK:
        return _f5_synthesize_sync(text, steps)


# ── FastAPI app ──────────────────────────────────────────────────────────────
app = FastAPI(title="voice-bridge", version="0.3.0")


@app.on_event("startup")
async def _boot() -> None:
    global _F5_MODEL_OBJ, _REF_AUDIO_PATH, _REF_AUDIO_HASH, _WARMUP_MS
    global _F5_LOAD_ERR, _F5_LOCK, _CACHE_INDEX, _CACHE_LOCK

    _F5_LOCK = asyncio.Lock()
    _CACHE_LOCK = asyncio.Lock()

    # Cache directory + initial index (cheap; just lists .wav stems).
    CACHE_DIR.mkdir(parents=True, exist_ok=True)
    _CACHE_INDEX = _scan_cache_dir()
    _jlog("boot_cache_index", dir=str(CACHE_DIR), entries=len(_CACHE_INDEX))

    try:
        _REF_AUDIO_PATH = _ensure_ref_audio()
        _REF_AUDIO_HASH = _ref_hash(_REF_AUDIO_PATH)
        _jlog("boot_ref_audio", path=str(_REF_AUDIO_PATH),
              ref_hash=_REF_AUDIO_HASH)
    except Exception as exc:  # pragma: no cover
        _F5_LOAD_ERR = f"ref_audio: {exc}"
        _jlog("boot_ref_audio_failed", err=str(exc))
        return

    try:
        from f5_tts_mlx.cfm import F5TTS

        t0 = time.monotonic()
        _F5_MODEL_OBJ = F5TTS.from_pretrained(F5_MODEL)
        _jlog("boot_f5_loaded", model=F5_MODEL,
              load_ms=int((time.monotonic() - t0) * 1000))

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
    cache_size_b = sum(p.stat().st_size for p in _CACHE_INDEX.values()
                       if p.exists())
    return {
        "status": "ok" if _F5_MODEL_OBJ is not None else "degraded",
        "version": app.version,
        "f5_loaded": _F5_MODEL_OBJ is not None,
        "f5_load_error": _F5_LOAD_ERR,
        "ref_audio_used": str(_REF_AUDIO_PATH) if _REF_AUDIO_PATH else None,
        "ref_hash": _REF_AUDIO_HASH,
        "model_warmup_ms": _WARMUP_MS,
        "cache": {
            "dir": str(CACHE_DIR),
            "entries": len(_CACHE_INDEX),
            "size_mb": round(cache_size_b / (1024 * 1024), 3),
            "hits": _CACHE_HITS,
            "misses": _CACHE_MISSES,
        },
        "backends": {
            "whisper": WHISPER_URL,
            "litellm": LITELLM_URL,
            "piper_fallback": PIPER_URL,
        },
    }


# ── /tts (cache → F5 primary, Piper fallback, service_down last) ─────────────
async def _piper_fallback(text: str) -> Optional[bytes]:
    """POST text to Piper Tower :8001/synthesize. Returns WAV bytes or None.

    NOTE (P1 part9b): the Piper backend is kept as a *fallback only* — F5 is
    the primary TTS path. This block is intentionally not removed so the
    daemon stays operational if F5 fails to load. Marked DEPRECATED for
    primary use; see voice-bridge spec §2.5.
    """
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


def _service_down_response(request_id: str, latency_ms: int,
                           err_kind: Optional[str]) -> Optional[Response]:
    """Return service_down.wav when both F5 and Piper are unavailable."""
    if not SERVICE_DOWN_WAV.exists():
        return None
    try:
        wav_bytes = SERVICE_DOWN_WAV.read_bytes()
    except OSError as exc:
        _jlog("service_down_read_err", err=str(exc))
        return None
    _jlog("tts", request_id=request_id, tts_backend_used="service_down",
          latency_ms=latency_ms, error=err_kind)
    return Response(
        content=wav_bytes,
        media_type="audio/wav",
        headers={
            "X-TTS-Backend": "service_down",
            "X-TTS-Cache-Hit": "false",
            "X-TTS-Latency-Ms": str(latency_ms),
            "X-Request-Id": request_id,
        },
    )


async def _write_cache(cache_key: str, wav_bytes: bytes) -> None:
    """Persist a freshly synthesized WAV under CACHE_DIR/<key>.wav."""
    assert _CACHE_LOCK is not None
    out = CACHE_DIR / f"{cache_key}.wav"
    try:
        # Atomic-ish write: tmp file + rename so partial writes never appear.
        tmp = out.with_suffix(".wav.tmp")
        tmp.write_bytes(wav_bytes)
        tmp.replace(out)
        async with _CACHE_LOCK:
            _CACHE_INDEX[cache_key] = out
        _jlog("cache_write", cache_key=cache_key, bytes=len(wav_bytes))
    except OSError as exc:
        _jlog("cache_write_err", cache_key=cache_key, err=str(exc))


@app.post("/tts")
async def tts(payload: dict[str, Any]) -> Response:
    global _CACHE_HITS, _CACHE_MISSES

    request_id = str(uuid.uuid4())
    text = (payload.get("text") or payload.get("input") or "").strip()
    if not text:
        raise HTTPException(status_code=400, detail="payload.text is required")
    if len(text) > TTS_TEXT_MAX_CHARS:
        _jlog("tts_text_too_long", request_id=request_id,
              text_len=len(text), max=TTS_TEXT_MAX_CHARS)
        raise HTTPException(
            status_code=400,
            detail=f"text too long, max {TTS_TEXT_MAX_CHARS} chars",
        )
    raw_steps = int(payload.get("steps", F5_DEFAULT_STEPS))
    steps = max(F5_STEPS_MIN, min(F5_STEPS_MAX, raw_steps))
    if steps != raw_steps:
        _jlog("tts_steps_clamped", request_id=request_id,
              requested=raw_steps, clamped=steps,
              min=F5_STEPS_MIN, max=F5_STEPS_MAX)
    voice_ref = payload.get("voice_ref")  # accepted; reserved for multi-voice

    started = time.monotonic()

    # 1) Cache lookup -------------------------------------------------------
    cache_key = _cache_key(text, _voice_ref_token(voice_ref), steps)
    cached_path = _CACHE_INDEX.get(cache_key)
    if cached_path is None:
        # Fallback: file may have been dropped in by the pool generator after
        # boot; re-check the dir lazily.
        candidate = CACHE_DIR / f"{cache_key}.wav"
        if candidate.exists():
            cached_path = candidate
            _CACHE_INDEX[cache_key] = candidate

    if cached_path is not None and cached_path.exists():
        try:
            wav_bytes = cached_path.read_bytes()
            _CACHE_HITS += 1
            latency_ms = int((time.monotonic() - started) * 1000)
            _jlog("tts", request_id=request_id, tts_backend_used="cache",
                  latency_ms=latency_ms, phrase_len=len(text),
                  text_hash=_hash8(text), cache_key=cache_key)
            return Response(
                content=wav_bytes,
                media_type="audio/wav",
                headers={
                    "X-TTS-Backend": "cache",
                    "X-TTS-Cache-Hit": "true",
                    "X-TTS-Cache-Key": cache_key,
                    "X-TTS-Latency-Ms": str(latency_ms),
                    "X-Request-Id": request_id,
                },
            )
        except OSError as exc:
            _jlog("cache_read_err", cache_key=cache_key, err=str(exc))
            # Fall through and regenerate.

    _CACHE_MISSES += 1
    err_kind: Optional[str] = None

    # 2) F5 synthesis (cache miss) -----------------------------------------
    if _F5_MODEL_OBJ is not None and _F5_LOCK is not None:
        try:
            wav_bytes = await asyncio.wait_for(
                _run_f5(text, steps),
                timeout=F5_TIMEOUT_S,
            )
            await _write_cache(cache_key, wav_bytes)
            latency_ms = int((time.monotonic() - started) * 1000)
            _jlog("tts", request_id=request_id, tts_backend_used="f5",
                  latency_ms=latency_ms, phrase_len=len(text),
                  text_hash=_hash8(text), voice_ref=voice_ref, steps=steps,
                  cache_key=cache_key)
            return Response(
                content=wav_bytes,
                media_type="audio/wav",
                headers={
                    "X-TTS-Backend": "f5",
                    "X-TTS-Cache-Hit": "false",
                    "X-TTS-Cache-Key": cache_key,
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

    # 3) Piper Tower fallback (DEPRECATED for primary path; safety-net only)
    wav_fallback = await _piper_fallback(text)
    if wav_fallback is not None:
        latency_ms = int((time.monotonic() - started) * 1000)
        _jlog("tts", request_id=request_id, tts_backend_used="piper_fallback",
              latency_ms=latency_ms, phrase_len=len(text),
              text_hash=_hash8(text), error=err_kind)
        return Response(
            content=wav_fallback,
            media_type="audio/wav",
            headers={
                "X-TTS-Backend": "piper_fallback",
                "X-TTS-Cache-Hit": "false",
                "X-TTS-Latency-Ms": str(latency_ms),
                "X-Request-Id": request_id,
            },
        )

    # 4) service_down.wav last resort --------------------------------------
    latency_ms = int((time.monotonic() - started) * 1000)
    sd = _service_down_response(request_id, latency_ms, err_kind)
    if sd is not None:
        return sd

    _jlog("tts_all_backends_down", request_id=request_id,
          phrase_len=len(text), error=err_kind)
    raise HTTPException(
        status_code=503,
        detail=(
            f"TTS unavailable: F5 ({err_kind}), Piper fallback, and "
            "service_down.wav all failed"
        ),
    )


# ── /tts/service_down (direct WAV serve, smoke-test friendly) ───────────────
@app.get("/tts/service_down")
async def tts_service_down() -> Response:
    """Serve the pre-rendered fallback WAV directly, no F5 / Piper call.

    Useful for smoke-testing the resilience asset without forcing a fault.
    Added in P1 part9a (resilience pass).
    """
    if not SERVICE_DOWN_WAV.exists():
        raise HTTPException(
            status_code=503,
            detail=f"service_down WAV missing at {SERVICE_DOWN_WAV}",
        )
    try:
        wav_bytes = SERVICE_DOWN_WAV.read_bytes()
    except OSError as exc:
        raise HTTPException(
            status_code=500,
            detail=f"service_down WAV unreadable: {exc}",
        )
    return Response(
        content=wav_bytes,
        media_type="audio/wav",
        headers={
            "X-TTS-Backend": "service_down",
            "X-TTS-Bytes": str(len(wav_bytes)),
        },
    )


# ── /tts/cache/{stats,clear} ────────────────────────────────────────────────
@app.get("/tts/cache/stats")
async def cache_stats() -> dict[str, Any]:
    total_b = 0
    valid_count = 0
    for p in _CACHE_INDEX.values():
        try:
            total_b += p.stat().st_size
            valid_count += 1
        except OSError:
            continue
    total = _CACHE_HITS + _CACHE_MISSES
    hit_rate = round(_CACHE_HITS / total, 4) if total else 0.0
    return {
        "dir": str(CACHE_DIR),
        "count": valid_count,
        "size_mb": round(total_b / (1024 * 1024), 3),
        "hits": _CACHE_HITS,
        "misses": _CACHE_MISSES,
        "hit_rate_since_boot": hit_rate,
    }


@app.delete("/tts/cache")
async def cache_clear(
    x_admin_key: Optional[str] = Header(default=None, alias="X-Admin-Key"),
) -> dict[str, Any]:
    if ADMIN_KEY and x_admin_key != ADMIN_KEY:
        raise HTTPException(status_code=403, detail="invalid admin key")

    assert _CACHE_LOCK is not None
    removed = 0
    async with _CACHE_LOCK:
        for p in list(_CACHE_INDEX.values()):
            try:
                p.unlink()
                removed += 1
            except OSError:
                continue
        _CACHE_INDEX.clear()
        # Also sweep any stragglers not in the index.
        for stray in CACHE_DIR.glob("*.wav"):
            try:
                stray.unlink()
                removed += 1
            except OSError:
                continue
    _jlog("cache_cleared", removed=removed)
    return {"removed": removed}


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
