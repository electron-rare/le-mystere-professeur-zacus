#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

echo "[1/2] Build matrix…"
if [[ -x "./build_all.sh" ]]; then
  ./build_all.sh
else
  pio run -e esp32dev
  pio run -e esp32_release
  pio run -e esp8266_oled
  pio run -e ui_rp2040_ili9488
  pio run -e ui_rp2040_ili9486
fi

echo
read -r -p "[2/2] Plug USB now, then press Enter to start serial smoke… " _

python3 tools/dev/serial_smoke.py --wait-port 180 --role auto --baud 19200
