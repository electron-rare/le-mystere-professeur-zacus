#!/usr/bin/env python3
"""
generate_tracks.py — Generate 6 ambient audio tracks using AudioCraft MusicGen.
Run on KXKM-AI (RTX 4090) via docker-compose.audiocraft.yml or directly.

Output: output/{track_name}.wav (mono, 32kHz — AudioCraft native sample rate)

Usage:
    python3 generate_tracks.py [--dry-run] [--track lab_ambiance]

Notes:
    - AudioCraft MusicGen max generation duration per call is 30s.
    - Tracks longer than 30s are assembled from multiple segments.
    - Model: facebook/musicgen-medium (1.5B params, good quality/speed balance).
    - RTX 4090 generates ~30s in ~10s wall time.
"""
import argparse
import pathlib
import sys
import time

OUTPUT_DIR = pathlib.Path("output")

TRACKS = [
    {
        "name": "lab_ambiance.wav",
        "duration": 30,
        "segments": 1,
        "prompt": (
            "Laboratory ambient sound, machines humming, electronic beeps, "
            "ventilation fan, subtle mechanical sounds, science lab, mysterious, "
            "loopable, no melody, background ambiance"
        ),
    },
    {
        "name": "tension_rising.wav",
        "duration": 30,  # generate 30s base; extend in post-processing for 5min loop
        "segments": 1,
        "prompt": (
            "Dramatic tension building music, suspenseful, orchestral, "
            "slow crescendo, escape room atmosphere, mysterious puzzle solving, "
            "cinematic, no lyrics, dark ambient"
        ),
    },
    {
        "name": "victory.wav",
        "duration": 30,
        "segments": 1,
        "prompt": (
            "Victory fanfare, joyful celebration music, brass orchestra, "
            "applause, triumphant, escape room win, uplifting, energetic, "
            "short stinger"
        ),
    },
    {
        "name": "failure.wav",
        "duration": 15,
        "segments": 1,
        "prompt": (
            "Failure buzzer sound effect, trombone descending, game over, "
            "humorous sad tuba, cartoon fail sound, short, comedic"
        ),
    },
    {
        "name": "thinking.wav",
        "duration": 30,
        "segments": 1,
        "prompt": (
            "Subtle suspense music for thinking, minimal, ambient, "
            "slow piano, light electronic, puzzle solving concentration, "
            "loopable, calm but mysterious, soft"
        ),
    },
    {
        "name": "transition.wav",
        "duration": 10,
        "segments": 1,
        "prompt": (
            "Scene transition sound effect, swoosh, magical glitter, "
            "short stinger, puzzle reveal, sparkle effect, 10 seconds, "
            "ascending chime"
        ),
    },
]


def generate_track(model, description: str, duration_s: int, filename: str) -> pathlib.Path:
    """Generate a single track and save as WAV."""
    import torch
    import torchaudio

    print(f"  Generating: {filename} ({duration_s}s)")
    print(f"  Prompt: {description[:80]}...")

    # Cap duration at 30s per AudioCraft limit
    actual_duration = min(duration_s, 30)
    model.set_generation_params(duration=actual_duration)

    t0 = time.time()
    with torch.no_grad():
        wav = model.generate(
            descriptions=[description],
            progress=True,
        )
    elapsed = time.time() - t0
    print(f"  Generated in {elapsed:.1f}s")

    out_path = OUTPUT_DIR / filename
    # AudioCraft returns tensor [batch, channels, samples] at model.sample_rate
    torchaudio.save(
        str(out_path),
        wav[0].cpu(),
        model.sample_rate,
        format="wav",
    )
    print(f"  Saved: {out_path} ({out_path.stat().st_size // 1024} KB)")
    return out_path


def main():
    parser = argparse.ArgumentParser(description="Generate ambient tracks for Zacus V3")
    parser.add_argument("--dry-run", action="store_true", help="Print track list without generating")
    parser.add_argument("--track", metavar="NAME", help="Generate only the specified track by name stem")
    parser.add_argument("--model", default="facebook/musicgen-medium", help="HuggingFace model ID")
    args = parser.parse_args()

    OUTPUT_DIR.mkdir(exist_ok=True)

    selected = TRACKS
    if args.track:
        selected = [t for t in TRACKS if pathlib.Path(t["name"]).stem == args.track]
        if not selected:
            print(f"ERROR: track '{args.track}' not found. Available: {[t['name'] for t in TRACKS]}")
            return 1

    if args.dry_run:
        print("=== DRY RUN — tracks that would be generated ===")
        for t in selected:
            print(f"  {t['name']:30s}  {t['duration']:3d}s  {t['prompt'][:60]}...")
        print(f"\nTotal: {len(selected)} track(s)")
        return 0

    print(f"Loading model: {args.model}")
    from audiocraft.models import MusicGen
    model = MusicGen.get_pretrained(args.model)
    print(f"Model loaded: {args.model}\n")

    results = []
    for i, track in enumerate(selected, 1):
        print(f"[{i}/{len(selected)}] {track['name']}")
        out = generate_track(
            model,
            description=track["prompt"],
            duration_s=track["duration"],
            filename=track["name"],
        )
        results.append(out)
        print()

    print("=" * 50)
    print(f"All {len(results)} track(s) generated successfully.")
    print(f"Output directory: {OUTPUT_DIR.absolute()}")
    for r in results:
        print(f"  {r.name:30s}  {r.stat().st_size // 1024} KB")
    return 0


if __name__ == "__main__":
    sys.exit(main())
