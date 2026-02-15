#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

DEFAULT_ENVS=(esp32dev esp32_release esp8266_oled ui_rp2040_ili9488 ui_rp2040_ili9486)
BUILD_STATUS="SKIPPED"
SMOKE_STATUS="SKIPPED"
SMOKE_COMMAND_STRING=""

TIMESTAMP="$(date -u +"%Y%m%d-%H%M%S")"
ARTIFACT_DIR="$ROOT/artifacts/rc_live/$TIMESTAMP"
LOG_DIR="$ROOT/logs"
RUN_LOG="$LOG_DIR/run_matrix_and_smoke_${TIMESTAMP}.log"
STEPS_TSV="$ARTIFACT_DIR/steps.tsv"
SUMMARY_JSON="$ARTIFACT_DIR/summary.json"
SUMMARY_MD="$ARTIFACT_DIR/summary.md"

mkdir -p "$ARTIFACT_DIR" "$LOG_DIR"
: > "$RUN_LOG"
: > "$STEPS_TSV"

EXIT_CODE=0
FINALIZED=0

action_log() {
  local msg="$1"
  echo "$msg" | tee -a "$RUN_LOG"
}

log_step() {
  action_log "[step] $*"
}

log_info() {
  action_log "[info] $*"
}

log_warn() {
  action_log "[warn] $*"
}

log_error() {
  action_log "[error] $*"
}

append_step() {
  local name="$1"
  local status="$2"
  local code="$3"
  local log_file="$4"
  local details="$5"
  printf '%s\t%s\t%s\t%s\t%s\n' "$name" "$status" "$code" "$log_file" "$details" >> "$STEPS_TSV"
}

set_failure() {
  local rc="$1"
  if [[ "$EXIT_CODE" == "0" ]]; then
    EXIT_CODE="$rc"
  fi
}

require_cmd() {
  local cmd="$1"
  if ! command -v "$cmd" >/dev/null 2>&1; then
    log_error "missing command: $cmd"
    set_failure 127
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
    return
  fi
  if [[ -f ".venv/bin/activate" ]]; then
    # shellcheck disable=SC1091
    source .venv/bin/activate
    log_info "using local venv: .venv"
  fi
}

ensure_pyserial() {
  if python3 -c "import serial" >/dev/null 2>&1; then
    return
  fi
  activate_local_venv_if_present
  if python3 -c "import serial" >/dev/null 2>&1; then
    return
  fi
  log_error "pyserial is missing. Run ./tools/dev/bootstrap_local.sh first."
  set_failure 12
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

run_step_cmd() {
  local step_name="$1"
  local log_file="$2"
  shift 2
  mkdir -p "$(dirname "$log_file")"
  printf '[step] %s\n' "$*" | tee -a "$RUN_LOG" "$log_file"
  set +e
  "$@" >>"$log_file" 2>&1
  local rc=$?
  set -e
  if [[ "$rc" == "0" ]]; then
    append_step "$step_name" "PASS" "0" "$log_file" ""
    return 0
  fi
  append_step "$step_name" "FAIL" "$rc" "$log_file" ""
  return "$rc"
}

list_ports_verbose() {
  action_log "  [ports] available serial ports (best effort):"
  if ! python3 -m serial.tools.list_ports -v | tee -a "$RUN_LOG"; then
    log_warn "unable to list ports; install pyserial via ./tools/dev/bootstrap_local.sh"
  fi
}

print_usb_alert() {
  cat <<'EOM' | tee -a "$RUN_LOG"
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

Confirm USB connection before smoke starts.
EOM
}

wait_for_usb_confirmation() {
  local warning="⚠️ BRANCHE L’USB MAINTENANT ⚠️"
  local probe_every_s=15

  if [[ "${ZACUS_NO_COUNTDOWN:-0}" == "1" ]]; then
    log_info "USB wait gate skipped (ZACUS_NO_COUNTDOWN=1)"
    return
  fi

  print_usb_alert
  for _ in 1 2 3; do
    printf '\a%s\n' "$warning" | tee -a "$RUN_LOG"
  done
  list_ports_verbose

  if [[ -t 0 ]]; then
    log_info "press Enter once USB is connected (ports are listed every ${probe_every_s}s)"
    while true; do
      if read -r -t "$probe_every_s" -p "Press Enter to continue: " _; then
        echo | tee -a "$RUN_LOG"
        break
      fi
      echo | tee -a "$RUN_LOG"
      printf '\a%s\n' "$warning" | tee -a "$RUN_LOG"
      list_ports_verbose
    done
  else
    if read -r -t 1 _; then
      log_info "USB confirmation received from stdin"
    else
      log_warn "stdin is non-interactive; waiting ${probe_every_s}s then continuing"
      sleep "$probe_every_s"
      list_ports_verbose
    fi
  fi

  log_info "USB confirmation complete"
}

run_smoke_auto() {
  local default_wait_port="${ZACUS_WAIT_PORT:-3}"
  local smoke_role="${ZACUS_SMOKE_ROLE:-auto}"
  local smoke_baud="${ZACUS_BAUD:-115200}"
  local smoke_timeout="${ZACUS_TIMEOUT:-1.0}"
  local smoke_log="$ARTIFACT_DIR/smoke_auto.log"

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
  set +e
  "${SMOKE_CMD[@]}" 2>&1 | tee "$smoke_log" | tee -a "$RUN_LOG"
  local rc=${PIPESTATUS[0]}
  set -e

  if [[ "$rc" == "0" ]]; then
    if grep -q "SKIP: no hardware detected" "$smoke_log"; then
      append_step "smoke_auto" "SKIP" "0" "$smoke_log" "no hardware"
      SMOKE_STATUS="SKIP"
    else
      append_step "smoke_auto" "PASS" "0" "$smoke_log" ""
      SMOKE_STATUS="OK"
    fi
    return 0
  fi

  append_step "smoke_auto" "FAIL" "$rc" "$smoke_log" ""
  SMOKE_STATUS="FAILED"
  return "$rc"
}

write_summary() {
  local rc="$1"
  "$PYTHON_BIN" - "$ARTIFACT_DIR" "$STEPS_TSV" "$SUMMARY_JSON" "$SUMMARY_MD" "$TIMESTAMP" "$BUILD_STATUS" "$SMOKE_STATUS" "$SMOKE_COMMAND_STRING" "$rc" "$RUN_LOG" <<'PY'
import json
import sys
from pathlib import Path

artifact_dir = Path(sys.argv[1])
steps_tsv = Path(sys.argv[2])
summary_json = Path(sys.argv[3])
summary_md = Path(sys.argv[4])
timestamp = sys.argv[5]
build_status = sys.argv[6]
smoke_status = sys.argv[7]
smoke_cmd = sys.argv[8]
exit_code = int(sys.argv[9])
run_log = sys.argv[10]

steps = []
if steps_tsv.exists():
    for raw in steps_tsv.read_text(encoding="utf-8").splitlines():
        if not raw.strip():
            continue
        name, status, step_code, log_file, details = raw.split("\t", 4)
        steps.append(
            {
                "name": name,
                "status": status,
                "exit_code": int(step_code),
                "log_file": log_file,
                "details": details,
            }
        )

if any(step["status"] == "FAIL" for step in steps) or exit_code != 0:
    result = "FAIL"
elif any(step["status"] == "PASS" for step in steps):
    result = "PASS"
else:
    result = "SKIP"

summary = {
    "timestamp": timestamp,
    "result": result,
    "exit_code": exit_code,
    "build_status": build_status,
    "smoke_status": smoke_status,
    "smoke_command": smoke_cmd,
    "steps": steps,
    "logs": {
        "run_log": run_log,
    },
}
summary_json.write_text(json.dumps(summary, indent=2), encoding="utf-8")

rows = [
    "# RC live summary",
    "",
    f"- Result: **{result}**",
    f"- Exit code: `{exit_code}`",
    f"- Build status: `{build_status}`",
    f"- Smoke status: `{smoke_status}`",
    f"- Run log: `{Path(run_log).name}`",
    "",
    "| Step | Status | Exit | Log | Details |",
    "|---|---|---:|---|---|",
]
for step in steps:
    rows.append(
        f"| {step['name']} | {step['status']} | {step['exit_code']} | `{Path(step['log_file']).name}` | {step['details']} |"
    )
summary_md.write_text("\n".join(rows) + "\n", encoding="utf-8")
PY
}

finalize() {
  local rc="$1"
  if [[ "$FINALIZED" == "1" ]]; then
    return
  fi
  FINALIZED=1
  trap - EXIT
  write_summary "$rc"
  cp "$RUN_LOG" "$ARTIFACT_DIR/run_matrix_and_smoke.log"
  action_log "[done] summary: $SUMMARY_JSON"
  action_log "[done] summary: $SUMMARY_MD"
  exit "$rc"
}

trap 'finalize "$?"' EXIT

parse_envs
log_info "selected envs: ${ENVS[*]}"
log_info "artifacts: $ARTIFACT_DIR"

if [[ -x ".venv/bin/python" ]]; then
  PYTHON_BIN=".venv/bin/python"
else
  PYTHON_BIN="python3"
fi

if [[ "${ZACUS_SKIP_PIO:-0}" != "1" ]]; then
  require_cmd pio || exit "$EXIT_CODE"
  choose_platformio_core_dir
fi

if [[ "${ZACUS_SKIP_SMOKE:-0}" != "1" ]]; then
  require_cmd python3 || exit "$EXIT_CODE"
  ensure_pyserial || exit "$EXIT_CODE"
fi

if [[ "${ZACUS_SKIP_PIO:-0}" == "1" ]]; then
  BUILD_STATUS="SKIPPED"
  append_step "build_matrix" "SKIP" "0" "$ARTIFACT_DIR/build_matrix.log" "ZACUS_SKIP_PIO=1"
  log_step "build matrix skipped"
else
  SKIP_IF_BUILT="${ZACUS_SKIP_IF_BUILT:-1}"
  if [[ "${ZACUS_FORCE_BUILD:-0}" == "1" ]]; then
    log_step "build matrix forced"
    BUILD_STATUS="OK"
    for env in "${ENVS[@]}"; do
      if ! run_step_cmd "build_${env}" "$ARTIFACT_DIR/build_${env}.log" pio run -e "$env"; then
        BUILD_STATUS="FAILED"
        set_failure 10
        break
      fi
    done
  elif [[ "$SKIP_IF_BUILT" == "1" ]] && all_builds_present; then
    BUILD_STATUS="SKIPPED"
    append_step "build_matrix" "SKIP" "0" "$ARTIFACT_DIR/build_matrix.log" "artifacts already present"
    log_step "build matrix skipped (artifacts already present)"
  else
    log_step "build matrix running"
    BUILD_STATUS="OK"
    for env in "${ENVS[@]}"; do
      if ! run_step_cmd "build_${env}" "$ARTIFACT_DIR/build_${env}.log" pio run -e "$env"; then
        BUILD_STATUS="FAILED"
        set_failure 10
        break
      fi
    done
  fi
fi

if [[ "${ZACUS_SKIP_SMOKE:-0}" == "1" ]]; then
  SMOKE_STATUS="SKIPPED"
  append_step "smoke_auto" "SKIP" "0" "$ARTIFACT_DIR/smoke_auto.log" "ZACUS_SKIP_SMOKE=1"
  log_step "serial smoke skipped"
else
  echo
  wait_for_usb_confirmation
  log_step "serial smoke running"
  if ! run_smoke_auto; then
    set_failure 20
    log_error "smoke step failed; inspect $ARTIFACT_DIR/smoke_auto.log"
  fi
fi

echo | tee -a "$RUN_LOG"
action_log "=== Run summary ==="
action_log "Build status : $BUILD_STATUS"
action_log "Smoke status : $SMOKE_STATUS"
if [[ -n "$SMOKE_COMMAND_STRING" ]]; then
  action_log "Smoke cmd    : $SMOKE_COMMAND_STRING"
fi

exit "$EXIT_CODE"
