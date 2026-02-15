#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

DEFAULT_ENVS=(esp32dev esp32_release esp8266_oled ui_rp2040_ili9488 ui_rp2040_ili9486)
BUILD_STATUS="SKIPPED (not run)"
SMOKE_STATUS="SKIPPED (not run)"
SMOKE_COMMAND_STRING=""

log_step() {
  echo "[step] $*"
}

log_info() {
  echo "[info] $*"
}

log_warn() {
  echo "[warn] $*"
}

log_error() {
  echo "[error] $*" >&2
}

require_cmd() {
  local cmd=$1
  if ! command -v "$cmd" >/dev/null 2>&1; then
    log_error "missing command: $cmd"
    return 1
  fi
}

choose_platformio_core_dir() {
  if [[ -n "${PLATFORMIO_CORE_DIR:-}" ]]; then
    log_info "PLATFORMIO_CORE_DIR=${PLATFORMIO_CORE_DIR}"
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
  log_info "PLATFORMIO_CORE_DIR=${PLATFORMIO_CORE_DIR}"
}

activate_local_venv_if_present() {
  if [[ -n "${VIRTUAL_ENV:-}" ]]; then
    return 0
  fi
  if [[ -f ".venv/bin/activate" ]]; then
    # shellcheck disable=SC1091
    source .venv/bin/activate
    log_info "using local venv: .venv"
  fi
}

ensure_pyserial() {
  if python3 -c "import serial" >/dev/null 2>&1; then
    return 0
  fi
  activate_local_venv_if_present
  if python3 -c "import serial" >/dev/null 2>&1; then
    return 0
  fi
  log_error "pyserial is missing. Run ./tools/dev/bootstrap_local.sh first."
  return 1
}

parse_envs() {
  local raw="${ZACUS_ENV:-}"
  if [[ -z "$raw" ]]; then
    ENVS=("${DEFAULT_ENVS[@]}")
    return
  fi
  raw="${raw//,/ }"
  # shellcheck disable=SC2206
  ENVS=($raw)
  if (( ${#ENVS[@]} == 0 )); then
    ENVS=("${DEFAULT_ENVS[@]}")
  fi
}

all_builds_present() {
  local env
  for env in "${ENVS[@]}"; do
    if [[ ! -f ".pio/build/$env/firmware.elf" && ! -f ".pio/build/$env/firmware.bin" && ! -f ".pio/build/$env/firmware.bin.signed" ]]; then
      return 1
    fi
  done
  return 0
}

run_pio_env() {
  local env=$1
  local logfile
  logfile="$(mktemp)"
  if ! pio run -e "$env" 2>&1 | tee "$logfile"; then
    if grep -q "HTTPClientError" "$logfile"; then
      log_error "PlatformIO failed to download packages due to network restrictions."
      log_error "Rerun: pio run -e $env on a machine with outbound HTTP access."
    else
      log_error "PlatformIO build failed for env $env; inspect the log above."
    fi
    rm -f "$logfile"
    return 1
  fi
  rm -f "$logfile"
  return 0
}

run_build_matrix() {
  local env
  local failed_envs=()
  for env in "${ENVS[@]}"; do
    log_info "building env: $env"
    if ! run_pio_env "$env"; then
      failed_envs+=("$env")
      break
    fi
  done
  if (( ${#failed_envs[@]} > 0 )); then
    log_error "build matrix failed on: ${failed_envs[*]}"
    log_error "retry command: pio run -e ${failed_envs[0]}"
    return 1
  fi
  return 0
}

list_ports_verbose() {
  echo "  [ports] available serial ports (best effort):"
  if ! python3 -m serial.tools.list_ports -v; then
    log_warn "unable to list ports; install pyserial via ./tools/dev/bootstrap_local.sh"
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
    log_info "USB countdown skipped (ZACUS_NO_COUNTDOWN=1)"
    return
  fi
  local countdown="${ZACUS_USB_COUNTDOWN:-5}"
  if ! [[ "$countdown" =~ ^[0-9]+$ ]]; then
    countdown=5
  fi
  if (( countdown <= 0 )); then
    countdown=1
  fi
  log_info "starting USB countdown (${countdown}s)"
  for ((i = countdown; i > 0; i--)); do
    printf '\a'
    printf '  Plug USB now — %ds remaining...\n' "$i"
    if (( i % 5 == 0 )); then
      echo "⚠️ BRANCHE L’USB MAINTENANT ⚠️"
      list_ports_verbose
    fi
    sleep 1
  done
  log_info "USB countdown complete"
}

run_smoke() {
  local default_wait_port="${ZACUS_WAIT_PORT:-3}"
  local smoke_role="${ZACUS_SMOKE_ROLE:-auto}"
  local smoke_baud="${ZACUS_BAUD:-19200}"
  local smoke_timeout="${ZACUS_TIMEOUT:-1.0}"

  if ! [[ "$default_wait_port" =~ ^[0-9]+$ ]]; then
    default_wait_port=3
  fi

  SMOKE_CMD=(python3 tools/dev/serial_smoke.py --role "$smoke_role" --baud "$smoke_baud" --timeout "$smoke_timeout")
  if [[ -n "${ZACUS_SMOKE_PORT:-}" ]]; then
    SMOKE_CMD+=(--port "${ZACUS_SMOKE_PORT}")
  fi
  if [[ "${ZACUS_SMOKE_ALL:-0}" == "1" ]]; then
    SMOKE_CMD+=(--all)
  fi
  if [[ "${ZACUS_REQUIRE_HW:-0}" == "1" ]]; then
    SMOKE_CMD+=(--wait-port 180)
  else
    SMOKE_CMD+=(--wait-port "$default_wait_port" --allow-no-hardware)
  fi

  SMOKE_COMMAND_STRING="$(printf '%q ' "${SMOKE_CMD[@]}")"
  log_info "smoke command: $SMOKE_COMMAND_STRING"
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
    log_error "smoke step failed; rerun command above when hardware is ready"
    return 1
  fi
  set -e
  rm -f "$SMOKE_LOG"
  return 0
}

parse_envs
log_info "selected envs: ${ENVS[*]}"

if [[ "${ZACUS_SKIP_PIO:-0}" != "1" ]]; then
  require_cmd pio || exit 127
  choose_platformio_core_dir
fi

if [[ "${ZACUS_SKIP_SMOKE:-0}" != "1" ]]; then
  require_cmd python3 || exit 127
  ensure_pyserial || exit 12
fi

if [[ "${ZACUS_SKIP_PIO:-0}" == "1" ]]; then
  BUILD_STATUS="SKIPPED (ZACUS_SKIP_PIO=1)"
  log_step "build matrix skipped"
else
  SKIP_IF_BUILT="${ZACUS_SKIP_IF_BUILT:-1}"
  if [[ "${ZACUS_FORCE_BUILD:-0}" == "1" ]]; then
    log_step "build matrix forced"
    if run_build_matrix; then
      BUILD_STATUS="OK"
    else
      BUILD_STATUS="FAILED"
      exit 10
    fi
  elif [[ "$SKIP_IF_BUILT" == "1" ]] && all_builds_present; then
    BUILD_STATUS="SKIPPED (already built)"
    log_step "build matrix skipped (artifacts already present)"
  else
    log_step "build matrix running"
    if run_build_matrix; then
      BUILD_STATUS="OK"
    else
      BUILD_STATUS="FAILED"
      exit 10
    fi
  fi
fi

if [[ "${ZACUS_SKIP_SMOKE:-0}" == "1" ]]; then
  SMOKE_STATUS="SKIPPED (ZACUS_SKIP_SMOKE=1)"
  log_step "serial smoke skipped"
else
  echo
  print_usb_alert
  run_countdown
  log_step "serial smoke running"
  if run_smoke; then
    :
  else
    exit 20
  fi
fi

echo
echo "=== Run summary ==="
echo "Build status : $BUILD_STATUS"
if [[ -n "$SMOKE_COMMAND_STRING" ]]; then
  echo "Smoke status : $SMOKE_STATUS"
  echo "Smoke cmd    : $SMOKE_COMMAND_STRING"
else
  echo "Smoke status : $SMOKE_STATUS"
fi
