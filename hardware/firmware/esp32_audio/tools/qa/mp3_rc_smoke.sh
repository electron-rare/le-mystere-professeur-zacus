#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT_DIR"

echo "[mp3-rc-smoke] validate story specs"
make story-validate

echo "[mp3-rc-smoke] generate story sources"
make story-gen

echo "[mp3-rc-smoke] build esp32dev"
pio run -e esp32dev

echo "[mp3-rc-smoke] build esp32_release"
pio run -e esp32_release

echo "[mp3-rc-smoke] build esp8266_oled"
pio run -e esp8266_oled

echo "[mp3-rc-smoke] build screen nodemcuv2"
(
  cd screen_esp8266_hw630
  pio run -e nodemcuv2
)

echo "[mp3-rc-smoke] static checks OK"
echo "[mp3-rc-smoke] next live serial commands:"
echo "  MP3_STATUS"
echo "  MP3_UI_STATUS"
echo "  MP3_SCAN START"
echo "  MP3_SCAN_PROGRESS"
echo "  MP3_QUEUE_PREVIEW 5"
echo "  MP3_BACKEND_STATUS"
echo "  MP3_CAPS"
