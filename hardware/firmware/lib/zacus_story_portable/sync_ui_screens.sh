#!/bin/bash
# Synchronise les fichiers JSON d'écrans vers le dossier LittleFS de l'UI RP2040
# Usage: ./tools/dev/sync_ui_screens.sh

set -euo pipefail
cd "$(dirname "$0")/../.."
source tools/dev/layout_paths.sh

SRC="$(pwd)/artifacts/littlefs_build/screens"
UI_TFT_SRC_ROOT="$(fw_ui_tft_src)"
DST="$(cd "$UI_TFT_SRC_ROOT/.." && pwd)/data"

if [ ! -d "$SRC" ]; then
  echo "[FAIL] Source screens introuvable: $SRC" >&2
  exit 1
fi
mkdir -p "$DST"
cp -v "$SRC"/*.json "$DST"/
echo "[OK] Synchronisation des écrans UI terminée."
