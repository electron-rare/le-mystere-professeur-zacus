envs=(
echo "[OK] Build global terminé."

#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
source "tools/dev/agent_utils.sh"

build_gate pio run -e esp32dev -e esp32_release -e esp8266_oled -e ui_rp2040_ili9488 -e ui_rp2040_ili9486
artefact_gate .pio/build artifacts/build
log "Build OK"
