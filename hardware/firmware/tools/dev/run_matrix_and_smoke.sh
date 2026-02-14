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

BUILD_STATUS="SKIPPED (ZACUS_SKIP_PIO=1)"
if [[ "${ZACUS_SKIP_PIO:-0}" == "1" ]]; then
  echo "[1/3] Build matrix… (skipped via ZACUS_SKIP_PIO=1)"
else
ENVS=(esp32dev esp32_release esp8266_oled ui_rp2040_ili9488 ui_rp2040_ili9486)

all_builds_present() {
  for env in "${ENVS[@]}"; do
    if [[ ! -f ".pio/build/$env/firmware.bin" && ! -f ".pio/build/$env/firmware.elf" ]]; then
      return 1
    fi
  done
  return 0
}

BUILD_STATUS="SKIPPED (ZACUS_SKIP_PIO=1)"
if [[ "${ZACUS_SKIP_PIO:-0}" == "1" ]]; then
  echo "[1/3] Build matrix… (skipped via ZACUS_SKIP_PIO=1)"
elif [[ "${ZACUS_FORCE_BUILD:-0}" == "1" ]]; then
  echo "[1/3] Build matrix… (forced rebuild)"
  if [[ -x "./build_all.sh" ]]; then
    run_build_all
  else
    for env in "${ENVS[@]}"; do
      run_pio_env "$env"
    done
  fi
  BUILD_STATUS="OK"
elif all_builds_present; then
  echo "[1/3] Build matrix… (already built)"
  BUILD_STATUS="SKIPPED (already built)"
else
  echo "[1/3] Build matrix…"
  if [[ -x "./build_all.sh" ]]; then
    run_build_all
  else
    for env in "${ENVS[@]}"; do
      run_pio_env "$env"
    done
  fi
  BUILD_STATUS="OK"
fi
  BUILD_STATUS="OK"
fi

echo
print_usb_alert
run_countdown

echo "[2/3] Running serial smoke…"
DEFAULT_WAIT_PORT="${ZACUS_WAIT_PORT:-30}"
SMOKE_CMD=(python3 tools/dev/serial_smoke.py --role auto --baud 19200)
if [[ "${ZACUS_REQUIRE_HW:-0}" == "1" ]]; then
  SMOKE_CMD+=(--wait-port 180)
else
  SMOKE_CMD+=(--wait-port "$DEFAULT_WAIT_PORT" --allow-no-hardware)
fi
SMOKE_COMMAND_STRING="$(printf '%q ' "${SMOKE_CMD[@]}")"
SMOKE_LOG="$(mktemp)"
set +e
if "${SMOKE_CMD[@]}" 2>&1 | tee "$SMOKE_LOG"; then
  if grep -q "SKIP: no hardware detected" "$SMOKE_LOG"; then
    SMOKE_STATUS="SKIP"
  else
    SMOKE_STATUS="OK"
  fi
else
  SMOKE_STATUS="FAILED"
  rm -f "$SMOKE_LOG"
  echo
  echo "Smoke step failed; rerun the command above once the hardware is ready."
  exit 1
fi
set -e
rm -f "$SMOKE_LOG"

echo
echo "=== Run summary ==="
echo "Build status: $BUILD_STATUS"
echo "Smoke status: $SMOKE_STATUS (command: $SMOKE_COMMAND_STRING)"
