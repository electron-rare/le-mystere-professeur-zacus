#!/bin/bash
# Script pour builder et tester tous les firmwares PlatformIO du repo
set -e
cd "$(dirname "$0")"

envs=(
  "esp32dev"
  "esp32_release"
  "ui_rp2040_ili9488"
  "ui_rp2040_ili9486"
  "esp8266_oled"
)

for env in "${envs[@]}"; do
  echo "[BUILD] ${env}..."
  pio run -e "${env}"
done

echo "[OK] Build global terminé."
