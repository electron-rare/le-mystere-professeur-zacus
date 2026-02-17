#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/agent_utils.sh"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
REPO_ROOT="$(cd "$ROOT/../.." && pwd)"
cd "$ROOT"

DEFAULT_ENVS=(esp32dev esp32_release esp8266_oled ui_rp2040_ili9488 ui_rp2040_ili9486)
BUILD_STATUS="SKIPPED"
PORT_STATUS="SKIPPED"
SMOKE_STATUS="SKIPPED"
UI_LINK_STATUS="SKIPPED"
SMOKE_COMMAND_STRING=""
SERIAL_MODULE_AVAILABLE="1"

TIMESTAMP="$(date -u +"%Y%m%d-%H%M%S")"
ARTIFACT_DIR="$ROOT/artifacts/rc_live/$TIMESTAMP"
LOG_DIR="$ROOT/logs"
RUN_LOG="$LOG_DIR/run_matrix_and_smoke_${TIMESTAMP}.log"
STEPS_TSV="$ARTIFACT_DIR/steps.tsv"
SUMMARY_JSON="$ARTIFACT_DIR/summary.json"
SUMMARY_MD="$ARTIFACT_DIR/summary.md"
PORTS_RESOLVE_JSON="$ARTIFACT_DIR/ports_resolve.json"
RESOLVE_PORTS_ALLOW_RETRY="0"

mkdir -p "$ARTIFACT_DIR" "$LOG_DIR"
: > "$RUN_LOG"
: > "$STEPS_TSV"

EXIT_CODE=0
FINALIZED=0
PORT_ESP32=""
PORT_ESP8266=""
RESOLVE_REASON_ESP32=""
RESOLVE_REASON_ESP8266=""
RESOLVE_LOCATION_ESP32=""
RESOLVE_LOCATION_ESP8266=""
PYTHON_BIN="python3"
ENVS=("${DEFAULT_ENVS[@]}")

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

should_record_resolve_failure() {
  [[ "${RESOLVE_PORTS_ALLOW_RETRY:-0}" != "1" ]]
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

run_build_env() {
  local env="$1"
  local log_file="$ARTIFACT_DIR/build_${env}.log"
  mkdir -p ".pio/build/${env}/src"
  if run_step_cmd "build_${env}" "$log_file" pio run -e "$env"; then
    return 0
  fi
  if grep -q "firmware.elf" "$log_file" 2>/dev/null; then
    log_warn "build_${env} missing firmware.elf; retrying once"
    if run_step_cmd "build_${env}_retry" "$log_file" pio run -e "$env"; then
      return 0
    fi
  fi
  return 1
}

list_ports_verbose() {
  echo "  [ports] available serial ports (best effort):"
  if ! python3 -m serial.tools.list_ports -v | tee -a "$RUN_LOG"; then
    echo "[warn] unable to list ports; install pyserial via ./tools/dev/bootstrap_local.sh"
  fi
}

emit_usb_warning() {
  local warning="$1"
  for _ in 1 2 3; do
    printf '\a%s\n' "$warning" | tee -a "$RUN_LOG"
  done
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

  if [[ "${ZACUS_REQUIRE_HW:-0}" == "1" ]]; then
    wait_for_required_usb
    return $?
  fi

  if [[ "${ZACUS_NO_COUNTDOWN:-0}" == "1" ]]; then
    log_info "USB wait gate skipped (ZACUS_NO_COUNTDOWN=1)"
  else
    print_usb_alert
    emit_usb_warning "$warning"
    list_ports_verbose

    if [[ -t 0 ]]; then
      log_info "press Enter once USB is connected (ports are listed every ${probe_every_s}s)"
      while true; do
        if read -r -t "$probe_every_s" -p "Press Enter to continue: " _; then
          echo | tee -a "$RUN_LOG"
          break
        fi
        echo | tee -a "$RUN_LOG"
        emit_usb_warning "$warning"
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
  fi
  if ! resolve_live_ports; then
    return 1
  fi
  log_info "USB confirmation complete"
  return 0
}


wait_for_required_usb() {
  local warning="⚠️ BRANCHE L’USB MAINTENANT ⚠️"
  local warn_interval=5
  local list_interval=15
  local max_wait="${ZACUS_WAIT_MAX_SECS:-0}"
  local start
  if ! [[ "$max_wait" =~ ^[0-9]+$ ]]; then
    max_wait=0
  fi

  print_usb_alert
  emit_usb_warning "$warning"
  list_ports_verbose
  start=$(date +%s)
  local last_warn="$start"
  local last_list="$start"
  local _prev_retry="${RESOLVE_PORTS_ALLOW_RETRY:-0}"
  RESOLVE_PORTS_ALLOW_RETRY="1"
  trap 'RESOLVE_PORTS_ALLOW_RETRY="${_prev_retry:-0}"' RETURN

  while true; do
    if resolve_live_ports && [[ "$PORT_STATUS" == "OK" && -n "$PORT_ESP32" && -n "$PORT_ESP8266" ]]; then
      echo "required ports detected"
      return 0
    fi
    local now
    now=$(date +%s)
    if [[ "$max_wait" -gt 0 ]] && (( now - start >= max_wait )); then
      log_warn "ZACUS_WAIT_MAX_SECS=${max_wait} reached; exiting USB wait"
      return 1
    fi
    if (( now - last_warn >= warn_interval )); then
      emit_usb_warning "$warning"
      log_warn "waiting for CP2102 adapters..."
      last_warn=$now
    fi
    if (( now - last_list >= list_interval )); then
      list_ports_verbose
      last_list=$now
    fi
    sleep 1
  done
}

resolve_live_ports() {
  local wait_port="${ZACUS_WAIT_PORT:-3}"
  local resolve_log="$ARTIFACT_DIR/resolve_ports.log"
  local require_hw="${ZACUS_REQUIRE_HW:-0}"

  if ! [[ "$wait_port" =~ ^[0-9]+$ ]]; then
    wait_port=3
  fi

  local resolver_cmd=(
    "$PYTHON_BIN"
    "$RESOLVER"
    "--wait-port" "$wait_port"
    "--auto-ports"
    "--need-esp32"
    "--need-esp8266"
  )

  if [[ "$(uname -s)" == "Darwin" ]]; then
    resolver_cmd+=("--prefer-cu")
  fi

  if [[ -n "${ZACUS_PORT_ESP32:-}" ]]; then
    resolver_cmd+=("--port-esp32" "${ZACUS_PORT_ESP32}")
  fi
  if [[ -n "${ZACUS_PORT_ESP8266:-}" ]]; then
    resolver_cmd+=("--port-esp8266" "${ZACUS_PORT_ESP8266}")
  fi
  if [[ -n "$PORTS_RESOLVE_JSON" ]]; then
    resolver_cmd+=("--ports-resolve-json" "$PORTS_RESOLVE_JSON")
  fi

  if [[ "$require_hw" != "1" ]]; then
    resolver_cmd+=("--allow-no-hardware")
  fi

  printf '[step] %s\n' "${resolver_cmd[*]}" | tee -a "$RUN_LOG" "$resolve_log"
  set +e
  local resolve_json
  resolve_json="$("${resolver_cmd[@]}" 2>>"$resolve_log")"
  local rc=$?
  set -e
  printf '%s\n' "$resolve_json" >> "$resolve_log"
  printf '%s\n' "$resolve_json" > "$PORTS_RESOLVE_JSON"

  if [[ "$rc" != "0" ]]; then
    PORT_STATUS="FAILED"
    append_step "resolve_ports" "FAIL" "$rc" "$resolve_log" "resolver command failed"
    if should_record_resolve_failure; then
      set_failure 21
    fi
    return 1
  fi

  eval "$(RESOLVE_JSON="$resolve_json" "$PYTHON_BIN" -c '
import json, os, shlex
v = json.loads(os.environ["RESOLVE_JSON"])
print("RESOLVE_STATUS=" + shlex.quote(str(v.get("status", "fail"))))
print("RESOLVE_PORT_ESP32=" + shlex.quote(str(v.get("ports", {}).get("esp32", ""))))
print("RESOLVE_PORT_ESP8266=" + shlex.quote(str(v.get("ports", {}).get("esp8266", ""))))
print("RESOLVE_REASON_ESP32=" + shlex.quote(str(v.get("reasons", {}).get("esp32", ""))))
print("RESOLVE_REASON_ESP8266=" + shlex.quote(str(v.get("reasons", {}).get("esp8266", ""))))
print("RESOLVE_LOCATION_ESP32=" + shlex.quote(str(v.get("details", {}).get("esp32", {}).get("location", ""))))
print("RESOLVE_LOCATION_ESP8266=" + shlex.quote(str(v.get("details", {}).get("esp8266", {}).get("location", ""))))
print("RESOLVE_NOTES=" + shlex.quote(" | ".join(v.get("notes", []))))
')"

  PORT_ESP32="$RESOLVE_PORT_ESP32"
  PORT_ESP8266="$RESOLVE_PORT_ESP8266"
  RESOLVE_REASON_ESP32="$RESOLVE_REASON_ESP32"
  RESOLVE_REASON_ESP8266="$RESOLVE_REASON_ESP8266"
  RESOLVE_LOCATION_ESP32="$RESOLVE_LOCATION_ESP32"
  RESOLVE_LOCATION_ESP8266="$RESOLVE_LOCATION_ESP8266"

  if [[ "$RESOLVE_STATUS" == "pass" ]]; then
    local location_guard=0
    local location_notes=()
    if [[ "$(uname -s)" == "Darwin" ]]; then
      local accept_reason='^(location-map:|learned-map:|fingerprint|usb-hint|manual-override)'
      if [[ -z "${ZACUS_PORT_ESP32:-}" && ! "$RESOLVE_REASON_ESP32" =~ $accept_reason ]]; then
        location_guard=1
        location_notes+=("esp32 reason=${RESOLVE_REASON_ESP32:-unknown} (need location-map/fingerprint)")
      fi
      if [[ -z "${ZACUS_PORT_ESP8266:-}" && ! "$RESOLVE_REASON_ESP8266" =~ $accept_reason ]]; then
        location_guard=1
        location_notes+=("esp8266 reason=${RESOLVE_REASON_ESP8266:-unknown} (need location-map/fingerprint)")
      fi
    fi

    if [[ "$location_guard" == "1" ]]; then
      PORT_ESP32=""
      PORT_ESP8266=""
      if [[ "$require_hw" == "1" ]]; then
        PORT_STATUS="FAILED"
        append_step "resolve_ports" "FAIL" "1" "$resolve_log" "$(IFS='; '; echo "${location_notes[*]}")"
        log_error "port resolution rejected: $(IFS='; '; echo "${location_notes[*]}")"
        if should_record_resolve_failure; then
          set_failure 21
        fi
        return 1
      fi
      PORT_STATUS="SKIP"
      append_step "resolve_ports" "SKIP" "0" "$resolve_log" "$(IFS='; '; echo "${location_notes[*]}")"
      log_warn "port resolution skipped: $(IFS='; '; echo "${location_notes[*]}")"
      return 0
    fi

    PORT_STATUS="OK"
    append_step "resolve_ports" "PASS" "0" "$resolve_log" "esp32=$PORT_ESP32 esp8266=$PORT_ESP8266"
    log_info "ESP32=${PORT_ESP32:-n/a} location=${RESOLVE_LOCATION_ESP32:-unknown} (${RESOLVE_REASON_ESP32:-n/a})"
    log_info "ESP8266=${PORT_ESP8266:-n/a} location=${RESOLVE_LOCATION_ESP8266:-unknown} (${RESOLVE_REASON_ESP8266:-n/a})"
    return 0
  fi

  if [[ "$RESOLVE_STATUS" == "skip" ]]; then
    PORT_STATUS="SKIP"
    append_step "resolve_ports" "SKIP" "0" "$resolve_log" "${RESOLVE_NOTES:-no hardware}"
    log_warn "port resolution skipped: ${RESOLVE_NOTES:-no hardware}"
    return 0
  fi

  PORT_STATUS="FAILED"
  append_step "resolve_ports" "FAIL" "1" "$resolve_log" "${RESOLVE_NOTES:-status=fail}"
  log_error "port resolution failed: ${RESOLVE_NOTES:-status=fail}"
  if should_record_resolve_failure; then
    set_failure 21
  fi
  return 1
}

run_role_smoke() {
  local role="$1"
  local port="$2"
  local log_file="$3"
  local wait_port="${ZACUS_WAIT_PORT:-3}"
  local timeout_s="${ZACUS_TIMEOUT:-1.0}"
  local baud="115200"

  if [[ ! "$wait_port" =~ ^[0-9]+$ ]]; then
    wait_port=3
  fi

  local cmd=("$PYTHON_BIN" "$SERIAL_SMOKE" --role "$role" --port "$port" --baud "$baud" --timeout "$timeout_s" --wait-port "$wait_port")
  printf '[step] %s\n' "${cmd[*]}" | tee -a "$RUN_LOG" "$log_file"
  set +e
  "${cmd[@]}" >>"$log_file" 2>&1
  local rc=$?
  set -e

  # Ajout monitor UI_LINK pour ESP8266
  if [[ "$role" == "esp8266_usb" ]]; then
    local monitor_log="$ARTIFACT_DIR/ui_link_monitor.log"
    local monitor_cmd=("$PYTHON_BIN" "$SERIAL_SMOKE" --role "$role" --port "$port" --baud "$baud" --timeout "7.0" --wait-port "$wait_port")
    printf '[step] monitor UI_LINK %s\n' "${monitor_cmd[*]}" | tee -a "$RUN_LOG" "$monitor_log"
    set +e
    "${monitor_cmd[@]}" >>"$monitor_log" 2>&1
    local monitor_rc=$?
    set -e
    if [[ "$monitor_rc" == "0" ]]; then
      append_step "ui_link_monitor" "PASS" "0" "$monitor_log" "UI_LINK monitor"
    else
      append_step "ui_link_monitor" "FAIL" "$monitor_rc" "$monitor_log" "UI_LINK monitor"
      set_failure 23
    fi
  fi

  if [[ "$rc" == "0" ]]; then
    append_step "smoke_${role}" "PASS" "0" "$log_file" "baud=$baud"
    return 0
  fi
  append_step "smoke_${role}" "FAIL" "$rc" "$log_file" "baud=$baud"
  return "$rc"
}

run_ui_link_check() {
  local ui_log="$ARTIFACT_DIR/ui_link.log"
  local wait_s="${ZACUS_UI_LINK_WAIT:-2}"

  if [[ -z "$PORT_ESP32" ]]; then
    UI_LINK_STATUS="SKIP"
    append_step "ui_link" "SKIP" "0" "$ui_log" "esp32 port unavailable"
    return 0
  fi

  UI_LINK_COMMAND="$PYTHON_BIN - <PORT_ESP32> UI_LINK_STATUS"
  if [[ "$wait_s" =~ ^[0-9]+$ ]] && (( wait_s > 0 )); then
    sleep "$wait_s"
  fi
  printf '[step] ui-link check on %s\n' "$PORT_ESP32" | tee -a "$RUN_LOG" "$ui_log"

set +e
"$PYTHON_BIN" - "$PORT_ESP32" >"$ui_log" 2>&1 <<'PY'
import re
import sys
import time

import serial

port = sys.argv[1]
line_seen = ""
connected = None
command = b"UI_LINK_STATUS\n"
deadline = time.time() + 14.0

try:
    ser = serial.Serial(port, 115200, timeout=0.5)
except Exception as exc:
    print(f"serial open failed: {exc}")
    raise SystemExit(4)

try:
    while time.time() < deadline:
        ser.reset_input_buffer()
        ser.write(command)
        attempt_deadline = time.time() + 3.0
        while time.time() < attempt_deadline:
            line = ser.readline().decode("utf-8", errors="ignore").strip()
            if not line:
                continue
            print(line)
            if "UI_LINK_STATUS" not in line:
                continue
            line_seen = line
            match = re.search(r"connected=(\d)", line)
            if match:
                connected = int(match.group(1))
            if connected == 1:
                break
        if connected == 1:
            break
        time.sleep(0.5)
finally:
    ser.close()

if not line_seen:
    print("UI_LINK_STATUS missing")
    raise SystemExit(2)
if connected != 1:
    print("UI_LINK_STATUS connected=0")
    raise SystemExit(3)
raise SystemExit(0)
PY
  local rc=$?
set -e

  if [[ "$rc" == "0" ]]; then
    UI_LINK_STATUS="OK"
    append_step "ui_link" "PASS" "0" "$ui_log" "connected=1"
    return 0
  fi

  UI_LINK_STATUS="FAILED"
  if [[ "$rc" == "3" ]]; then
    append_step "ui_link" "FAIL" "$rc" "$ui_log" "connected=0"
  else
    append_step "ui_link" "FAIL" "$rc" "$ui_log" "status unavailable"
  fi
  return "$rc"
}

run_story_screen_smoke() {
  local log_file="$ARTIFACT_DIR/story_screen_smoke.log"

  if [[ "${ZACUS_SKIP_SCREEN_CHECK:-0}" == "1" ]]; then
    append_step "story_screen" "SKIP" "0" "$log_file" "ZACUS_SKIP_SCREEN_CHECK=1"
    log_step "story screen smoke skipped"
    return 0
  fi

  if [[ -z "$PORT_ESP32" ]]; then
    append_step "story_screen" "SKIP" "0" "$log_file" "esp32 port unavailable"
    return 0
  fi

  if [[ "$UI_LINK_STATUS" != "OK" ]]; then
    append_step "story_screen" "SKIP" "0" "$log_file" "ui_link status=$UI_LINK_STATUS"
    return 0
  fi

  local cmd=("$PYTHON_BIN" "$STORY_SCREEN_SMOKE" --port "$PORT_ESP32" --baud 115200)
  printf '[step] %s\n' "${cmd[*]}" | tee -a "$RUN_LOG" "$log_file"
  set +e
  "${cmd[@]}" >>"$log_file" 2>&1
  local rc=$?
  set -e

  if [[ "$rc" == "0" ]]; then
    append_step "story_screen" "PASS" "0" "$log_file" "screen scene changed"
    return 0
  fi
  append_step "story_screen" "FAIL" "$rc" "$log_file" "screen scene unchanged"
  return "$rc"
}

write_summary() {
  local rc="$1"
  "$PYTHON_BIN" - "$ARTIFACT_DIR" "$STEPS_TSV" "$ARTIFACT_DIR/summary.json" "$ARTIFACT_DIR/summary.md" "$TIMESTAMP" "$BUILD_STATUS" "$PORT_STATUS" "$SMOKE_STATUS" "$UI_LINK_STATUS" "$SMOKE_COMMAND_STRING" "$PORT_ESP32" "$PORT_ESP8266" "$PORTS_RESOLVE_JSON" "$rc" "$RUN_LOG" <<'PY'
import json
import sys
from pathlib import Path

artifact_dir = Path(sys.argv[1])
steps_tsv = Path(sys.argv[2])
summary_json = Path(sys.argv[3])
summary_md = Path(sys.argv[4])
timestamp = sys.argv[5]
build_status = sys.argv[6]
port_status = sys.argv[7]
smoke_status = sys.argv[8]
ui_link_status = sys.argv[9]
smoke_cmd = sys.argv[10]
port_esp32 = sys.argv[11]
port_esp8266 = sys.argv[12]
ports_json = Path(sys.argv[13])
exit_code = int(sys.argv[14])
run_log = sys.argv[15]

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

ports_resolve = {}
if ports_json.exists():
    try:
        ports_resolve = json.loads(ports_json.read_text(encoding="utf-8"))
    except Exception:
        ports_resolve = {}

dry_run = any(
    step["status"] == "SKIP"
    and step["name"].startswith(("build", "upload", "smoke", "gate"))
    and any(token in step["details"].lower() for token in ("dry-run", "zacus_skip_pio=1", "zacus_skip_smoke=1"))
    for step in steps
)

if any(step["status"] == "FAIL" for step in steps) or exit_code != 0:
    result = "FAIL"
elif dry_run:
  result = "SKIP"
elif any(step["status"] == "PASS" for step in steps):
    result = "PASS"
else:
    result = "SKIP"

ports_payload = ports_resolve.get("ports", {}) if isinstance(ports_resolve, dict) else {}
esp32_payload = ports_payload.get("esp32")
if isinstance(esp32_payload, dict):
  esp32_payload = esp32_payload.get("port")
resolved_esp32 = esp32_payload or port_esp32
esp8266_payload = ports_payload.get("esp8266_usb") or ports_payload.get("esp8266")
if isinstance(esp8266_payload, dict):
  esp8266_payload = esp8266_payload.get("port")
resolved_esp8266 = esp8266_payload or port_esp8266
port_note = ""
if not resolved_esp32 or not resolved_esp8266:
  if ports_resolve.get("status") in ("pass", "ok"):
    port_note = "resolver ok but ports missing"
  elif ports_resolve:
    port_note = "resolver data missing"
  else:
    port_note = "ports_resolve.json missing"

summary = {
    "timestamp": timestamp,
    "result": result,
    "exit_code": exit_code,
    "build_status": build_status,
    "port_status": port_status,
    "smoke_status": smoke_status,
    "ui_link_status": ui_link_status,
    "smoke_command": smoke_cmd,
    "ports": {
        "esp32": {
        "port": resolved_esp32 or "",
            "location": ports_resolve.get("details", {}).get("esp32", {}).get("location", ""),
            "reason": ports_resolve.get("details", {}).get("esp32", {}).get("reason", ""),
        },
        "esp8266_usb": {
        "port": resolved_esp8266 or "",
            "location": ports_resolve.get("details", {}).get("esp8266", {}).get("location", ""),
            "reason": ports_resolve.get("details", {}).get("esp8266", {}).get("reason", ""),
        },
    },
    "ports_note": port_note,
    "steps": steps,
    "logs": {
        "run_log": run_log,
        "ports_resolve_json": str(ports_json),
    },
}
summary_json.write_text(json.dumps(summary, indent=2), encoding="utf-8")

rows = [
    "# RC live summary",
    "",
    f"- Result: **{result}**",
    *( ["- Mode: `DRY-RUN`"] if dry_run else [] ),
    f"- Exit code: `{exit_code}`",
    f"- Build status: `{build_status}`",
    f"- Port status: `{port_status}`",
    f"- Smoke status: `{smoke_status}`",
    f"- UI link status: `{ui_link_status}`",
    f"- ESP32 port: `{resolved_esp32 or 'n/a'}`",
    f"- ESP8266 USB port: `{resolved_esp8266 or 'n/a'}`",
    *( [f"- Port detail: `{port_note}`"] if port_note else [] ),
    f"- Run log: `{Path(run_log).name}`",
    "",
    "| Step | Status | Exit | Log | Details |",
    "|---|---|---:|---|---|",
]
for step in steps:
  log_name = Path(step["log_file"]).name
  if step["name"] == "resolve_ports" and log_name == "ports_resolve.json":
    log_name = "resolve_ports.log"
  rows.append(
    f"| {step['name']} | {step['status']} | {step['exit_code']} | `{log_name}` | {step['details']} |"
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
  if [[ -f "$RUN_LOG" ]]; then
    cp "$RUN_LOG" "$ARTIFACT_DIR/run_matrix_and_smoke.log"
  else
    log_warn "log absent: $RUN_LOG"
  fi
  action_log "[done] summary: $ARTIFACT_DIR/summary.json"
  action_log "[done] summary: $ARTIFACT_DIR/summary.md"
  exit "$rc"
}

trap 'finalize "$EXIT_CODE"' EXIT


parse_envs
log_info "selected envs: ${ENVS[*]}"
log_info "artifacts: $ARTIFACT_DIR"

if [[ -x ".venv/bin/python" ]]; then
  PYTHON_BIN=".venv/bin/python"
else
  PYTHON_BIN="python3"
fi

SERIAL_SMOKE="$ROOT/tools/dev/serial_smoke.py"
STORY_SCREEN_SMOKE="$ROOT/tools/dev/story_screen_smoke.py"
RESOLVER="$REPO_ROOT/tools/test/resolve_ports.py"

if [[ "${ZACUS_SKIP_PIO:-0}" != "1" ]]; then
  require_cmd pio || exit "$EXIT_CODE"
  choose_platformio_core_dir
fi

if [[ "${ZACUS_SKIP_SMOKE:-0}" != "1" ]]; then
  require_cmd python3 || exit "$EXIT_CODE"
  activate_local_venv_if_present
  if ! python3 -c 'import serial' 2>/dev/null; then
    log_warn "pyserial missing; smoke/port gates will be skipped"
    SERIAL_MODULE_AVAILABLE="0"
  fi
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
      if ! run_build_env "$env"; then
        BUILD_STATUS="FAILED"
        set_failure 10
        break
      fi
    done
  elif [[ "$SKIP_IF_BUILT" == "1" ]] && all_builds_present; then
    BUILD_STATUS="SKIPPED"
    append_step "build_matrix" "PASS" "0" "$ARTIFACT_DIR/build_matrix.log" "artifacts already present (cached)"
    log_step "build matrix skipped (artifacts already present)"
  else
    log_step "build matrix running"
    BUILD_STATUS="OK"
    for env in "${ENVS[@]}"; do
      if ! run_build_env "$env"; then
        BUILD_STATUS="FAILED"
        set_failure 10
        break
      fi
    done
  fi
fi

if [[ "${ZACUS_SKIP_SMOKE:-0}" == "1" ]]; then
  SMOKE_STATUS="SKIPPED"
  UI_LINK_STATUS="SKIPPED"
  PORT_STATUS="SKIPPED"
  append_step "resolve_ports" "SKIP" "0" "$ARTIFACT_DIR/resolve_ports.log" "ZACUS_SKIP_SMOKE=1"
  append_step "smoke_esp32" "SKIP" "0" "$ARTIFACT_DIR/smoke_esp32.log" "ZACUS_SKIP_SMOKE=1"
  append_step "smoke_esp8266_usb" "SKIP" "0" "$ARTIFACT_DIR/smoke_esp8266_usb.log" "ZACUS_SKIP_SMOKE=1"
  append_step "ui_link" "SKIP" "0" "$ARTIFACT_DIR/ui_link.log" "ZACUS_SKIP_SMOKE=1"
  append_step "story_screen" "SKIP" "0" "$ARTIFACT_DIR/story_screen_smoke.log" "ZACUS_SKIP_SMOKE=1"
  log_step "serial smoke skipped"
elif [[ "$SERIAL_MODULE_AVAILABLE" != "1" ]]; then
  SMOKE_STATUS="SKIPPED"
  UI_LINK_STATUS="SKIPPED"
  PORT_STATUS="SKIPPED"
  append_step "resolve_ports" "SKIP" "0" "$ARTIFACT_DIR/resolve_ports.log" "pyserial missing"
  append_step "smoke_esp32" "SKIP" "0" "$ARTIFACT_DIR/smoke_esp32.log" "pyserial missing"
  append_step "smoke_esp8266_usb" "SKIP" "0" "$ARTIFACT_DIR/smoke_esp8266_usb.log" "pyserial missing"
  append_step "ui_link" "SKIP" "0" "$ARTIFACT_DIR/ui_link.log" "pyserial missing"
  append_step "story_screen" "SKIP" "0" "$ARTIFACT_DIR/story_screen_smoke.log" "pyserial missing"
  log_warn "serial tooling unavailable"
else
    port_loop_success=1
  if [[ "${ZACUS_REQUIRE_HW:-0}" == "1" ]]; then
    port_loop_success=0
    while true; do
      echo
      log_step "port resolution"
      if ! wait_for_usb_confirmation; then
        set_failure 21
        break
      fi
      if [[ "$PORT_STATUS" != "OK" || -z "$PORT_ESP32" || -z "$PORT_ESP8266" ]]; then
        log_warn "ports unresolved; retrying"
        continue
      fi
      if [[ ! -e "$PORT_ESP32" || ! -e "$PORT_ESP8266" ]]; then
        log_warn "required port disappeared; re-waiting"
        continue
      fi
      port_loop_success=1
      break
    done
    if [[ "$port_loop_success" == "0" ]]; then
      PORT_STATUS="FAILED"
      log_warn "port resolution aborted"
    fi
  else
    echo
    log_step "port resolution"
    if ! wait_for_usb_confirmation; then
      set_failure 21
    fi
  fi

  if [[ "$PORT_STATUS" == "OK" && "$port_loop_success" == "1" ]]; then
    local_smoke_fail=0
    SMOKE_COMMAND_STRING="$PYTHON_BIN tools/dev/serial_smoke.py --role <esp32|esp8266_usb> --port <resolved> --baud 115200"

    if ! run_role_smoke "esp32" "$PORT_ESP32" "$ARTIFACT_DIR/smoke_esp32.log"; then
      local_smoke_fail=1
    fi
    if ! run_role_smoke "esp8266_usb" "$PORT_ESP8266" "$ARTIFACT_DIR/smoke_esp8266_usb.log"; then
      local_smoke_fail=1
    fi

    if [[ "$local_smoke_fail" == "1" ]]; then
      SMOKE_STATUS="FAILED"
      set_failure 22
    else
      SMOKE_STATUS="OK"
    fi

    if ! run_ui_link_check; then
      set_failure 23
    fi

    if ! run_story_screen_smoke; then
      set_failure 24
    fi
  else
    SMOKE_STATUS="SKIPPED"
    UI_LINK_STATUS="SKIPPED"
    append_step "smoke_esp32" "SKIP" "0" "$ARTIFACT_DIR/smoke_esp32.log" "port resolution $PORT_STATUS"
    append_step "smoke_esp8266_usb" "SKIP" "0" "$ARTIFACT_DIR/smoke_esp8266_usb.log" "port resolution $PORT_STATUS"
    append_step "ui_link" "SKIP" "0" "$ARTIFACT_DIR/ui_link.log" "port resolution $PORT_STATUS"
    append_step "story_screen" "SKIP" "0" "$ARTIFACT_DIR/story_screen_smoke.log" "port resolution $PORT_STATUS"
    if [[ "$PORT_STATUS" == "FAILED" ]]; then
      set_failure 21
    fi
  fi
fi


log "=== Run summary ==="
log "Build status : $BUILD_STATUS"
log "Port status  : $PORT_STATUS"
log "Smoke status : $SMOKE_STATUS"
log "UI link      : $UI_LINK_STATUS"
if [[ -n "$SMOKE_COMMAND_STRING" ]]; then
  log "Smoke cmd    : $SMOKE_COMMAND_STRING"
fi


# Audit Codex et hook génération automatique


artefact_gate "$ARTIFACT_DIR" "artifacts/rc_live/${TIMESTAMP}_agent"
logs_gate "$LOG_DIR" "artifacts/rc_live/${TIMESTAMP}_logs"

prune_rc_live_runs "$ROOT/artifacts/rc_live" "${ZACUS_RC_KEEP_RUNS:-2}"

exit "$EXIT_CODE"
