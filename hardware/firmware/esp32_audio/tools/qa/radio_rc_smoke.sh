#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT_DIR"

echo "[radio-rc-smoke] story-validate"
make story-validate

echo "[radio-rc-smoke] story-gen"
make story-gen

echo "[radio-rc-smoke] build esp32dev"
pio run -e esp32dev

echo "[radio-rc-smoke] build esp32_release"
pio run -e esp32_release

echo "[radio-rc-smoke] build esp8266_oled"
pio run -e esp8266_oled

echo "[radio-rc-smoke] build ui_rp2040_ili9488"
pio run -e ui_rp2040_ili9488

echo "[radio-rc-smoke] build ui_rp2040_ili9486"
pio run -e ui_rp2040_ili9486

echo "[radio-rc-smoke] Command checklist (manual serial)"
cat <<'CMDS'
RADIO_STATUS
RADIO_LIST 0 5
RADIO_PLAY 1
RADIO_META
RADIO_NEXT
RADIO_PREV
RADIO_STOP
WIFI_STATUS
WIFI_SCAN
WIFI_AP_ON
WEB_STATUS
CMDS

echo "[radio-rc-smoke] OK"
