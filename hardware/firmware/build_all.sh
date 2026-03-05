#!/usr/bin/env bash
set -euo pipefail

# Sécurisation de $1 pour éviter unbound variable
arg1="${1:-}"
cd "$(dirname "$0")"
source "tools/dev/agent_utils.sh"

# Keep build_all usable even when ~/.platformio is not writable in CI/sandboxed runs.
if [[ -z "${PLATFORMIO_CORE_DIR:-}" ]]; then
	if [[ -d "$HOME/.platformio" && -w "$HOME/.platformio" ]]; then
		export PLATFORMIO_CORE_DIR="$HOME/.platformio"
	elif [[ -d "/tmp/pio_audit" ]]; then
		export PLATFORMIO_CORE_DIR="/tmp/pio_audit"
	else
		export PLATFORMIO_CORE_DIR="/tmp/zacus-platformio-${USER:-user}"
	fi
fi
mkdir -p "$PLATFORMIO_CORE_DIR"
log "PLATFORMIO_CORE_DIR=$PLATFORMIO_CORE_DIR"

if [[ -n "$arg1" ]]; then
	build_gate pio run -e "$arg1"
else
	build_gate pio run -e freenove_esp32s3 -e esp8266_oled
fi
artefact_gate .pio/build artifacts/build
log "Build OK"
