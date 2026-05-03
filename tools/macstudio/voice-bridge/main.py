"""voice-bridge — FastAPI daemon (P1 part7 + part9b cache + part11/part12 WS + part13 NPC prompt/CORS).

Spec: docs/superpowers/specs/2026-05-03-tts-stt-llm-macstudio-design.md §2.5

Routes
------
GET  /health              → liveness probe (always 200 once process is up)
GET  /health/ready        → readiness probe (503 until F5 warm-up done)
POST /tts                 → on-disk cache → F5-TTS-MLX → Piper Tower fallback
                            Pydantic-validated body, per-IP rate-limited.
GET  /tts/cache/stats     → cache count / size / hit rate (since boot)
DELETE /tts/cache         → clear the on-disk cache (admin key required)
POST /voice/transcribe    → multipart proxy → whisper.cpp :8300 /inference
POST /voice/intent        → forward → LiteLLM npc-fast (model="npc-fast")
WS   /voice/ws            → STT streaming for ESP32 firmware (PCM16 16 kHz mono).
                            Hello-handshake, binary frames, end → whisper.cpp →
                            optional intent forward → optional F5 TTS reply
                            stream (PCM16 24 kHz, chunked binary), then close.

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

P1 part13 additions
-------------------
* CORS middleware (origins via $CORS_ORIGINS, comma-separated allow-list,
  defaults to Vite dev/preview ports on localhost). ``*`` is intentionally
  forbidden because we keep ``allow_credentials=True``: browsers reject the
  combination per the Fetch spec. Adjust the env var per deployment when
  exposing the dashboard from a non-localhost origin.
* NPC system prompt injection (env $NPC_SYSTEM_PROMPT, multiline allowed):
  every call to LiteLLM ``npc-fast`` from ``/voice/intent`` and ``/voice/ws``
  is prefixed with a system message that anchors the LLM to the Professeur
  Zacus persona and explicitly tolerates whisper transliteration drift on
  the name (« Zaku » / « Zacusse » → still addressed to Zacus). The system
  message is *only* injected when the caller has not already supplied a
  ``system`` role / ``messages`` override in the request context, so power
  users keep full control.
"""
from __future__ import annotations

import asyncio
import hashlib
import io
import json
import logging
import os
import shutil
import struct
import subprocess
import time
import uuid
from pathlib import Path
from typing import Any, Dict, Optional

import httpx
import numpy as np
import soundfile as sf
from fastapi import (
    FastAPI,
    File,
    Header,
    HTTPException,
    Request,
    Response,
    UploadFile,
    WebSocket,
    WebSocketDisconnect,
    status,
)
from fastapi.middleware.cors import CORSMiddleware
from fastapi.responses import JSONResponse
from pydantic import BaseModel, Field
from slowapi import Limiter, _rate_limit_exceeded_handler
from slowapi.errors import RateLimitExceeded
from slowapi.util import get_remote_address

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

# Rate limit on /tts (per client IP). slowapi syntax: "<n>/<unit>".
TTS_RATE_LIMIT = os.getenv("TTS_RATE_LIMIT", "10/second")

# CORS allow-list (P1 part13): comma-separated origins. Default covers the
# local Vite dev server (5174 = atelier, 5173 = dashboard alt port) and the
# vite preview build (4173). Override per-deployment via $CORS_ORIGINS when
# the dashboard ships from a remote origin (Tailscale, prod hostname, etc.).
# NOTE: ``*`` is forbidden by browsers when ``allow_credentials=True`` (Fetch
# spec); we therefore enumerate explicit origins.
CORS_ORIGINS = os.getenv(
    "CORS_ORIGINS",
    "http://localhost:5174,http://localhost:5173,http://localhost:4173",
)

# NPC system prompt (P1 part13): prefixed to every LiteLLM ``npc-fast`` call
# coming through ``/voice/intent`` and ``/voice/ws`` so the LLM stays in the
# Professeur Zacus persona and tolerates whisper-side name drift. Override
# per-deployment via $NPC_SYSTEM_PROMPT (multiline allowed in shell via $'…').
NPC_SYSTEM_PROMPT = os.getenv(
    "NPC_SYSTEM_PROMPT",
    (
        "Tu es le Professeur Zacus, savant excentrique et théâtral, dans un "
        "escape room français. Le joueur peut prononcer ton nom de plusieurs "
        "façons (« Zacus », « Zaku », « Zacusse ») suite aux imperfections de "
        "la transcription audio — accepte toutes les variantes comme te "
        "désignant. Réponds toujours en français, deux phrases maximum, ton "
        "dramatique mais clair. Ne révèle jamais directement la solution "
        "d'une énigme — propose seulement un indice subtil ou une réflexion "
        "qui guide le joueur. Si le joueur demande de l'aide, suggère qu'il "
        "décroche le téléphone du Professeur pour recevoir un indice complet."
    ),
)

# /voice/ws constants — must mirror firmware-side framing (chantier C_slice7).
WS_EXPECTED_VERSION = 1
WS_EXPECTED_FORMAT = "pcm_s16"
WS_EXPECTED_SR = 16_000
WS_BYTES_PER_SAMPLE = 2  # PCM 16-bit mono
WS_MAX_DURATION_S = 30
WS_MAX_BYTES = WS_EXPECTED_SR * WS_BYTES_PER_SAMPLE * WS_MAX_DURATION_S  # 960 KB
WS_FORWARD_INTENT = os.getenv("WS_FORWARD_INTENT", "1") not in {"0", "false", "no"}
# Server-side TTS over WS (P1 part12). Default: mirror WS_FORWARD_INTENT, since
# synthesizing the intent reply is the natural follow-up. Can be force-disabled
# (e.g. for STT-only smoke tests) by setting WS_FORWARD_TTS=0 explicitly.
_WS_FORWARD_TTS_RAW = os.getenv("WS_FORWARD_TTS")
if _WS_FORWARD_TTS_RAW is None:
    WS_FORWARD_TTS = WS_FORWARD_INTENT
else:
    WS_FORWARD_TTS = _WS_FORWARD_TTS_RAW not in {"0", "false", "no"}
# Output PCM framing: 24 kHz mono int16 (matches F5 native sample rate).
WS_TTS_OUTPUT_SR = F5_SAMPLE_RATE  # 24_000
WS_TTS_BYTES_PER_SAMPLE = 2  # PCM 16-bit mono
WS_TTS_CHUNK_BYTES = int(os.getenv("WS_TTS_CHUNK_BYTES", "4096"))  # ~85 ms @ 24 kHz
WS_TTS_MAX_DURATION_S = 30
WS_TTS_MAX_BYTES = (
    WS_TTS_OUTPUT_SR * WS_TTS_BYTES_PER_SAMPLE * WS_TTS_MAX_DURATION_S
)  # 1.44 MB
# steps used when synthesizing the intent reply over WS (kept conservative).
WS_TTS_STEPS = max(F5_STEPS_MIN, min(F5_STEPS_MAX, F5_DEFAULT_STEPS))

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


# ── request models (Pydantic v2) ────────────────────────────────────────────
# Backwards-compat: Pydantic accepts the same JSON shape as the previous
# `dict[str, Any]` handler (text/steps/voice_ref). Validation now returns
# 422 on malformed bodies (length, type), instead of generic 400/500s.
class TtsRequest(BaseModel):
    # `text` is required and capped (cap also enforced explicitly in the
    # handler so we keep the existing structured 400 error message). Pydantic
    # emits 422 with a clear schema error if the field is missing entirely.
    text: str = Field(min_length=1, max_length=TTS_TEXT_MAX_CHARS)
    # `steps` is intentionally *not* clamped here: the handler still applies
    # max(MIN, min(MAX, raw_steps)) for backwards compatibility with existing
    # smoke tests that pass values like 99 and expect a 200 + clamp log
    # rather than a 422. Pydantic only ensures the value is an int.
    steps: int = Field(default=F5_DEFAULT_STEPS)
    voice_ref: Optional[str] = None
    # Legacy alias for the older daemon ("input" instead of "text") is handled
    # in the handler before validation, so the schema stays small.

    model_config = {"extra": "ignore"}


class IntentRequest(BaseModel):
    text: str = Field(min_length=1)
    context: Optional[Dict[str, Any]] = None

    model_config = {"extra": "ignore"}


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
# slowapi limiter keyed by client IP. Rate is configurable via $TTS_RATE_LIMIT.
# Health probes, cache stats and the static service_down asset are exempt
# (see decorator list below) — they must remain hammerable by Tailscale probes.
limiter = Limiter(key_func=get_remote_address, default_limits=[])

app = FastAPI(title="voice-bridge", version="0.4.0")
app.state.limiter = limiter
app.add_exception_handler(RateLimitExceeded, _rate_limit_exceeded_handler)

# CORS (P1 part13). Mounted before any router/route so preflight OPTIONS hits
# the middleware first. Methods enumerated explicitly (GET/POST/DELETE/OPTIONS)
# rather than ``*`` to keep the surface small and predictable; add new verbs
# here when introducing new routes (e.g. PUT for cache invalidation patterns).
_cors_allow_origins = [o.strip() for o in CORS_ORIGINS.split(",") if o.strip()]
app.add_middleware(
    CORSMiddleware,
    allow_origins=_cors_allow_origins,
    allow_credentials=True,
    allow_methods=["GET", "POST", "DELETE", "OPTIONS"],
    allow_headers=["*"],
)
_jlog("boot_cors_configured", origins=_cors_allow_origins)


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
    """Liveness probe — always 200 once the process accepts connections.

    Use ``/health/ready`` for readiness (i.e. F5 warmed up). Splitting the two
    lets watchdogs distinguish "process dead, restart now" (no /health) from
    "process up but not ready yet" (200 here, 503 on /ready).
    """
    cache_size_b = sum(p.stat().st_size for p in _CACHE_INDEX.values()
                       if p.exists())
    return {
        "status": "ok",
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


@app.get("/health/ready")
async def health_ready() -> JSONResponse:
    """Readiness probe — 503 until F5 is loaded *and* warmed up.

    Body is the same regardless of status so callers can parse safely:
        {"ready": bool, "f5_loaded": bool, "warmup_ms": int|None,
         "cache_size": int}
    """
    ready = _F5_MODEL_OBJ is not None and _WARMUP_MS is not None
    body = {
        "ready": ready,
        "f5_loaded": _F5_MODEL_OBJ is not None,
        "warmup_ms": _WARMUP_MS,
        "cache_size": len(_CACHE_INDEX),
    }
    code = status.HTTP_200_OK if ready else status.HTTP_503_SERVICE_UNAVAILABLE
    return JSONResponse(content=body, status_code=code)


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
@limiter.limit(TTS_RATE_LIMIT)
async def tts(request: Request) -> Response:
    """Synthesize one phrase. Pydantic-validated body, per-IP rate limit.

    The slowapi decorator requires ``request: Request`` as a positional arg
    so it can extract the client IP. We hand-parse the body afterwards so we
    can keep accepting the legacy ``input`` alias and emit the same custom
    errors as before.
    """
    global _CACHE_HITS, _CACHE_MISSES

    request_id = str(uuid.uuid4())

    # Parse + validate. We map the legacy "input" key to "text" before handing
    # the dict to Pydantic so the model schema stays clean.
    try:
        raw_body = await request.json()
    except Exception:
        raise HTTPException(status_code=400, detail="invalid JSON body")
    if not isinstance(raw_body, dict):
        raise HTTPException(status_code=400, detail="JSON body must be an object")
    if "text" not in raw_body and "input" in raw_body:
        raw_body = {**raw_body, "text": raw_body["input"]}
    try:
        payload_model = TtsRequest.model_validate(raw_body)
    except Exception as exc:
        # Pydantic ValidationError → 422
        raise HTTPException(status_code=422, detail=str(exc))

    text = payload_model.text.strip()
    if not text:
        raise HTTPException(status_code=400, detail="payload.text is required")
    if len(text) > TTS_TEXT_MAX_CHARS:
        _jlog("tts_text_too_long", request_id=request_id,
              text_len=len(text), max=TTS_TEXT_MAX_CHARS)
        raise HTTPException(
            status_code=400,
            detail=f"text too long, max {TTS_TEXT_MAX_CHARS} chars",
        )
    raw_steps = int(payload_model.steps)
    steps = max(F5_STEPS_MIN, min(F5_STEPS_MAX, raw_steps))
    if steps != raw_steps:
        _jlog("tts_steps_clamped", request_id=request_id,
              requested=raw_steps, clamped=steps,
              min=F5_STEPS_MIN, max=F5_STEPS_MAX)
    voice_ref = payload_model.voice_ref  # accepted; reserved for multi-voice

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
async def intent(payload: IntentRequest) -> JSONResponse:
    """Forward a user prompt to LiteLLM ``npc-fast`` (MLX 7B Q4 alias).

    Pydantic enforces ``text`` non-empty + optional ``context`` dict; FastAPI
    returns 422 automatically for malformed bodies.
    """
    request_id = str(uuid.uuid4())
    text = payload.text.strip()
    context = payload.context or {}
    if not text:
        raise HTTPException(status_code=400, detail="payload.text is required")

    # P1 part13: prefix every npc-fast call with the Professeur Zacus persona
    # system prompt unless the caller already specified its own. Priority order:
    #   1. caller-supplied ``messages`` array (verbatim, no injection)
    #   2. caller-supplied ``system`` string (used as system role)
    #   3. default NPC_SYSTEM_PROMPT
    # When the caller passes an opaque ``context`` dict (legacy shape), we keep
    # forwarding it as a system message *after* the persona prompt so the LLM
    # sees both the Zacus framing and the per-request scenario hints.
    messages: list[dict[str, str]] = []
    explicit_messages = context.get("messages") if isinstance(context, dict) else None
    if isinstance(explicit_messages, list) and explicit_messages:
        # Caller takes full control — pass through as-is (validate roles loosely).
        for m in explicit_messages:
            if isinstance(m, dict) and "role" in m and "content" in m:
                messages.append({"role": str(m["role"]),
                                 "content": str(m["content"])})
        # Still append the user text if they didn't include it themselves.
        if not any(m.get("role") == "user" for m in messages):
            messages.append({"role": "user", "content": text})
    else:
        explicit_system = (
            context.get("system") if isinstance(context, dict) else None
        )
        system_prompt = (
            str(explicit_system).strip()
            if isinstance(explicit_system, str) and explicit_system.strip()
            else NPC_SYSTEM_PROMPT
        )
        messages.append({"role": "system", "content": system_prompt})
        # Legacy: a non-string context dict (without ``messages``/``system``
        # keys) gets serialized as a secondary system message so existing
        # callers keep working — we just sandwich it after the persona prompt.
        if (
            isinstance(context, dict)
            and context
            and "messages" not in context
            and "system" not in context
        ):
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


# ── /voice/ws (firmware STT streaming, P1 part11) ───────────────────────────
def _wrap_pcm16_as_wav(pcm: bytes, sample_rate: int = WS_EXPECTED_SR) -> bytes:
    """Prepend a 44-byte RIFF/WAVE header to raw PCM16 mono samples.

    whisper.cpp's HTTP server expects a real WAV file; doing the wrap here
    keeps the firmware payload to pure raw PCM (saves bytes on the wire and
    makes the ESP32 codec path simpler).
    """
    n_samples_bytes = len(pcm)
    n_channels = 1
    bits_per_sample = 16
    byte_rate = sample_rate * n_channels * bits_per_sample // 8
    block_align = n_channels * bits_per_sample // 8
    fmt_chunk = struct.pack(
        "<4sIHHIIHH",
        b"fmt ", 16, 1, n_channels, sample_rate,
        byte_rate, block_align, bits_per_sample,
    )
    data_chunk = struct.pack("<4sI", b"data", n_samples_bytes) + pcm
    riff_size = 4 + len(fmt_chunk) + len(data_chunk)
    header = struct.pack("<4sI4s", b"RIFF", riff_size, b"WAVE")
    return header + fmt_chunk + data_chunk


async def _whisper_transcribe_pcm(pcm: bytes) -> str:
    """POST a wrapped WAV to whisper.cpp /inference, return transcript text.

    Raises ``RuntimeError`` on transport / parsing failure so the WS handler
    can decide how to surface the error to the firmware.
    """
    wav = _wrap_pcm16_as_wav(pcm)
    files = {"file": ("clip.wav", wav, "audio/wav")}
    async with httpx.AsyncClient(timeout=30.0) as client:
        resp = await client.post(f"{WHISPER_URL}/inference", files=files)
    if resp.status_code != 200:
        raise RuntimeError(f"whisper {resp.status_code}: {resp.text[:200]}")
    try:
        data = resp.json()
    except json.JSONDecodeError as exc:
        raise RuntimeError(f"whisper non-JSON response: {exc}")
    text = (data.get("text") or "").strip()
    return text


def _wav_to_pcm16(wav_bytes: bytes) -> bytes:
    """Strip the RIFF/WAVE container and return raw PCM16 mono samples.

    The cache stores 24 kHz mono PCM_16 WAVs (see ``_f5_synthesize_sync``)
    so we expect a standard 44-byte RIFF header. To stay tolerant against
    files written with extra chunks (LIST/INFO etc.), we walk the chunks
    and grab the ``data`` payload explicitly rather than blindly skipping
    44 bytes.
    """
    if len(wav_bytes) < 44 or wav_bytes[:4] != b"RIFF" or wav_bytes[8:12] != b"WAVE":
        # Not a WAV — assume already raw PCM and return as-is.
        return wav_bytes
    pos = 12
    while pos + 8 <= len(wav_bytes):
        chunk_id = wav_bytes[pos:pos + 4]
        chunk_size = struct.unpack("<I", wav_bytes[pos + 4:pos + 8])[0]
        if chunk_id == b"data":
            start = pos + 8
            return bytes(wav_bytes[start:start + chunk_size])
        pos += 8 + chunk_size
        # Chunks are padded to even byte boundaries.
        if chunk_size % 2:
            pos += 1
    # No data chunk found — fallback to raw header skip.
    return bytes(wav_bytes[44:])


def _float32_to_pcm16(samples: np.ndarray) -> bytes:
    """Convert float32 [-1.0..1.0] mono numpy array to PCM_16 little-endian."""
    arr = np.clip(samples, -1.0, 1.0)
    pcm = (arr * 32767.0).astype("<i2")
    return pcm.tobytes()


def _f5_synthesize_pcm_sync(text: str, steps: int) -> bytes:
    """Synthesize via F5 and return raw PCM16 mono @ 24 kHz (no WAV header).

    Exists alongside ``_f5_synthesize_sync`` so the WS path can stream raw
    samples without paying the WAV-header round-trip. Mirrors the same MLX
    invocation 1:1 (same prompt, same sampler args) — keep them in sync.
    """
    import mlx.core as mx  # noqa: WPS433
    from f5_tts_mlx.utils import convert_char_to_pinyin

    audio_arr, sr = sf.read(str(_REF_AUDIO_PATH))
    if sr != F5_SAMPLE_RATE:
        raise RuntimeError(
            f"Reference audio sample rate {sr} ≠ {F5_SAMPLE_RATE} Hz"
        )
    audio = mx.array(audio_arr)

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
    pcm_f32 = np.array(wave, dtype=np.float32)
    return _float32_to_pcm16(pcm_f32)


async def _run_f5_pcm(text: str, steps: int) -> bytes:
    assert _F5_LOCK is not None
    async with _F5_LOCK:
        return _f5_synthesize_pcm_sync(text, steps)


async def _intent_complete(text: str) -> Optional[str]:
    """Call LiteLLM npc-fast and return the assistant content, or None on fail.

    Used by the WS handler in opportunistic mode — if the call fails we still
    deliver the transcript and close cleanly (firmware can fall back to its
    local hint/grammar matching).

    P1 part13: also prepends the NPC_SYSTEM_PROMPT so the WS path stays
    persona-aligned and tolerates whisper transliteration drift on the
    Professeur Zacus name (« Zaku » / « Zacusse » → still bound to Zacus).
    """
    messages = [
        {"role": "system", "content": NPC_SYSTEM_PROMPT},
        {"role": "user", "content": text},
    ]
    try:
        async with httpx.AsyncClient(timeout=20.0) as client:
            resp = await client.post(
                f"{LITELLM_URL}/v1/chat/completions",
                headers={"Authorization": f"Bearer {LITELLM_KEY}"},
                json={"model": "npc-fast", "messages": messages},
            )
        if resp.status_code != 200:
            _jlog("ws_intent_upstream_err", status=resp.status_code,
                  body=resp.text[:200])
            return None
        body = resp.json()
        return body.get("choices", [{}])[0].get("message", {}).get("content", "")
    except (httpx.ConnectError, httpx.TimeoutException, httpx.HTTPError) as exc:
        _jlog("ws_intent_backend_down", err=type(exc).__name__)
        return None


@app.websocket("/voice/ws")
async def voice_ws(ws: WebSocket) -> None:
    """STT streaming endpoint for the ESP32 firmware (P1 part11 + C_slice7).

    Protocol (text framing in JSON, payload framing in binary):

        client → server : {"type":"hello","version":1,
                           "sample_rate":16000,"format":"pcm_s16",
                           "session_id":"<mac>"}
        client → server : <binary PCM16 mono frames, any size>...
        client → server : {"type":"end"}
        server → client : {"type":"stt","text":"...","final":true}
        server → client : {"type":"intent","content":"...","model":"npc-fast"}
                          (only if WS_FORWARD_INTENT and the call succeeded)
        server → client : {"type":"speak_start","sample_rate":24000,
                           "format":"pcm_s16","total_estimated_ms":<int>}
        server → client : <binary PCM16 mono frames @ 24 kHz, ≤4096 bytes>...
        server → client : {"type":"speak_end","duration_ms":<int>,
                           "backend":"f5"|"cache"|"none","latency_ms":<int>}
                          (speak_* trio only if WS_FORWARD_TTS and intent_ok)
        server closes   : 1000 normal

    Error close codes:
        1003 unsupported  — bad hello (version/format/sample_rate)
        1009 message too big — buffer would exceed 30 s of audio
        1011 internal     — STT pipeline failure
    """
    await ws.accept()
    request_id = str(uuid.uuid4())
    session_id: Optional[str] = None
    started = time.monotonic()

    # ── 1. Hello handshake ───────────────────────────────────────────────
    try:
        hello_raw = await asyncio.wait_for(ws.receive_text(), timeout=5.0)
    except (asyncio.TimeoutError, WebSocketDisconnect):
        _jlog("ws_hello_timeout", request_id=request_id)
        await ws.close(code=1002, reason="hello timeout")
        return
    try:
        hello = json.loads(hello_raw)
    except json.JSONDecodeError:
        _jlog("ws_hello_bad_json", request_id=request_id)
        await ws.close(code=1003, reason="hello not JSON")
        return

    if (
        hello.get("type") != "hello"
        or int(hello.get("version", 0)) != WS_EXPECTED_VERSION
        or hello.get("format") != WS_EXPECTED_FORMAT
        or int(hello.get("sample_rate", 0)) != WS_EXPECTED_SR
    ):
        _jlog("ws_hello_unsupported", request_id=request_id, hello=hello)
        await ws.close(code=1003, reason="unsupported hello")
        return
    session_id = hello.get("session_id")
    _jlog("ws_hello_ok", request_id=request_id, session_id=session_id)

    # ── 2. Streaming loop: binary frames + text "end" ────────────────────
    buf = bytearray()
    try:
        while True:
            msg = await ws.receive()
            # FastAPI / Starlette receive() returns a dict with either "bytes"
            # or "text" populated; closing handshakes are surfaced as a
            # "websocket.disconnect" type.
            if msg.get("type") == "websocket.disconnect":
                _jlog("ws_client_disconnect_midstream",
                      request_id=request_id, bytes=len(buf))
                return
            if (data := msg.get("bytes")) is not None:
                if len(buf) + len(data) > WS_MAX_BYTES:
                    _jlog("ws_buffer_overflow", request_id=request_id,
                          have=len(buf), max=WS_MAX_BYTES)
                    await ws.close(code=1009, reason="audio > 30 s cap")
                    return
                buf.extend(data)
                continue
            if (text := msg.get("text")) is not None:
                try:
                    ctrl = json.loads(text)
                except json.JSONDecodeError:
                    _jlog("ws_ctrl_bad_json", request_id=request_id)
                    continue
                if ctrl.get("type") == "end":
                    break
                # Unknown control message — log and ignore (forward-compat).
                _jlog("ws_ctrl_unknown", request_id=request_id,
                      ctrl_type=ctrl.get("type"))
    except WebSocketDisconnect:
        _jlog("ws_disconnect", request_id=request_id, bytes=len(buf))
        return

    if not buf:
        _jlog("ws_end_empty", request_id=request_id)
        await ws.close(code=1003, reason="no audio")
        return

    # ── 3. Whisper STT on the buffered PCM ──────────────────────────────
    try:
        transcript = await _whisper_transcribe_pcm(bytes(buf))
    except (RuntimeError, httpx.HTTPError) as exc:
        _jlog("ws_stt_failed", request_id=request_id, err=str(exc))
        # Best-effort: tell the client and close. Firmware can decide.
        try:
            await ws.send_text(json.dumps(
                {"type": "error", "stage": "stt", "detail": str(exc)[:200]}
            ))
        finally:
            await ws.close(code=1011, reason="stt failed")
        return

    stt_ms = int((time.monotonic() - started) * 1000)
    await ws.send_text(json.dumps(
        {"type": "stt", "text": transcript, "final": True}
    ))
    _jlog("ws_stt_done", request_id=request_id, session_id=session_id,
          bytes=len(buf), text_len=len(transcript), latency_ms=stt_ms)

    # ── 4. Optional intent forward (LiteLLM npc-fast) ────────────────────
    intent_content: Optional[str] = None
    if WS_FORWARD_INTENT and transcript:
        intent_started = time.monotonic()
        intent_content = await _intent_complete(transcript)
        if intent_content is not None:
            await ws.send_text(json.dumps(
                {"type": "intent", "content": intent_content, "model": "npc-fast"}
            ))
            _jlog("ws_intent_done", request_id=request_id,
                  reply_len=len(intent_content),
                  latency_ms=int((time.monotonic() - intent_started) * 1000))

    # ── 5. Optional TTS forward (F5 → cache, raw PCM stream) ─────────────
    tts_chunks_sent = 0
    tts_total_bytes = 0
    tts_backend_used: Optional[str] = None
    tts_stage_latency_ms = 0
    if WS_FORWARD_TTS and intent_content:
        tts_started = time.monotonic()
        speak_text = intent_content.strip()
        if not speak_text:
            _jlog("ws_tts_empty_intent", request_id=request_id)
        else:
            # Truncate over-long replies so we never send > 30 s of audio
            # back; the firmware buffer is small and the diffusion latency
            # blows up linearly with text length.
            if len(speak_text) > TTS_TEXT_MAX_CHARS:
                _jlog("ws_tts_truncated", request_id=request_id,
                      had=len(speak_text), max=TTS_TEXT_MAX_CHARS)
                speak_text = speak_text[:TTS_TEXT_MAX_CHARS]
            cache_key = _cache_key(
                speak_text, _voice_ref_token(None), WS_TTS_STEPS
            )
            pcm: Optional[bytes] = None
            tts_err: Optional[str] = None

            # 5a. Cache lookup (mirrors the /tts handler) ─────────────────
            cached_path = _CACHE_INDEX.get(cache_key)
            if cached_path is None:
                candidate = CACHE_DIR / f"{cache_key}.wav"
                if candidate.exists():
                    cached_path = candidate
                    _CACHE_INDEX[cache_key] = candidate
            if cached_path is not None and cached_path.exists():
                try:
                    wav_bytes = cached_path.read_bytes()
                    pcm = _wav_to_pcm16(wav_bytes)
                    tts_backend_used = "cache"
                    _jlog("ws_tts_cache_hit", request_id=request_id,
                          cache_key=cache_key, bytes=len(pcm))
                except OSError as exc:
                    _jlog("ws_tts_cache_read_err",
                          request_id=request_id, err=str(exc))
                    cached_path = None  # fall through to F5

            # 5b. F5 synthesis on miss ────────────────────────────────────
            if pcm is None and _F5_MODEL_OBJ is not None:
                try:
                    pcm = await asyncio.wait_for(
                        _run_f5_pcm(speak_text, WS_TTS_STEPS),
                        timeout=F5_TIMEOUT_S,
                    )
                    tts_backend_used = "f5"
                    # Persist a WAV copy alongside /tts cache entries so the
                    # next request (HTTP or WS) hits the cache.
                    try:
                        buf = io.BytesIO()
                        sf.write(
                            buf,
                            np.frombuffer(pcm, dtype="<i2").astype(np.float32) / 32767.0,
                            F5_SAMPLE_RATE,
                            format="WAV",
                            subtype="PCM_16",
                        )
                        await _write_cache(cache_key, buf.getvalue())
                    except Exception as exc:  # noqa: BLE001
                        _jlog("ws_tts_cache_write_err",
                              request_id=request_id, err=str(exc))
                except asyncio.TimeoutError:
                    tts_err = "timeout"
                    _jlog("ws_tts_f5_timeout", request_id=request_id,
                          timeout_s=F5_TIMEOUT_S)
                except Exception as exc:  # noqa: BLE001
                    tts_err = type(exc).__name__
                    _jlog("ws_tts_f5_error", request_id=request_id,
                          err=tts_err, msg=str(exc))
            elif pcm is None:
                tts_err = "f5_not_loaded"
                _jlog("ws_tts_f5_not_loaded", request_id=request_id,
                      load_err=_F5_LOAD_ERR)

            tts_stage_latency_ms = int(
                (time.monotonic() - tts_started) * 1000
            )

            if pcm is None:
                # Tell the client we failed to synthesize, then send a
                # zero-length speak_end so the receiver state machine still
                # advances cleanly. NEVER fall back to service_down.wav
                # over WS — the client can fetch it via REST if it cares.
                try:
                    await ws.send_text(json.dumps({
                        "type": "error",
                        "stage": "tts",
                        "message": tts_err or "tts unavailable",
                    }))
                    await ws.send_text(json.dumps({
                        "type": "speak_end",
                        "duration_ms": 0,
                        "backend": "none",
                        "latency_ms": tts_stage_latency_ms,
                    }))
                except (WebSocketDisconnect, RuntimeError):
                    pass
                tts_backend_used = "none"
            else:
                # Cap memory: drop trailing samples beyond 30 s of audio.
                if len(pcm) > WS_TTS_MAX_BYTES:
                    _jlog("ws_tts_truncate_audio", request_id=request_id,
                          had=len(pcm), max=WS_TTS_MAX_BYTES)
                    pcm = pcm[:WS_TTS_MAX_BYTES]
                duration_ms = int(
                    len(pcm) / WS_TTS_BYTES_PER_SAMPLE
                    * 1000 / WS_TTS_OUTPUT_SR
                )
                try:
                    await ws.send_text(json.dumps({
                        "type": "speak_start",
                        "sample_rate": WS_TTS_OUTPUT_SR,
                        "format": "pcm_s16",
                        "total_estimated_ms": duration_ms,
                    }))
                    # Stream PCM in fixed-size chunks. Final chunk may be
                    # shorter; that is the firmware's responsibility to
                    # handle (it already does for the inbound STT path).
                    for offset in range(0, len(pcm), WS_TTS_CHUNK_BYTES):
                        chunk = pcm[offset:offset + WS_TTS_CHUNK_BYTES]
                        await ws.send_bytes(chunk)
                        tts_chunks_sent += 1
                        tts_total_bytes += len(chunk)
                    await ws.send_text(json.dumps({
                        "type": "speak_end",
                        "duration_ms": duration_ms,
                        "backend": tts_backend_used or "f5",
                        "latency_ms": tts_stage_latency_ms,
                    }))
                except WebSocketDisconnect:
                    _jlog("ws_tts_client_dropped",
                          request_id=request_id,
                          chunks_sent=tts_chunks_sent,
                          bytes_sent=tts_total_bytes)
                    return

            _jlog("ws_tts_done", request_id=request_id,
                  session_id=session_id,
                  tts_stage_latency_ms=tts_stage_latency_ms,
                  tts_chunks_sent=tts_chunks_sent,
                  tts_total_bytes=tts_total_bytes,
                  tts_backend_used=tts_backend_used or "none",
                  cache_key=cache_key)

    # ── 6. Clean close ──────────────────────────────────────────────────
    try:
        await ws.close(code=1000, reason="done")
    except RuntimeError:
        # Client may have already closed; that's fine.
        pass


if __name__ == "__main__":  # pragma: no cover
    import uvicorn

    uvicorn.run(app, host="0.0.0.0", port=8200)
