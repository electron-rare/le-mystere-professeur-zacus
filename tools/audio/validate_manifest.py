#!/usr/bin/env python3
from __future__ import annotations
import json
import subprocess
import sys
from pathlib import Path


def load_yaml(path: Path):
    ruby = "require 'yaml'; require 'json'; puts JSON.generate(YAML.load_file(ARGV[0]))"
    p = subprocess.run(["ruby", "-e", ruby, str(path)], capture_output=True, text=True)
    if p.returncode != 0:
        raise SystemExit(f"YAML parse error: {p.stderr.strip()}")
    return json.loads(p.stdout)


def main() -> int:
    manifest = Path("audio/manifests/zacus_v1_audio.yaml")
    if not manifest.exists():
        print("Manifest not found:", manifest)
        return 1
    doc = load_yaml(manifest)
    missing = []
    for asset in doc.get("audio_assets", []):
        p = asset.get("path", "")
        if p == "PROMPT_ONLY":
            continue
        if p and not Path(p).exists():
            missing.append(p)
    if missing:
        print("Missing list:")
        for m in missing:
            print(f" - {m}")
        return 2
    print("OK: all declared audio files exist (or PROMPT_ONLY).")
    return 0


if __name__ == "__main__":
    sys.exit(main())
