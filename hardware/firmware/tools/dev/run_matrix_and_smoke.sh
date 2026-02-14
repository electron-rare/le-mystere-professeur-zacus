#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

choose_platformio_core_dir() {
  if [[ -n "${PLATFORMIO_CORE_DIR:-}" ]]; then
    echo "PLATFORMIO_CORE_DIR=${PLATFORMIO_CORE_DIR}"
    return
  fi
  local candidate="$HOME/.platformio"
  if mkdir -p "$candidate" >/dev/null 2>&1 && [[ -w "$candidate" ]]; then
    export PLATFORMIO_CORE_DIR="$candidate"
  else
    candidate="/tmp/zacus-platformio-${USER}-$(date +%s)"
    mkdir -p "$candidate"
    export PLATFORMIO_CORE_DIR="$candidate"
  fi
  echo "PLATFORMIO_CORE_DIR=${PLATFORMIO_CORE_DIR}"
}

prepare_pip_cache() {
  local cache_dir="/tmp/zacus-pip-cache-${USER}"
  mkdir -p "$cache_dir"
  export PIP_CACHE_DIR="$cache_dir"
}

run_pio_env() {
  local env=$1
  local logfile
  logfile="$(mktemp)"
  if ! pio run -e "$env" 2>&1 | tee "$logfile"; then
    if grep -q "HTTPClientError" "$logfile"; then
      echo
      echo "PlatformIO failed to download packages due to network restrictions."
      echo "Please rerun 'pio run -e $env' on a machine with outbound HTTP access."
    else
      echo
      echo "PlatformIO build failed for env $env; inspect the log above."
    fi
    rm -f "$logfile"
    exit 1
  fi
  rm -f "$logfile"
}

run_build_all() {
  local logfile
  logfile="$(mktemp)"
  if ! ./build_all.sh 2>&1 | tee "$logfile"; then
    if grep -q "HTTPClientError" "$logfile"; then
      echo
      echo "PlatformIO failed to download packages during build_all due to network restrictions."
      echo "Please rerun './build_all.sh' on a machine with outbound HTTP access."
    else
      echo
      echo "build_all.sh failed; inspect the output above."
    fi
    rm -f "$logfile"
    exit 1
  fi
  rm -f "$logfile"
}

list_ports_verbose() {
  echo "  [ports] Available serial ports (best effort):"
  if ! python3 -m serial.tools.list_ports -v; then
    echo "    (unable to list ports; install pyserial via 'pip install pyserial')"
  fi
}

print_usb_alert() {
  cat <<'EOF'
===========================================
🚨🚨🚨  USB CONNECT ALERT  🚨🚨🚨
===========================================
Connect the CP2102-based adapters for:
  • ESP32 (primary role, LOCATION 20-6.1.1)
  • ESP8266 (secondary role, LOCATION 20-6.1.2)
Optional: RP2040/Pico devices.

macOS devices typically appear as:
  /dev/cu.SLAB_USBtoUART*
  /dev/cu.usbserial-*
  /dev/cu.usbmodem*

Keep the cables seated and adapters powered before the countdown ends.
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
    printf '  Plug USB now — %ds remaining...\n' "$i"
    if (( i % 5 == 0 )); then
      echo "⚠️ BRANCHE L’USB MAINTENANT ⚠️"
      list_ports_verbose
    fi
    sleep 1
  done
  echo "USB countdown complete."
}

choose_platformio_core_dir
prepare_pip_cache

echo "[0/3] Ensuring Python deps..."
python3 -m pip install --no-cache-dir -U pip pyserial

echo "[1/3] Build matrix…"
if [[ -x "./build_all.sh" ]]; then
  run_build_all
else
  run_pio_env esp32dev
  run_pio_env esp32_release
  run_pio_env esp8266_oled
  run_pio_env ui_rp2040_ili9488
  run_pio_env ui_rp2040_ili9486
fi

echo
print_usb_alert
run_countdown

echo "[2/3] Running serial smoke…"
SMOKE_CMD=(python3 tools/dev/serial_smoke.py --role auto --baud 19200)
if [[ "${ZACUS_REQUIRE_HW:-0}" == "1" ]]; then
  SMOKE_CMD+=(--wait-port 180)
else
  SMOKE_CMD+=(--wait-port 3 --allow-no-hardware)
fi
"${SMOKE_CMD[@]}"
