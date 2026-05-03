#!/usr/bin/env python3
"""generate_npc_pool.py — Professor Zacus NPC audio pool generator.

Reads npc_phrases.yaml and synthesizes each phrase via either:
- Piper TTS API (legacy backend, MP3 output) — default for backwards compat.
- voice-bridge F5-TTS (P1 part9b backend, WAV output) — preferred for the
  Zacus reference voice.

Both backends write a manifest.json index that maps phrase keys to audio
files. Generators are idempotent: existing files are skipped unless
``--force`` is passed (cf. tools/CLAUDE.md convention).

Usage (Piper, legacy):
    python3 generate_npc_pool.py

Usage (F5 via voice-bridge):
    python3 generate_npc_pool.py \
        --backend f5 \
        --voice-bridge-url http://192.168.0.150:8200 \
        --steps 8

Common flags:
    --output         Output directory (default: hotline_tts)
    --manifest       Path to output manifest JSON
    --phrases        Path to npc_phrases.yaml
    --force          Re-synthesize even if cached file exists
    --dry-run        Print phrase keys without calling TTS
    --delay          Delay between requests (default: 0.3 s)

Backend-specific flags (F5 only):
    --voice-bridge-url   Base URL of voice-bridge daemon
    --steps              F5 inference steps (8 = best quality offline pool)
    --ref-wav            Optional reference WAV path (passed as voice_ref)
"""

from __future__ import annotations

import argparse
import hashlib
import json
import sys
import time
from pathlib import Path
from typing import Generator, Optional

# pyyaml is a soft dependency — error early with a clear message
try:
    import yaml
except ImportError:
    print("ERROR: pyyaml is required. Install with: pip install pyyaml", file=sys.stderr)
    sys.exit(1)

# requests is a soft dependency
try:
    import requests
except ImportError:
    print("ERROR: requests is required. Install with: pip install requests", file=sys.stderr)
    sys.exit(1)


# ---------------------------------------------------------------------------
# YAML traversal helpers
# ---------------------------------------------------------------------------

def _iter_phrases(data: dict, prefix: str = "") -> Generator[tuple[str, str], None, None]:
    """Recursively yield (key, text) pairs from the YAML phrase tree.

    A node is considered a phrase leaf when it is a dict containing a 'text'
    field (and optionally a 'key' field).  Lists are iterated by index.
    """
    if isinstance(data, dict):
        if "text" in data:
            # Leaf phrase node — use the embedded key if present, else prefix
            phrase_key = data.get("key") or prefix
            yield phrase_key, data["text"]
        else:
            for k, v in data.items():
                child_prefix = f"{prefix}.{k}" if prefix else k
                yield from _iter_phrases(v, child_prefix)
    elif isinstance(data, list):
        for idx, item in enumerate(data):
            child_prefix = f"{prefix}.{idx}"
            yield from _iter_phrases(item, child_prefix)


def load_phrases(phrases_path: Path) -> list[tuple[str, str]]:
    """Return a flat list of (key, text) tuples from npc_phrases.yaml."""
    with open(phrases_path, encoding="utf-8") as fh:
        data = yaml.safe_load(fh)
    return list(_iter_phrases(data))


# ---------------------------------------------------------------------------
# Filename / cache key helpers
# ---------------------------------------------------------------------------

def key_to_filename(key: str, ext: str = "mp3") -> str:
    """Convert a dotted phrase key to a safe file name.

    Example: "hints.SCENE_LA_DETECTOR.level_1.0" → "hints__SCENE_LA_DETECTOR__level_1__0.mp3"
    """
    safe = key.replace(".", "__")
    safe = "".join(c if c.isalnum() or c in "_-" else "_" for c in safe)
    return f"{safe}.{ext}"


def f5_cache_key(text: str, ref_hash: str, steps: int) -> str:
    """Compute the SHA-256 cache key used by both pool & voice-bridge.

    Mirrors the voice-bridge cache key format so that pool entries can be
    pre-warmed into the daemon cache (drop the .wav into the cache dir and
    the daemon will hit it on next request).
    """
    payload = f"{text}|{ref_hash}|{steps}".encode("utf-8")
    return hashlib.sha256(payload).hexdigest()[:16]


def voice_ref_token(ref_path: Optional[Path]) -> str:
    """Cache-key token derived from the optional reference path.

    Mirrors the daemon contract (see ``main.py::_voice_ref_token``):
      * When ``ref_path`` is None, the pool gen lets the daemon pick its
        boot-time default reference, so both sides hash the literal
        sentinel ``"default"``.
      * Otherwise both sides hash the path string verbatim — never the
        file content — so the client can compute the cache_key without
        needing the daemon's reference WAV.
    """
    if ref_path is None:
        return "default"
    return hashlib.sha256(str(ref_path).encode("utf-8")).hexdigest()[:16]


# ---------------------------------------------------------------------------
# Piper TTS synthesis (legacy backend)
# ---------------------------------------------------------------------------

def synthesize_piper(text: str, voice: str, tts_url: str, timeout_s: float = 30.0) -> bytes:
    """POST text to OpenAI-compatible Piper API and return raw MP3 bytes.

    openedai-speech on Tower:8001 (OpenAI-compatible):
        POST /v1/audio/speech
        Body: {"model": "tts-1", "voice": "...", "input": "...", "response_format": "mp3"}
        Response: audio/mpeg binary
    """
    url = tts_url.rstrip("/") + "/v1/audio/speech"
    payload = {
        "model": "tts-1",
        "voice": voice,
        "input": text,
        "response_format": "mp3",
    }
    resp = requests.post(url, json=payload, timeout=timeout_s)
    resp.raise_for_status()
    return resp.content


# ---------------------------------------------------------------------------
# voice-bridge F5 synthesis (P1 part9b backend)
# ---------------------------------------------------------------------------

def synthesize_f5(
    text: str,
    voice_bridge_url: str,
    steps: int,
    ref_wav: Optional[str] = None,
    timeout_s: float = 60.0,
) -> tuple[bytes, dict[str, str]]:
    """POST text to voice-bridge /tts and return (WAV bytes, response headers).

    The returned headers include diagnostic info such as ``X-TTS-Backend``
    (``f5``, ``cache``, ``piper_fallback``), ``X-TTS-Cache-Hit`` and
    ``X-TTS-Latency-Ms`` — these get folded into the manifest for traceability.
    """
    url = voice_bridge_url.rstrip("/") + "/tts"
    payload: dict[str, object] = {"text": text, "steps": steps}
    if ref_wav:
        payload["voice_ref"] = ref_wav
    resp = requests.post(url, json=payload, timeout=timeout_s)
    resp.raise_for_status()
    return resp.content, dict(resp.headers)


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Generate audio pool for Professor Zacus NPC phrases via Piper TTS "
            "or the voice-bridge F5 backend."
        )
    )
    repo_root = Path(__file__).resolve().parents[2]

    parser.add_argument(
        "--backend",
        choices=("piper", "f5"),
        default="piper",
        help="TTS backend (default: piper for backwards compat).",
    )

    # Piper-specific
    parser.add_argument(
        "--tts-url",
        default="http://192.168.0.120:8001",
        help="Piper TTS base URL (default: http://192.168.0.120:8001).",
    )
    parser.add_argument(
        "--voice",
        default="zacus",
        help="Piper voice name (default: zacus).",
    )

    # F5-specific
    parser.add_argument(
        "--voice-bridge-url",
        default="http://192.168.0.150:8200",
        help="voice-bridge base URL (default: http://192.168.0.150:8200).",
    )
    parser.add_argument(
        "--steps",
        type=int,
        default=8,
        help="F5 inference steps (default: 8 = best for offline pool).",
    )
    parser.add_argument(
        "--ref-wav",
        type=Path,
        default=None,
        help=(
            "Optional reference WAV passed to voice-bridge as voice_ref. "
            "When absent, voice-bridge uses its boot-time default reference."
        ),
    )

    # Common
    parser.add_argument(
        "--output",
        type=Path,
        default=repo_root / "hotline_tts",
        help="Output directory (default: <repo>/hotline_tts).",
    )
    parser.add_argument(
        "--manifest",
        type=Path,
        default=None,
        help="Path to output manifest JSON (default: <output>/<backend>/manifest.json for f5, <output>/manifest.json for piper).",
    )
    parser.add_argument(
        "--phrases",
        type=Path,
        default=repo_root / "game" / "scenarios" / "npc_phrases.yaml",
        help="Path to npc_phrases.yaml.",
    )
    parser.add_argument(
        "--force",
        action="store_true",
        help="Re-synthesize even if the cached audio file already exists.",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Print phrase keys and texts without calling TTS API.",
    )
    parser.add_argument(
        "--delay",
        type=float,
        default=0.3,
        help="Delay in seconds between TTS requests (default: 0.3).",
    )
    parser.add_argument(
        "--limit",
        type=int,
        default=0,
        help="Process only the first N phrases (0 = all). Useful for smoke tests.",
    )
    return parser.parse_args(argv)


# ---------------------------------------------------------------------------
# Per-backend run loops
# ---------------------------------------------------------------------------

def _run_piper(args: argparse.Namespace, phrases: list[tuple[str, str]]) -> int:
    args.output.mkdir(parents=True, exist_ok=True)
    manifest_path: Path = args.manifest or (args.output / "manifest.json")

    manifest: dict[str, str] = {}
    failed: list[tuple[str, str, str]] = []
    succeeded = 0

    for idx, (key, text) in enumerate(phrases, start=1):
        filename = key_to_filename(key, ext="mp3")
        out_path = args.output / filename
        rel_path = (
            str(out_path.relative_to(args.output.parent))
            if out_path.is_relative_to(args.output.parent)
            else str(out_path)
        )

        print(f"[{idx}/{len(phrases)}] {key} → {filename}", end="", flush=True)

        if out_path.exists() and out_path.stat().st_size > 0 and not args.force:
            print(" (cached)")
            manifest[key] = rel_path
            succeeded += 1
            continue

        try:
            t0 = time.monotonic()
            audio_bytes = synthesize_piper(text, args.voice, args.tts_url)
            out_path.write_bytes(audio_bytes)
            manifest[key] = rel_path
            succeeded += 1
            dt_ms = int((time.monotonic() - t0) * 1000)
            print(f" OK ({len(audio_bytes)} B in {dt_ms} ms)")
        except requests.exceptions.ConnectionError as exc:
            print(f" FAIL (connection error: {exc})")
            failed.append((key, text, f"ConnectionError: {exc}"))
        except requests.exceptions.Timeout:
            print(" FAIL (timeout)")
            failed.append((key, text, "Timeout"))
        except requests.exceptions.HTTPError as exc:
            print(f" FAIL (HTTP {exc.response.status_code}: {exc.response.text[:120]})")
            failed.append((key, text, f"HTTPError: {exc}"))
        except Exception as exc:  # noqa: BLE001
            print(f" FAIL ({type(exc).__name__}: {exc})")
            failed.append((key, text, str(exc)))

        if args.delay > 0:
            time.sleep(args.delay)

    manifest_path.parent.mkdir(parents=True, exist_ok=True)
    with open(manifest_path, "w", encoding="utf-8") as fh:
        json.dump(manifest, fh, ensure_ascii=False, indent=2)
    print(f"\nManifest written: {manifest_path} ({len(manifest)} entries)")
    print(
        f"\nSummary: {succeeded} succeeded, {len(failed)} failed "
        f"out of {len(phrases)} phrases"
    )
    if failed:
        print("\nFailed phrases:")
        for key, text, error in failed:
            print(f"  [{key}] {error}")
            print(f"    text: {text[:80]}{'…' if len(text) > 80 else ''}")
    return 0 if not failed else 1


def _run_f5(args: argparse.Namespace, phrases: list[tuple[str, str]]) -> int:
    f5_dir = args.output / "f5"
    f5_dir.mkdir(parents=True, exist_ok=True)
    manifest_path: Path = args.manifest or (f5_dir / "manifest.json")

    ref_hash = voice_ref_token(args.ref_wav)
    print(
        f"F5 backend → {args.voice_bridge_url} (steps={args.steps}, "
        f"ref_hash={ref_hash})"
    )

    manifest_entries: dict[str, dict[str, object]] = {}
    failed: list[tuple[str, str, str]] = []
    succeeded = 0

    for idx, (key, text) in enumerate(phrases, start=1):
        # Prefer the explicit YAML key for the file name (stable, human-readable);
        # the SHA-256 cache_key is also recorded for daemon-side cache pinning.
        filename = key_to_filename(key, ext="wav")
        out_path = f5_dir / filename
        rel_path = (
            str(out_path.relative_to(args.output.parent))
            if out_path.is_relative_to(args.output.parent)
            else str(out_path)
        )
        cache_key = f5_cache_key(text, ref_hash, args.steps)

        print(
            f"[{idx}/{len(phrases)}] {key} → {filename} (ck={cache_key})",
            end="",
            flush=True,
        )

        if out_path.exists() and out_path.stat().st_size > 0 and not args.force:
            print(" (cached)")
            manifest_entries[key] = {
                "cache_key": cache_key,
                "audio_path": rel_path,
                "text": text,
                "voice_bridge_url": args.voice_bridge_url,
                "steps": args.steps,
                "ref_hash": ref_hash,
                "cached": True,
            }
            succeeded += 1
            continue

        try:
            t0 = time.monotonic()
            wav_bytes, hdrs = synthesize_f5(
                text,
                voice_bridge_url=args.voice_bridge_url,
                steps=args.steps,
                ref_wav=str(args.ref_wav) if args.ref_wav else None,
            )
            out_path.write_bytes(wav_bytes)
            dt_ms = int((time.monotonic() - t0) * 1000)
            backend_used = hdrs.get("x-tts-backend") or hdrs.get("X-TTS-Backend") or "?"
            cache_hit = hdrs.get("x-tts-cache-hit") or hdrs.get("X-TTS-Cache-Hit") or "?"
            srv_lat = hdrs.get("x-tts-latency-ms") or hdrs.get("X-TTS-Latency-Ms") or "?"
            manifest_entries[key] = {
                "cache_key": cache_key,
                "audio_path": rel_path,
                "text": text,
                "voice_bridge_url": args.voice_bridge_url,
                "steps": args.steps,
                "ref_hash": ref_hash,
                "gen_at_ms": int(time.time() * 1000),
                "server_backend": backend_used,
                "server_cache_hit": cache_hit,
                "server_latency_ms": srv_lat,
            }
            succeeded += 1
            print(
                f" OK ({len(wav_bytes) // 1024} kB in {dt_ms} ms, "
                f"backend={backend_used}, hit={cache_hit})"
            )
        except requests.exceptions.ConnectionError as exc:
            print(f" FAIL (connection error: {exc})")
            failed.append((key, text, f"ConnectionError: {exc}"))
            print(
                "\nFATAL: voice-bridge is unreachable; aborting (no point retrying).",
                file=sys.stderr,
            )
            break
        except requests.exceptions.Timeout:
            print(" FAIL (timeout)")
            failed.append((key, text, "Timeout"))
        except requests.exceptions.HTTPError as exc:
            body = exc.response.text[:200] if exc.response is not None else ""
            print(f" FAIL (HTTP {exc.response.status_code}: {body})")
            failed.append((key, text, f"HTTPError: {exc}"))
        except Exception as exc:  # noqa: BLE001
            print(f" FAIL ({type(exc).__name__}: {exc})")
            failed.append((key, text, str(exc)))

        if args.delay > 0:
            time.sleep(args.delay)

    manifest = {
        "backend": "f5",
        "voice_bridge_url": args.voice_bridge_url,
        "steps": args.steps,
        "ref_hash": ref_hash,
        "ref_wav": str(args.ref_wav) if args.ref_wav else None,
        "generated_at_ms": int(time.time() * 1000),
        "entries": manifest_entries,
    }
    manifest_path.parent.mkdir(parents=True, exist_ok=True)
    with open(manifest_path, "w", encoding="utf-8") as fh:
        json.dump(manifest, fh, ensure_ascii=False, indent=2)
    print(f"\nManifest written: {manifest_path} ({len(manifest_entries)} entries)")
    print(
        f"\nSummary: {succeeded} succeeded, {len(failed)} failed "
        f"out of {len(phrases)} phrases"
    )
    if failed:
        print("\nFailed phrases:")
        for key, text, error in failed:
            print(f"  [{key}] {error}")
            print(f"    text: {text[:80]}{'…' if len(text) > 80 else ''}")
    return 0 if not failed else 1


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)

    if not args.phrases.exists():
        print(f"ERROR: phrases file not found: {args.phrases}", file=sys.stderr)
        return 1

    phrases = load_phrases(args.phrases)
    if not phrases:
        print("ERROR: no phrases found in YAML file", file=sys.stderr)
        return 1

    if args.limit > 0:
        phrases = phrases[: args.limit]

    print(
        f"Loaded {len(phrases)} phrases from {args.phrases} "
        f"(backend={args.backend})"
    )

    if args.dry_run:
        print("\n--- DRY RUN --- (no TTS calls)")
        for key, text in phrases:
            print(f"  [{key}] {text[:80]}{'…' if len(text) > 80 else ''}")
        print(f"\nTotal: {len(phrases)} phrases")
        return 0

    if args.backend == "piper":
        return _run_piper(args, phrases)
    return _run_f5(args, phrases)


if __name__ == "__main__":
    sys.exit(main())
