#!/usr/bin/env bash
set -euo pipefail

# Sécurisation de $1 pour éviter unbound variable
arg1="${1:-}"
cd "$(dirname "$0")"
source "tools/dev/agent_utils.sh"

if [[ -n "$arg1" ]]; then
	build_gate pio run -e "$arg1"
else
	build_gate pio run -e esp32dev -e esp32_release -e freenove_esp32s3 -e esp8266_oled -e ui_rp2040_ili9488 -e ui_rp2040_ili9486
fi
artefact_gate .pio/build artifacts/build
log "Build OK"
