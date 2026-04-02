#!/usr/bin/env python3
"""
generate_xtts_pool.py — Pre-generate all NPC phrases via XTTS-v2 (GPU voice clone).

Reads game/scenarios/npc_phrases.yaml, calls XTTS-v2 on kxkm-ai, saves WAV files
to hotline_tts/ directory, updates hotline_tts/manifest.json.

Idempotent: skips already-generated files unless --force is passed.

Usage:
    python3 tools/tts/generate_xtts_pool.py [--dry-run] [--force] [--host kxkm-ai:5002]

Requirements:
    pip install pyyaml requests
"""
import argparse
import json
import pathlib
import sys
import time
from typing import Any

import requests
import yaml

# Paths relative to project root (run from repo root)
PHRASES_YAML = pathlib.Path("game/scenarios/npc_phrases.yaml")
OUTPUT_DIR   = pathlib.Path("hotline_tts")
MANIFEST     = OUTPUT_DIR / "manifest.json"
DEFAULT_HOST = "kxkm-ai:5002"

# HTTP request settings
REQUEST_TIMEOUT_S  = 30
RETRY_COUNT        = 2
RETRY_DELAY_S      = 3


def flatten_phrases(data: dict[str, Any], prefix: str = "") -> list[dict[str, str]]:
    """Recursively extract all phrase entries with their keys and text.

    Handles both list-of-dicts (with 'text' + 'key') and nested dicts.
    Returns list of {"key": str, "text": str} dicts.
    """
    phrases: list[dict[str, str]] = []
    for k, v in data.items():
        path = f"{prefix}.{k}" if prefix else k
        if isinstance(v, list):
            for i, item in enumerate(v):
                if isinstance(item, dict) and "text" in item:
                    phrases.append({
                        "key":  item.get("key", f"{path}.{i}"),
                        "text": item["text"],
                    })
        elif isinstance(v, dict):
            phrases.extend(flatten_phrases(v, path))
    return phrases


def key_to_path(key: str) -> pathlib.Path:
    """Convert phrase key to output file path.

    Example: hints.P1_SON.level_1.0 → hotline_tts/hints/P1_SON/level_1/0.wav
    """
    parts = key.replace(".", "/")
    return OUTPUT_DIR / f"{parts}.wav"


def synthesize(text: str, host: str) -> bytes:
    """Call XTTS-v2 /v1/audio/speech endpoint, return raw WAV bytes.

    Raises requests.HTTPError on non-2xx response.
    """
    url = f"http://{host}/v1/audio/speech"
    resp = requests.post(
        url,
        json={"input": text},
        timeout=REQUEST_TIMEOUT_S,
    )
    resp.raise_for_status()
    return resp.content


def check_xtts_health(host: str) -> bool:
    """Return True if XTTS-v2 service is reachable and healthy."""
    try:
        resp = requests.get(f"http://{host}/health", timeout=5)
        return resp.status_code == 200
    except Exception:
        return False


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Pre-generate NPC TTS pool via XTTS-v2"
    )
    parser.add_argument("--dry-run", action="store_true",
                        help="Parse phrases and print plan without calling XTTS")
    parser.add_argument("--force", action="store_true",
                        help="Re-generate files that already exist")
    parser.add_argument("--host", default=DEFAULT_HOST,
                        help=f"XTTS-v2 host:port (default: {DEFAULT_HOST})")
    parser.add_argument("--phrases-yaml", default=str(PHRASES_YAML),
                        help="Path to npc_phrases.yaml")
    args = parser.parse_args()

    phrases_path = pathlib.Path(args.phrases_yaml)
    if not phrases_path.exists():
        print(f"ERROR: phrases file not found: {phrases_path}", file=sys.stderr)
        return 1

    # Parse phrase bank
    with open(phrases_path, encoding="utf-8") as f:
        data = yaml.safe_load(f)

    phrases = flatten_phrases(data)
    print(f"Phrase bank: {len(phrases)} phrases found in {phrases_path}")

    # Health check before starting (skip in dry-run)
    if not args.dry_run:
        print(f"Checking XTTS-v2 at {args.host}...", end=" ", flush=True)
        if not check_xtts_health(args.host):
            print("UNREACHABLE")
            print(f"ERROR: XTTS-v2 not reachable at http://{args.host}/health",
                  file=sys.stderr)
            print("Start the service first: docker compose -f tools/tts/docker-compose.xtts.yml up -d",
                  file=sys.stderr)
            return 1
        print("OK")

    # Load existing manifest
    manifest: dict[str, str] = {}
    if MANIFEST.exists():
        with open(MANIFEST, encoding="utf-8") as f:
            manifest = json.load(f)

    generated = skipped = errors = 0

    for ph in phrases:
        key  = ph["key"]
        text = ph["text"]
        path = key_to_path(key)

        # Skip if already generated (unless --force)
        if not args.force and path.exists():
            skipped += 1
            manifest[key] = str(path)
            continue

        label = "[DRY]" if args.dry_run else "[GEN]"
        preview = text[:70] + "..." if len(text) > 70 else text
        print(f"  {label} {key}")
        print(f"         {preview}")

        if args.dry_run:
            generated += 1
            continue

        # Synthesize with retries
        wav_bytes: bytes | None = None
        for attempt in range(1, RETRY_COUNT + 1):
            try:
                wav_bytes = synthesize(text, args.host)
                break
            except Exception as exc:
                print(f"  WARNING attempt {attempt}/{RETRY_COUNT}: {exc}",
                      file=sys.stderr)
                if attempt < RETRY_COUNT:
                    time.sleep(RETRY_DELAY_S)

        if wav_bytes is None:
            print(f"  ERROR: all retries failed for key={key}", file=sys.stderr)
            errors += 1
            continue

        # Write file
        try:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(wav_bytes)
            manifest[key] = str(path)
            generated += 1
        except OSError as exc:
            print(f"  ERROR writing {path}: {exc}", file=sys.stderr)
            errors += 1

    # Save manifest
    if not args.dry_run:
        MANIFEST.parent.mkdir(parents=True, exist_ok=True)
        with open(MANIFEST, "w", encoding="utf-8") as f:
            json.dump(manifest, f, indent=2, ensure_ascii=False)
        print(f"\nManifest written: {MANIFEST} ({len(manifest)} entries)")

    print(f"\nResult: {generated} generated, {skipped} skipped, {errors} errors")
    return 0 if errors == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
