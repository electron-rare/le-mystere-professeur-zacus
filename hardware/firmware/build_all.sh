#!/bin/bash
# Script pour builder et tester tous les firmwares PlatformIO du repo
set -e
cd "$(dirname "$0")"

# Build ESP32
if [ -d "esp32" ]; then
  echo "[BUILD] ESP32..."
  cd esp32 && pio run && cd ..
fi

# Build UI RP2040
if [ -d "ui/rp2040" ]; then
  echo "[BUILD] UI RP2040..."
  cd ui/rp2040 && pio run && cd ../..
fi
# Build écran ESP8266 (optionnel)
if [ -d "ui/esp8266_oled" ]; then
  echo "[BUILD] ESP8266 OLED..."
  cd ui/esp8266_oled && pio run && cd ../..
fi

echo "[OK] Build global terminé."
