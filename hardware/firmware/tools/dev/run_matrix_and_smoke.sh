#!/usr/bin/env bash
set -euo pipefail

export PLATFORMIO_CORE_DIR="${PLATFORMIO_CORE_DIR:-$HOME/.platformio}"

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

list_ports_verbose() {
  echo "  [ports] Available serial ports (best effort):"
  set +e
  python3 -m serial.tools.list_ports -v
  local status=$?
  set -e
  if (( status != 0 )); then
    echo "    (unable to list ports; install pyserial via 'pip install pyserial')"
  fi
}

print_usb_alert() {
  cat <<'EOF'
===========================================
🚨🚨🚨  USB CONNECT ALERT  🚨🚨🚨
===========================================
Plug in the CP2102-based USB serial adapters for:
  • ESP32 (primary role)
  • ESP8266 (secondary role)
Optional: RP2040-based Pico boards for extras.

macOS device patterns to watch for:
  /dev/cu.SLAB_USBtoUART*
  /dev/cu.usbserial-*
  /dev/cu.usbmodem*

Ensure the cables are seated and adapters have power.
EOF
}

run_countdown() {
  if [[ "${ZACUS_NO_COUNTDOWN:-0}" == "1" ]]; then
    echo "USB countdown skipped (ZACUS_NO_COUNTDOWN=1)."
    return
  fi

  local countdown="${ZACUS_USB_COUNTDOWN:-20}"
  if ! [[ "$countdown" =~ ^[0-9]+$ ]]; then
    countdown=20
  fi
  if (( countdown <= 0 )); then
    countdown=1
  fi

  echo "Starting USB countdown (${countdown}s)..."
  for ((i = countdown; i > 0; i--)); do
    printf '\a'
    echo "  Plug USB devices now — ${i}s remaining..."
    if (( i % 5 == 0 )); then
      echo "⚠️ BRANCHE L’USB MAINTENANT ⚠️"
      list_ports_verbose
    fi
    sleep 1
  done
  echo "USB countdown complete."
}

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
print_usb_alert
run_countdown

echo "[2/2] Running serial smoke…"
SMOKE_CMD=(python3 tools/dev/serial_smoke.py --role auto --baud 19200)
if [[ "${ZACUS_REQUIRE_HW:-0}" == "1" ]]; then
  SMOKE_CMD+=(--wait-port 180)
else
  SMOKE_CMD+=(--wait-port 3 --allow-no-hardware)
fi
"${SMOKE_CMD[@]}"
