#!/bin/bash
# migrate_littlefs_data.sh
# Centralise tous les assets LittleFS dans hardware/firmware/data/ et nettoie les anciens dossiers.
# Usage : bash migrate_littlefs_data.sh

set -euo pipefail

ROOT="$(dirname "$0")"
FIRMWARE_ROOT="$ROOT"
DATA_ROOT="$FIRMWARE_ROOT/data"

# Crée la structure cible si besoin
mkdir -p "$DATA_ROOT/story/apps" "$DATA_ROOT/story/screens" "$DATA_ROOT/story/audio" "$DATA_ROOT/story/actions" "$DATA_ROOT/audio" "$DATA_ROOT/radio" "$DATA_ROOT/net"

# Déplace les assets audio ESP32
if [ -d "$FIRMWARE_ROOT/esp32_audio/data" ]; then
  mv "$FIRMWARE_ROOT/esp32_audio/data"/*.mp3 "$DATA_ROOT/audio/" 2>/dev/null || true
  mv "$FIRMWARE_ROOT/esp32_audio/data"/*.wav "$DATA_ROOT/audio/" 2>/dev/null || true
  mv "$FIRMWARE_ROOT/esp32_audio/data/radio" "$DATA_ROOT/radio" 2>/dev/null || true
  mv "$FIRMWARE_ROOT/esp32_audio/data/net" "$DATA_ROOT/net" 2>/dev/null || true
fi

# Déplace les écrans/scènes RP2040
if [ -d "$FIRMWARE_ROOT/ui/rp2040_tft/data" ]; then
  mv "$FIRMWARE_ROOT/ui/rp2040_tft/data"/*.json "$DATA_ROOT/story/screens/" 2>/dev/null || true
fi

# Déplace les écrans/scènes ESP8266 (si dossier existe)
if [ -d "$FIRMWARE_ROOT/ui/esp8266_oled/data" ]; then
  mv "$FIRMWARE_ROOT/ui/esp8266_oled/data"/*.json "$DATA_ROOT/story/screens/" 2>/dev/null || true
fi

# Nettoie les anciens dossiers (optionnel, décommentez pour suppression auto)
# rm -rf "$FIRMWARE_ROOT/esp32_audio/data"
# rm -rf "$FIRMWARE_ROOT/ui/rp2040_tft/data"
# rm -rf "$FIRMWARE_ROOT/ui/esp8266_oled/data"

echo "Migration terminée. Vérifiez le dossier $DATA_ROOT et adaptez vos scripts si besoin."
