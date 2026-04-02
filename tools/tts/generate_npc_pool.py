#!/usr/bin/env python3
"""generate_npc_pool.py — Professor Zacus NPC MP3 pool generator.

Reads npc_phrases.yaml and synthesizes each phrase via Piper TTS API,
then writes MP3 files under --output directory and a manifest.json index.

Usage:
    python3 generate_npc_pool.py [options]

    --tts-url    Piper TTS base URL (default: http://192.168.0.120:8001)
    --voice      TTS voice name (default: tom-medium)
    --output     Output directory for MP3 files (default: hotline_tts)
    --manifest   Path to output manifest JSON (default: hotline_tts/manifest.json)
    --phrases    Path to npc_phrases.yaml (default: game/scenarios/npc_phrases.yaml)
    --dry-run    Print keys without calling TTS API
"""

from __future__ import annotations

import argparse
import json
import sys
import time
from pathlib import Path
from typing import Generator

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
# Key → safe file name
# ---------------------------------------------------------------------------

def key_to_filename(key: str) -> str:
    """Convert a dotted phrase key to a safe file name.

    Example: "hints.SCENE_LA_DETECTOR.level_1.0" → "hints__SCENE_LA_DETECTOR__level_1__0.mp3"
    """
    safe = key.replace(".", "__")
    # Remove any chars outside [a-zA-Z0-9_-]
    safe = "".join(c if c.isalnum() or c in "_-" else "_" for c in safe)
    return f"{safe}.mp3"


# ---------------------------------------------------------------------------
# TTS synthesis
# ---------------------------------------------------------------------------

def synthesize(text: str, voice: str, tts_url: str, timeout_s: float = 10.0) -> bytes:
    """POST text to Piper TTS API and return raw audio bytes (MP3 or WAV).

    Piper TTS API (Tower:8001):
        POST /api/tts
        Body: {"text": "...", "voice": "..."}
        Response: audio/mpeg or audio/wav binary

    Raises requests.HTTPError on non-2xx responses.
    """
    url = tts_url.rstrip("/") + "/api/tts"
    payload = {"text": text, "voice": voice}
    resp = requests.post(url, json=payload, timeout=timeout_s)
    resp.raise_for_status()
    return resp.content


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Generate MP3 pool for Professor Zacus NPC phrases via Piper TTS."
    )
    repo_root = Path(__file__).resolve().parents[2]
    parser.add_argument(
        "--tts-url",
        default="http://192.168.0.120:8001",
        help="Piper TTS base URL (default: http://192.168.0.120:8001)",
    )
    parser.add_argument(
        "--voice",
        default="tom-medium",
        help="TTS voice name (default: tom-medium)",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=repo_root / "hotline_tts",
        help="Output directory for MP3 files (default: <repo>/hotline_tts)",
    )
    parser.add_argument(
        "--manifest",
        type=Path,
        default=None,
        help="Path to output manifest JSON (default: <output>/manifest.json)",
    )
    parser.add_argument(
        "--phrases",
        type=Path,
        default=repo_root / "game" / "scenarios" / "npc_phrases.yaml",
        help="Path to npc_phrases.yaml",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Print phrase keys and texts without calling TTS API",
    )
    parser.add_argument(
        "--delay",
        type=float,
        default=0.3,
        help="Delay in seconds between TTS requests (default: 0.3)",
    )
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)

    # Resolve manifest path default
    manifest_path: Path = args.manifest or (args.output / "manifest.json")

    # Validate inputs
    if not args.phrases.exists():
        print(f"ERROR: phrases file not found: {args.phrases}", file=sys.stderr)
        return 1

    phrases = load_phrases(args.phrases)
    if not phrases:
        print("ERROR: no phrases found in YAML file", file=sys.stderr)
        return 1

    print(f"Loaded {len(phrases)} phrases from {args.phrases}")

    if args.dry_run:
        print("\n--- DRY RUN --- (no TTS calls)")
        for key, text in phrases:
            print(f"  [{key}] {text[:80]}{'…' if len(text) > 80 else ''}")
        print(f"\nTotal: {len(phrases)} phrases")
        return 0

    # Create output directory
    args.output.mkdir(parents=True, exist_ok=True)

    manifest: dict[str, str] = {}
    failed: list[tuple[str, str, str]] = []  # (key, text, error)
    succeeded = 0

    for idx, (key, text) in enumerate(phrases, start=1):
        filename = key_to_filename(key)
        out_path = args.output / filename
        rel_path = str(out_path.relative_to(args.output.parent)
                       if out_path.is_relative_to(args.output.parent)
                       else out_path)

        print(f"[{idx}/{len(phrases)}] {key} → {filename}", end="", flush=True)

        # Skip if already generated (idempotent re-run)
        if out_path.exists() and out_path.stat().st_size > 0:
            print(" (cached)")
            manifest[key] = rel_path
            succeeded += 1
            continue

        try:
            audio_bytes = synthesize(text, args.voice, args.tts_url)
            out_path.write_bytes(audio_bytes)
            manifest[key] = rel_path
            succeeded += 1
            print(f" OK ({len(audio_bytes)} bytes)")
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

    # Write manifest
    manifest_path.parent.mkdir(parents=True, exist_ok=True)
    with open(manifest_path, "w", encoding="utf-8") as fh:
        json.dump(manifest, fh, ensure_ascii=False, indent=2)
    print(f"\nManifest written: {manifest_path} ({len(manifest)} entries)")

    # Summary
    print(f"\nSummary: {succeeded} succeeded, {len(failed)} failed out of {len(phrases)} phrases")
    if failed:
        print("\nFailed phrases:")
        for key, text, error in failed:
            print(f"  [{key}] {error}")
            print(f"    text: {text[:80]}{'…' if len(text) > 80 else ''}")

    return 0 if not failed else 1


if __name__ == "__main__":
    sys.exit(main())
