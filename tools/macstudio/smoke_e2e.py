#!/usr/bin/env python3
"""voice-bridge end-to-end smoke test (P5 tools).

Exercises the full voice loop (whisper STT → npc-fast intent → F5 TTS) against
the live voice-bridge daemon on MacStudio (`:8200`) without needing the ESP32
firmware. Three modes:

* ``tts``  — POST ``/tts``, save WAV, mesure la latence.
* ``ws``   — WebSocket round-trip on ``/voice/ws`` (hello → audio chunks → end
              → stt + intent + speak_start + binary frames + speak_end).
* ``full`` — readiness probe + cache stats + tts + ws + final cache stats +
             rapport synthétique.

Usage (local, voice-bridge réachable via Tailscale)::

    uv run --with websockets --with httpx \
      python tools/macstudio/smoke_e2e.py --mode full

Exit codes
----------
* ``0`` — all probes succeeded.
* ``1`` — one or more probes returned an error (bad transcript, TTS failure,
          unexpected status code, etc.).
* ``2`` — voice-bridge unreachable (likely no Tailscale / firewall) — the test
          could not run at all. Useful in CI to "auto-skip" gracefully.

The script is intentionally dependency-light (`httpx`, `websockets`); ANSI
colours are inlined so we don't depend on ``rich`` (the Makefile target adds
``rich`` opportunistically but the script never imports it).
"""
from __future__ import annotations

import argparse
import asyncio
import json
import os
import shutil
import struct
import subprocess
import sys
import time
import uuid
import wave
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Optional

import httpx
import websockets

# ── ANSI colours (no rich dep) ──────────────────────────────────────────────
_USE_COLOR = sys.stdout.isatty() and os.getenv("NO_COLOR") is None
_GREEN = "\033[32m" if _USE_COLOR else ""
_RED = "\033[31m" if _USE_COLOR else ""
_YELLOW = "\033[33m" if _USE_COLOR else ""
_CYAN = "\033[36m" if _USE_COLOR else ""
_BOLD = "\033[1m" if _USE_COLOR else ""
_RESET = "\033[0m" if _USE_COLOR else ""


def _log(level: str, event: str, **fields: Any) -> None:
    """Single-line JSON log + colour-tagged human prefix on stderr."""
    tag = {
        "ok": f"{_GREEN}[OK]{_RESET}",
        "fail": f"{_RED}[FAIL]{_RESET}",
        "warn": f"{_YELLOW}[WARN]{_RESET}",
        "info": f"{_CYAN}[INFO]{_RESET}",
    }.get(level, f"[{level.upper()}]")
    payload = {"level": level, "event": event, **fields}
    print(f"{tag} {event} {json.dumps(fields, ensure_ascii=False)}",
          file=sys.stderr, flush=True)
    # Also append the structured form to a rolling jsonl for grepping.
    log_path = Path(os.getenv("ZACUS_SMOKE_LOG", "/tmp/zacus_smoke.jsonl"))
    try:
        with log_path.open("a") as fh:
            fh.write(json.dumps(payload, ensure_ascii=False) + "\n")
    except OSError:
        pass


# ── tiny PCM helpers (zero soundfile dep) ───────────────────────────────────
def _wrap_pcm16_as_wav(pcm: bytes, sample_rate: int) -> bytes:
    """Prepend a 44-byte RIFF/WAVE header to raw PCM16 mono samples."""
    n_bytes = len(pcm)
    n_channels = 1
    bps = 16
    byte_rate = sample_rate * n_channels * bps // 8
    block_align = n_channels * bps // 8
    fmt = struct.pack(
        "<4sIHHIIHH",
        b"fmt ", 16, 1, n_channels, sample_rate, byte_rate, block_align, bps,
    )
    data = struct.pack("<4sI", b"data", n_bytes) + pcm
    riff = struct.pack("<4sI4s", b"RIFF", 4 + len(fmt) + len(data), b"WAVE")
    return riff + fmt + data


def _read_wav_pcm16(path: Path) -> tuple[bytes, int, int]:
    """Read a WAV file and return (raw PCM16 bytes, sample_rate, channels)."""
    with wave.open(str(path), "rb") as wf:
        sr = wf.getframerate()
        ch = wf.getnchannels()
        sw = wf.getsampwidth()
        if sw != 2:
            raise RuntimeError(
                f"{path}: sample width {sw} bytes (need PCM16)")
        frames = wf.readframes(wf.getnframes())
    return frames, sr, ch


# ── audio fixture (FR via `say -v Thomas` + afconvert to PCM16@16k mono) ────
def ensure_french_fixture(out_path: Path,
                          text: str = "Bonjour Professeur Zacus") -> Path:
    """Generate a 16 kHz mono PCM16 WAV of `text` if `out_path` doesn't exist.

    Uses macOS ``say`` + ``afconvert`` which are part of the base OS — no brew
    install needed. Returns the path to the WAV.
    """
    if out_path.exists():
        return out_path
    say_bin = shutil.which("say")
    afconvert_bin = shutil.which("afconvert")
    if not say_bin or not afconvert_bin:
        raise RuntimeError(
            "Cannot generate French audio fixture: 'say' or 'afconvert' "
            "missing (need macOS host)")
    aiff = out_path.with_suffix(".aiff")
    subprocess.run(
        [say_bin, "-v", "Thomas", "-o", str(aiff), text],
        check=True,
    )
    subprocess.run(
        [
            afconvert_bin,
            "-f", "WAVE",
            "-d", "LEI16@16000",
            "-c", "1",
            str(aiff),
            str(out_path),
        ],
        check=True,
    )
    try:
        aiff.unlink()
    except OSError:
        pass
    return out_path


# ── result dataclasses ──────────────────────────────────────────────────────
@dataclass
class TtsResult:
    ok: bool
    status: int
    latency_ms: int
    backend: Optional[str]
    cache_hit: Optional[str]
    bytes_out: int
    saved_to: Optional[str]
    error: Optional[str] = None


@dataclass
class WsResult:
    ok: bool
    stt_text: Optional[str] = None
    intent_content: Optional[str] = None
    intent_model: Optional[str] = None
    speak_started: bool = False
    speak_backend: Optional[str] = None
    speak_duration_ms: int = 0
    speak_latency_ms: int = 0
    audio_bytes: int = 0
    saved_to: Optional[str] = None
    total_ms: int = 0
    error: Optional[str] = None


@dataclass
class Report:
    voice_bridge_url: str
    ready: Optional[dict[str, Any]] = None
    cache_stats_initial: Optional[dict[str, Any]] = None
    cache_stats_final: Optional[dict[str, Any]] = None
    tts: Optional[TtsResult] = None
    ws: Optional[WsResult] = None
    notes: list[str] = field(default_factory=list)

    def to_dict(self) -> dict[str, Any]:
        out: dict[str, Any] = {
            "voice_bridge_url": self.voice_bridge_url,
            "ready": self.ready,
            "cache_stats_initial": self.cache_stats_initial,
            "cache_stats_final": self.cache_stats_final,
            "notes": self.notes,
        }
        if self.tts is not None:
            out["tts"] = self.tts.__dict__
        if self.ws is not None:
            out["ws"] = self.ws.__dict__
        return out


# ── HTTP probes (health, cache stats, /tts) ─────────────────────────────────
async def probe_ready(base_url: str, *, timeout_s: float = 30.0,
                      poll_s: float = 1.5) -> Optional[dict[str, Any]]:
    """Poll /health/ready until 200 (or until timeout)."""
    deadline = time.monotonic() + timeout_s
    last: Optional[dict[str, Any]] = None
    async with httpx.AsyncClient(timeout=5.0) as client:
        while time.monotonic() < deadline:
            try:
                resp = await client.get(f"{base_url}/health/ready")
                last = resp.json() if resp.headers.get("content-type", "")\
                    .startswith("application/json") else {"raw": resp.text}
                last["_status"] = resp.status_code
                if resp.status_code == 200 and last.get("ready"):
                    _log("ok", "ready", **last)
                    return last
                _log("warn", "ready_pending",
                     status=resp.status_code, body=last)
            except (httpx.ConnectError, httpx.TimeoutException) as exc:
                _log("warn", "ready_unreachable", err=type(exc).__name__,
                     msg=str(exc))
                last = {"_status": 0, "error": type(exc).__name__}
            await asyncio.sleep(poll_s)
    _log("fail", "ready_timeout", last=last)
    return last


async def probe_cache_stats(base_url: str) -> Optional[dict[str, Any]]:
    try:
        async with httpx.AsyncClient(timeout=5.0) as client:
            resp = await client.get(f"{base_url}/tts/cache/stats")
        if resp.status_code != 200:
            _log("warn", "cache_stats_nonok", status=resp.status_code)
            return None
        body = resp.json()
        _log("info", "cache_stats", **body)
        return body
    except (httpx.ConnectError, httpx.TimeoutException) as exc:
        _log("warn", "cache_stats_unreachable", err=type(exc).__name__)
        return None


async def run_tts(base_url: str, text: str, save_to: Path,
                  *, timeout_s: float = 60.0) -> TtsResult:
    """POST /tts with the given text, save the WAV body, return latency."""
    started = time.monotonic()
    try:
        async with httpx.AsyncClient(timeout=timeout_s) as client:
            resp = await client.post(
                f"{base_url}/tts",
                json={"text": text},
            )
        latency_ms = int((time.monotonic() - started) * 1000)
        if resp.status_code != 200:
            _log("fail", "tts_nonok",
                 status=resp.status_code, body=resp.text[:300])
            return TtsResult(
                ok=False, status=resp.status_code, latency_ms=latency_ms,
                backend=None, cache_hit=None, bytes_out=0,
                saved_to=None,
                error=f"HTTP {resp.status_code}: {resp.text[:200]}",
            )
        save_to.parent.mkdir(parents=True, exist_ok=True)
        save_to.write_bytes(resp.content)
        backend = resp.headers.get("X-TTS-Backend")
        cache_hit = resp.headers.get("X-TTS-Cache-Hit")
        result = TtsResult(
            ok=True, status=200, latency_ms=latency_ms,
            backend=backend, cache_hit=cache_hit,
            bytes_out=len(resp.content), saved_to=str(save_to),
        )
        _log("ok", "tts_done", latency_ms=latency_ms,
             backend=backend, cache_hit=cache_hit,
             bytes=len(resp.content), saved=str(save_to))
        return result
    except (httpx.ConnectError, httpx.TimeoutException) as exc:
        latency_ms = int((time.monotonic() - started) * 1000)
        _log("fail", "tts_unreachable", err=type(exc).__name__,
             msg=str(exc))
        return TtsResult(
            ok=False, status=0, latency_ms=latency_ms,
            backend=None, cache_hit=None, bytes_out=0,
            saved_to=None, error=type(exc).__name__,
        )


# ── WebSocket round-trip ────────────────────────────────────────────────────
def _http_to_ws_url(base_url: str) -> str:
    """Map http(s)://host[:port] → ws(s)://host[:port]/voice/ws."""
    if base_url.startswith("https://"):
        return "wss://" + base_url[len("https://"):].rstrip("/") + "/voice/ws"
    if base_url.startswith("http://"):
        return "ws://" + base_url[len("http://"):].rstrip("/") + "/voice/ws"
    # bare host:port → assume ws
    return "ws://" + base_url.rstrip("/") + "/voice/ws"


async def run_ws(base_url: str, audio_path: Path, save_to: Path,
                 *, timeout_s: float = 90.0,
                 chunk_bytes: int = 4096) -> WsResult:
    """Connect /voice/ws, stream the WAV's PCM16 payload, collect responses."""
    pcm, sr, ch = _read_wav_pcm16(audio_path)
    if sr != 16000 or ch != 1:
        # voice-bridge expects 16 kHz mono. Fail loud, don't silently mismatch.
        return WsResult(
            ok=False,
            error=(
                f"audio fixture wrong format: {sr} Hz / {ch} ch "
                "(need 16000 Hz mono). Regenerate via ensure_french_fixture()."
            ),
        )

    ws_url = _http_to_ws_url(base_url)
    session_id = f"smoke-{uuid.uuid4().hex[:8]}"
    started = time.monotonic()
    rx_pcm = bytearray()
    speak_sr: Optional[int] = None
    result = WsResult(ok=False)

    closed_cleanly = False
    try:
        async with websockets.connect(
            ws_url, max_size=8 * 1024 * 1024, open_timeout=10.0,
        ) as ws:
            # Hello.
            await ws.send(json.dumps({
                "type": "hello",
                "version": 1,
                "sample_rate": 16000,
                "format": "pcm_s16",
                "session_id": session_id,
            }))
            _log("info", "ws_hello_sent", session_id=session_id,
                 audio_bytes=len(pcm), audio_ms=int(len(pcm) / 2 / 16000 * 1000))

            # Stream PCM in fixed-size chunks (mimics firmware).
            for offset in range(0, len(pcm), chunk_bytes):
                await ws.send(pcm[offset:offset + chunk_bytes])

            await ws.send(json.dumps({"type": "end"}))
            _log("info", "ws_end_sent")

            # Receive loop until the server closes (1000) or we hit deadline.
            deadline = time.monotonic() + timeout_s
            while time.monotonic() < deadline:
                try:
                    msg = await asyncio.wait_for(
                        ws.recv(),
                        timeout=max(1.0, deadline - time.monotonic()),
                    )
                except asyncio.TimeoutError:
                    result.error = "receive timeout"
                    _log("fail", "ws_recv_timeout")
                    break
                except websockets.exceptions.ConnectionClosedOK:
                    closed_cleanly = True
                    _log("info", "ws_closed_ok")
                    break
                except websockets.exceptions.ConnectionClosed as exc:
                    # Non-1000 close. Treat as transport error unless we
                    # already saw speak_end (the server may close after the
                    # last frame has been ack'd).
                    if result.speak_backend is not None:
                        closed_cleanly = True
                        _log("info", "ws_closed_post_speak",
                             code=getattr(exc, "code", None),
                             reason=getattr(exc, "reason", None))
                    else:
                        result.error = (
                            f"unexpected close code={getattr(exc, 'code', '?')} "
                            f"reason={getattr(exc, 'reason', '?')}"
                        )
                        _log("fail", "ws_unexpected_close",
                             code=getattr(exc, "code", None),
                             reason=getattr(exc, "reason", None))
                    break

                if isinstance(msg, (bytes, bytearray)):
                    rx_pcm.extend(msg)
                    # Log every ~200 KB for visibility, otherwise stay silent
                    # to keep stdout readable on large TTS replies.
                    if len(rx_pcm) % (200 * 1024) < len(msg):
                        _log("info", "ws_audio_chunk_progress",
                             total_bytes=len(rx_pcm))
                    continue

                # Text frame.
                try:
                    payload = json.loads(msg)
                except json.JSONDecodeError:
                    _log("warn", "ws_bad_text", msg=msg[:200])
                    continue
                kind = payload.get("type")
                if kind == "stt":
                    result.stt_text = payload.get("text", "")
                    _log("ok", "ws_stt", text=result.stt_text)
                elif kind == "intent":
                    result.intent_content = payload.get("content", "")
                    result.intent_model = payload.get("model")
                    _log("ok", "ws_intent",
                         model=result.intent_model,
                         reply_len=len(result.intent_content or ""),
                         preview=(result.intent_content or "")[:120])
                elif kind == "speak_start":
                    result.speak_started = True
                    speak_sr = int(payload.get("sample_rate", 24000))
                    _log("info", "ws_speak_start",
                         sample_rate=speak_sr,
                         total_estimated_ms=payload.get("total_estimated_ms"))
                elif kind == "speak_end":
                    result.speak_backend = payload.get("backend")
                    result.speak_duration_ms = int(
                        payload.get("duration_ms", 0))
                    result.speak_latency_ms = int(
                        payload.get("latency_ms", 0))
                    _log("ok", "ws_speak_end",
                         backend=result.speak_backend,
                         duration_ms=result.speak_duration_ms,
                         latency_ms=result.speak_latency_ms)
                elif kind == "error":
                    result.error = (
                        f"server error stage={payload.get('stage')} "
                        f"detail={payload.get('detail') or payload.get('message')}"
                    )
                    _log("fail", "ws_server_error", **payload)
                else:
                    _log("warn", "ws_unknown_text", **payload)
            else:
                result.error = result.error or "deadline reached"
    except websockets.exceptions.ConnectionClosedOK:
        # Race: connect context exits via close → recv exception bubbled here.
        closed_cleanly = True
        _log("info", "ws_closed_ok_outer")
    except (OSError, websockets.exceptions.WebSocketException) as exc:
        result.error = f"ws transport: {type(exc).__name__}: {exc}"
        _log("fail", "ws_transport_failed", err=type(exc).__name__,
             msg=str(exc))
        return result

    result.audio_bytes = len(rx_pcm)
    result.total_ms = int((time.monotonic() - started) * 1000)

    if rx_pcm:
        save_to.parent.mkdir(parents=True, exist_ok=True)
        wav_bytes = _wrap_pcm16_as_wav(bytes(rx_pcm), speak_sr or 24000)
        save_to.write_bytes(wav_bytes)
        result.saved_to = str(save_to)

    # OK if STT happened and either intent/speak round-trip ran cleanly OR
    # the server disabled it (in which case there's no error).
    result.ok = (
        result.stt_text is not None
        and result.error is None
    )
    if result.ok:
        _log("ok", "ws_round_trip_done",
             total_ms=result.total_ms,
             stt_text=result.stt_text,
             audio_bytes=result.audio_bytes,
             saved=result.saved_to)
    else:
        _log("fail", "ws_round_trip_incomplete",
             error=result.error,
             stt_text=result.stt_text,
             intent=result.intent_content,
             total_ms=result.total_ms)
    return result


# ── orchestration ───────────────────────────────────────────────────────────
async def main_async(args: argparse.Namespace) -> int:
    base_url = args.voice_bridge_url.rstrip("/")
    report = Report(voice_bridge_url=base_url)

    # Reachability probe (fast fail with exit code 2 if unreachable).
    try:
        async with httpx.AsyncClient(timeout=5.0) as client:
            await client.get(f"{base_url}/health")
    except (httpx.ConnectError, httpx.TimeoutException) as exc:
        _log("fail", "voice_bridge_unreachable",
             url=base_url, err=type(exc).__name__, msg=str(exc),
             hint=("vérifie Tailscale (`tailscale status`) "
                   "ou utilise --voice-bridge-url http://studio:8200"))
        report.notes.append(f"unreachable: {type(exc).__name__}: {exc}")
        Path(args.report).write_text(
            json.dumps(report.to_dict(), ensure_ascii=False, indent=2))
        return 2

    # Audio fixture (only needed for ws / full).
    audio_path = Path(args.audio)
    if args.mode in {"ws", "full"} and not audio_path.exists():
        _log("info", "generating_audio_fixture",
             out=str(audio_path), text=args.fixture_text)
        try:
            ensure_french_fixture(audio_path, text=args.fixture_text)
        except Exception as exc:  # noqa: BLE001
            _log("fail", "audio_fixture_failed",
                 err=type(exc).__name__, msg=str(exc))
            report.notes.append(f"audio fixture failed: {exc}")
            Path(args.report).write_text(
                json.dumps(report.to_dict(), ensure_ascii=False, indent=2))
            return 1

    rc = 0

    if args.mode == "tts":
        report.tts = await run_tts(
            base_url, args.text, Path(args.save_output),
            timeout_s=args.timeout,
        )
        rc = 0 if report.tts.ok else 1
    elif args.mode == "ws":
        report.ws = await run_ws(
            base_url, audio_path, Path(args.save_output),
            timeout_s=args.timeout,
        )
        rc = 0 if report.ws.ok else 1
    elif args.mode == "full":
        report.ready = await probe_ready(base_url, timeout_s=30.0)
        if report.ready is None or not report.ready.get("ready"):
            _log("fail", "abort_full_mode_not_ready",
                 ready=report.ready)
            rc = 1
        else:
            report.cache_stats_initial = await probe_cache_stats(base_url)
            tts_path = Path(args.save_output).with_name(
                Path(args.save_output).stem + "_tts.wav")
            ws_path = Path(args.save_output).with_name(
                Path(args.save_output).stem + "_ws.wav")
            report.tts = await run_tts(
                base_url, args.text, tts_path,
                timeout_s=args.timeout,
            )
            report.ws = await run_ws(
                base_url, audio_path, ws_path,
                timeout_s=args.timeout,
            )
            report.cache_stats_final = await probe_cache_stats(base_url)

            # Sanity-check cache delta.
            if (report.cache_stats_initial and report.cache_stats_final):
                d_misses = (report.cache_stats_final["misses"]
                            - report.cache_stats_initial["misses"])
                d_hits = (report.cache_stats_final["hits"]
                          - report.cache_stats_initial["hits"])
                _log("info", "cache_delta",
                     misses_added=d_misses, hits_added=d_hits)
                report.notes.append(
                    f"cache delta: +{d_hits} hits, +{d_misses} misses")

            rc = 0
            if not (report.tts and report.tts.ok):
                rc = 1
            if not (report.ws and report.ws.ok):
                rc = 1

    # Summary banner.
    print()
    print(f"{_BOLD}=== voice-bridge smoke report ==={_RESET}")
    print(f"  url            : {report.voice_bridge_url}")
    if report.ready:
        print(f"  ready          : {report.ready.get('ready')} "
              f"(warmup_ms={report.ready.get('warmup_ms')})")
    if report.tts:
        col = _GREEN if report.tts.ok else _RED
        print(f"  /tts           : {col}{report.tts.ok}{_RESET} "
              f"latency={report.tts.latency_ms} ms "
              f"backend={report.tts.backend} "
              f"cache_hit={report.tts.cache_hit} "
              f"bytes={report.tts.bytes_out}")
        if report.tts.saved_to:
            print(f"                   saved → {report.tts.saved_to}")
    if report.ws:
        col = _GREEN if report.ws.ok else _RED
        print(f"  /voice/ws      : {col}{report.ws.ok}{_RESET} "
              f"total={report.ws.total_ms} ms "
              f"speak_backend={report.ws.speak_backend} "
              f"speak_latency={report.ws.speak_latency_ms} ms "
              f"audio_bytes={report.ws.audio_bytes}")
        if report.ws.stt_text is not None:
            print(f"    stt          : {report.ws.stt_text!r}")
        if report.ws.intent_content is not None:
            preview = report.ws.intent_content.replace("\n", " ")[:160]
            print(f"    intent       : {preview!r}")
        if report.ws.saved_to:
            print(f"                   saved → {report.ws.saved_to}")
    if report.cache_stats_final:
        print(f"  cache (final)  : count={report.cache_stats_final['count']} "
              f"hits={report.cache_stats_final['hits']} "
              f"misses={report.cache_stats_final['misses']} "
              f"hit_rate={report.cache_stats_final['hit_rate_since_boot']}")
    print(f"  exit           : {rc}")
    print()

    Path(args.report).write_text(
        json.dumps(report.to_dict(), ensure_ascii=False, indent=2))
    _log("info", "report_written", path=args.report, rc=rc)
    return rc


def parse_args(argv: Optional[list[str]] = None) -> argparse.Namespace:
    p = argparse.ArgumentParser(
        prog="smoke_e2e.py",
        description="voice-bridge end-to-end smoke test (P5 tools)",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    p.add_argument("--voice-bridge-url", default="http://100.116.92.12:8200",
                   help="Base URL of the voice-bridge daemon")
    p.add_argument("--mode", choices=["tts", "ws", "full"], default="full",
                   help="tts → /tts only, ws → /voice/ws only, full → both + probes")
    p.add_argument("--text", default="Bonjour Professeur Zacus, où est la clé ?",
                   help="Texte envoyé à /tts (mode tts ou full)")
    p.add_argument("--audio", default="/tmp/zacus_smoke_in.wav",
                   help="Fichier audio source pour /voice/ws (PCM16 16 kHz mono); "
                        "généré via `say -v Thomas` si absent")
    p.add_argument("--fixture-text",
                   default="Bonjour Professeur Zacus, où est la clé ?",
                   help="Texte synthétisé localement quand on doit créer "
                        "le fichier --audio")
    p.add_argument("--save-output", default="/tmp/zacus_smoke_out.wav",
                   help="Chemin de sortie pour les WAV reçus (suffixé _tts/_ws "
                        "en mode full)")
    p.add_argument("--report", default="/tmp/zacus_smoke_report.json",
                   help="Chemin JSON du rapport synthétique")
    p.add_argument("--timeout", type=float, default=60.0,
                   help="Timeout (s) par requête (POST /tts ou /voice/ws)")
    return p.parse_args(argv)


def main() -> int:
    args = parse_args()
    return asyncio.run(main_async(args))


if __name__ == "__main__":
    sys.exit(main())
