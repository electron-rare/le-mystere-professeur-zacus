#!/usr/bin/env python3
"""Generate printable PNGs from the official printable manifest.
"""

from __future__ import annotations

import argparse
import base64
import os
import sys
from pathlib import Path

try:
    import yaml
except ImportError as exc:
    print("Missing dependency: install PyYAML:", exc)
    sys.exit(1)

try:
    import openai
except ImportError as exc:
    print("Missing dependency: install openai", exc)
    sys.exit(1)

from openai import OpenAI, OpenAIError

PRINTABLES_DIR = Path("printables")
MANIFEST_PATH = PRINTABLES_DIR / "manifests" / "zacus_v1_printables.yaml"
EXPORT_DIR = PRINTABLES_DIR / "export" / "png" / "zacus_v1"
MODEL = "gpt-image-1-mini"
SIZE = "1024x1024"
QUALITY = "low"
COST_PER_IMAGE = 0.005  # USD per low square image


def load_manifest(path: Path) -> list[dict]:
    if not path.exists():
        raise FileNotFoundError(f"Manifest not found: {path}")
    manifest = yaml.safe_load(path.read_text())
    items = manifest.get("items") or []
    if not isinstance(items, list):
        raise ValueError("Manifest items must be a list")
    return items


def load_prompt(item: dict) -> tuple[str, Path]:
    prompt_rel = item.get("prompt")
    if not prompt_rel:
        raise ValueError(f"Missing prompt reference for item {item.get('id')}")
    prompt_path = PRINTABLES_DIR / prompt_rel
    if not prompt_path.exists():
        raise FileNotFoundError(f"Prompt file missing: {prompt_path}")
    return prompt_path.read_text().strip(), prompt_path


def ensure_api_key() -> None:
    if not os.environ.get("OPENAI_API_KEY"):
        print("OPENAI_API_KEY is required for image generation")
        sys.exit(1)


def generate_image(client: OpenAI, prompt: str) -> bytes:
    response = client.images.generate(
        model=MODEL,
        prompt=prompt,
        size=SIZE,
        quality=QUALITY,
        n=1,
        output_format="png",
    )
    asset = response.data[0]
    data = asset.b64_json
    return base64.b64decode(data)


def main() -> int:
    parser = argparse.ArgumentParser(description="Generate PNG printables via OpenAI Images")
    parser.add_argument("--manifest", type=Path, default=MANIFEST_PATH, help="Printable manifest path")
    parser.add_argument("--force", action="store_true", help="Regenerate even when PNG exists")
    args = parser.parse_args()

    ensure_api_key()
    client = OpenAI()
    items = load_manifest(args.manifest)
    EXPORT_DIR.mkdir(parents=True, exist_ok=True)

    counters = {"generated": 0, "skipped": 0, "failed": 0}

    for item in items:
        item_id = item.get("id")
        if not item_id:
            print("Skipping entry without id")
            counters["failed"] += 1
            continue
        destination = EXPORT_DIR / f"{item_id}.png"
        if destination.exists() and not args.force:
            counters["skipped"] += 1
            continue
        try:
            prompt_text, prompt_path = load_prompt(item)
            image_bytes = generate_image(client, prompt_text)
            destination.write_bytes(image_bytes)
            counters["generated"] += 1
            print(f"Generated {destination}")
        except (OpenAIError, FileNotFoundError, ValueError) as exc:
            print(f"Failed {item_id}: {exc}")
            counters["failed"] += 1

    estimated_cost = counters["generated"] * COST_PER_IMAGE
    print(
        f"Summary: generated={counters['generated']}, skipped={counters['skipped']}, failed={counters['failed']}."
        f" Estimated cost: ${estimated_cost:.3f}"
    )
    return 0 if counters["failed"] == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
