#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT_DIR"
SKIP_BUILDS="${QA_STORY_V2_SKIP_BUILDS:-0}"
REQUIRE_CLEAN_GEN="${QA_STORY_V2_REQUIRE_CLEAN_GEN:-1}"

SPEC_DIR="../docs/protocols/story_specs/scenarios"

echo "[qa-story-v2] validate(strict)"
python3 tools/story_gen/story_gen.py validate --strict --spec-dir "$SPEC_DIR"

echo "[qa-story-v2] generate(strict) pass#1"
python3 tools/story_gen/story_gen.py generate --strict --spec-dir "$SPEC_DIR" --out-dir src/story/generated

if ! git diff --quiet -- src/story/generated; then
  if [ "$REQUIRE_CLEAN_GEN" = "1" ]; then
    echo "[qa-story-v2] ERROR: generated files changed after pass#1"
    echo "[qa-story-v2] run 'make story-gen' and commit generated files"
    git --no-pager diff -- src/story/generated
    exit 1
  fi
  echo "[qa-story-v2] WARN: generated files changed after pass#1"
fi

SNAPSHOT_DIR="$(mktemp -d)"
cleanup() {
  rm -rf "$SNAPSHOT_DIR"
}
trap cleanup EXIT
cp src/story/generated/*.h src/story/generated/*.cpp "$SNAPSHOT_DIR"/

echo "[qa-story-v2] generate(strict) pass#2 (idempotence)"
python3 tools/story_gen/story_gen.py generate --strict --spec-dir "$SPEC_DIR" --out-dir src/story/generated

if ! diff -ru "$SNAPSHOT_DIR" src/story/generated >/tmp/story_v2_idempotence.diff; then
  echo "[qa-story-v2] ERROR: generation not idempotent"
  cat /tmp/story_v2_idempotence.diff
  exit 1
fi

if [ "$SKIP_BUILDS" != "1" ]; then
  echo "[qa-story-v2] build esp32dev"
  pio run -e esp32dev --project-dir ..

  echo "[qa-story-v2] build esp32_release"
  pio run -e esp32_release --project-dir ..

  echo "[qa-story-v2] build esp8266_oled"
  pio run -e esp8266_oled --project-dir ..

  echo "[qa-story-v2] build ui_rp2040_ili9488"
  pio run -e ui_rp2040_ili9488 --project-dir ..

  echo "[qa-story-v2] build ui_rp2040_ili9486"
  pio run -e ui_rp2040_ili9486 --project-dir ..
else
  echo "[qa-story-v2] builds skipped (QA_STORY_V2_SKIP_BUILDS=1)"
fi

echo "[qa-story-v2] OK"
