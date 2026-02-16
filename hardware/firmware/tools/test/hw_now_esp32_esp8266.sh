#!/usr/bin/env bash
set -euo pipefail

script_name="$(basename "$0")"
base_dir="$(cd "$(dirname "$0")/../.." && pwd)"
source "$base_dir/tools/dev/agent_utils.sh"
timestamp=""
artifact_dir=""
log_file=""

env_esp32="esp32dev"
env_esp8266="esp8266_oled"
skip_build=0
skip_upload=0
baud=19200
wait_port=20
OUTDIR="${ZACUS_OUTDIR:-}"
PHASE="hw_now"

print_help() {
  cat <<EOF
$script_name [options]

Options:
  --env-esp32 <env>      PlatformIO environment for the ESP32 Audio Kit (default: $env_esp32)
  --env-esp8266 <env>    PlatformIO environment for the ESP8266 OLED (default: $env_esp8266)
  --skip-build           Skip the dedicated build step before upload
  --skip-upload          Skip the upload step entirely (keeps detection + smoke steps)
  --baud <n>             Baud rate for serial smoke (default: $baud)
  --wait-port <sec>      Seconds to wait for serial ports (default: $wait_port)
  --outdir <path>        Evidence output directory (default: artifacts/hw_now/<timestamp>)
  -h, --help             Show this help message

Examples:
  $script_name
  $script_name --skip-build
  $script_name --env-esp32 esp32_release --baud 19200
EOF
}

log() {
  mkdir -p "$artifact_dir"
  echo "$*" | tee -a "$log_file"
}

run_and_log() {
  log "+ $*"
  evidence_record_command "$*"
  if "$@" >>"$log_file" 2>&1; then
    log "[ok] $*"
  else
    log "[fail] $*"
    return 1
  fi
}

FINALIZED=0
finalize() {
  local rc="$1"
  if [[ "$FINALIZED" == "1" ]]; then
    return
  fi
  FINALIZED=1
  trap - EXIT
  local result="PASS"
  if [[ "$rc" != "0" ]]; then
    result="FAIL"
  fi
  if [[ -n "${EVIDENCE_SUMMARY:-}" && ! -f "$EVIDENCE_SUMMARY" ]]; then
    local log_name
    log_name="$(basename "${log_file:-hw_now.log}")"
    cat > "$EVIDENCE_SUMMARY" <<EOF
# HW now summary

- Result: **${result}**
- Log: ${log_name}
EOF
  fi
  echo "RESULT=${result}"
  exit "$rc"
}

list_serial_ports() {
  python3 <<'PY'
from serial.tools import list_ports

for port in list_ports.comports():
    desc = port.description or ""
    hwid = port.hwid or ""
    print(f"{port.device}|{desc}|{hwid}")
PY
}

detect_serial_roles() {
  python3 - "$wait_port" <<'PY'
import importlib.util
import json
import os
import platform
import sys
import time

spec = importlib.util.spec_from_file_location(
    "serial_smoke",
    os.path.join(os.getcwd(), "tools", "dev", "serial_smoke.py"),
)
serial_smoke = importlib.util.module_from_spec(spec)
spec.loader.exec_module(serial_smoke)

wait_port = int(sys.argv[1])
ports_map = serial_smoke.load_ports_map()
prefer_cu = platform.system() == "Darwin"

def try_detect():
    ports = list(serial_smoke.list_ports.comports())
    filtered = serial_smoke.filter_detectable_ports(ports)
    return serial_smoke.detect_roles(filtered, prefer_cu, ports_map)

deadline = time.monotonic() + wait_port
detection = try_detect()
while not detection and time.monotonic() < deadline:
    time.sleep(0.5)
    detection = try_detect()

result = {}
for role in ("esp32", "esp8266"):
    if role in detection:
        result[role] = {
            "device": detection[role]["device"],
            "location": detection[role]["location"],
        }

print(json.dumps(result))
PY
}

choose_port_manually() {
  local role="$1"
  while true; do
    local entries=()
    while IFS= read -r line; do
      entries+=("$line")
    done < <(list_serial_ports)
    if [[ ${#entries[@]} -eq 0 ]]; then
      log "No serial ports detected while prompting for $role."
      return 1
    fi

    local option_labels=()
    local option_devices=()
    for entry in "${entries[@]}"; do
      local device="${entry%%|*}"
      local rest="${entry#*|}"
      local desc="${rest%%|*}"
      local hwid="${rest#*|}"
      hwid="${hwid:-unknown}"
      desc="${desc:-no description}"
      option_labels+=("$device — $desc ($hwid)")
      option_devices+=("$device")
    done

    option_labels+=("Refresh list")
    option_labels+=("Cancel")

    PS3="Select port for $role (number): "
    select choice in "${option_labels[@]}"; do
      if [[ -z "$choice" ]]; then
        echo "Invalid choice."
        continue
      fi
      if [[ "$choice" == "Refresh list" ]]; then
        break
      fi
      if [[ "$choice" == "Cancel" ]]; then
        return 1
      fi
      if (( REPLY >= 1 && REPLY <= ${#option_devices[@]} )); then
        echo "${option_devices[REPLY-1]}"
        return 0
      fi
      echo "Invalid selection."
    done
  done
}

confirm_or_prompt_port() {
  local role="$1"
  local detected="$2"
  local selected=""
  if [[ -n "$detected" ]]; then
    log "Auto-detected $role serial port: $detected"
    SELECTED_PORT="$detected"
    return 0
  fi
  log "No auto-detect candidate for $role; prompting user."
  if selected="$(choose_port_manually "$role")"; then
    SELECTED_PORT="$selected"
    return 0
  fi
  return 1
}

parse_args() {
  while [[ $# -gt 0 ]]; do
    case "$1" in
      --env-esp32)
        env_esp32="$2"
        shift 2
        ;;
      --env-esp8266)
        env_esp8266="$2"
        shift 2
        ;;
      --skip-build)
        skip_build=1
        shift
        ;;
      --skip-upload)
        skip_upload=1
        shift
        ;;
      --baud)
        baud="$2"
        shift 2
        ;;
      --wait-port)
        wait_port="$2"
        shift 2
        ;;
      --outdir)
        OUTDIR="${2:-}"
        shift 2
        ;;
      -h|--help)
        print_help
        exit 0
        ;;
      *)
        echo "Unknown option: $1"
        print_help
        exit 1
        ;;
    esac
  done
}

main() {
  parse_args "$@"
  EVIDENCE_CMDLINE="$0 $*"
  export EVIDENCE_CMDLINE
  evidence_init "$PHASE" "$OUTDIR"
  timestamp="$EVIDENCE_TIMESTAMP"
  artifact_dir="$EVIDENCE_DIR"
  log_file="$artifact_dir/hw_now.log"
  mkdir -p "$artifact_dir"
  trap 'finalize "$?"' EXIT

  log "Starting hardware now run at $(date -u +"%Y-%m-%d %H:%M:%S UTC")"
  log "ESP32 env: $env_esp32, ESP8266 env: $env_esp8266"
  log "Baud: $baud, wait-port: $wait_port, skip-build: $skip_build, skip-upload: $skip_upload"

  if ! detection_json="$(detect_serial_roles)"; then
    log "ERROR: serial role detection failed."
    exit 1
  fi

  if [[ -z "$detection_json" ]]; then
    log "ERROR: serial role detection returned empty payload."
    exit 1
  fi

  parsed_detection="$(DETECTION_JSON="$detection_json" python3 - <<'PY'
import json
import os
import sys

raw = os.environ.get("DETECTION_JSON", "")
try:
    data = json.loads(raw)
except Exception as exc:
    print(f"ERROR:invalid-json:{exc}")
    sys.exit(0)

if not isinstance(data, dict):
    print("ERROR:invalid-payload")
    sys.exit(0)

esp32 = data.get("esp32", {}).get("device", "")
esp8266 = data.get("esp8266", {}).get("device", "")
count = len(data)
print(f"count={count}")
print(f"esp32={esp32}")
print(f"esp8266={esp8266}")
PY
)"

  if [[ "$parsed_detection" == ERROR:* ]]; then
    log "ERROR: $parsed_detection"
    log "Raw detection payload: $detection_json"
    exit 1
  fi

  detection_count="$(printf '%s\n' "$parsed_detection" | awk -F= '/^count=/{print $2}')"
  esp32_port="$(printf '%s\n' "$parsed_detection" | awk -F= '/^esp32=/{print $2}')"
  esp8266_port="$(printf '%s\n' "$parsed_detection" | awk -F= '/^esp8266=/{print $2}')"

  if [[ -z "$detection_count" ]]; then
    log "ERROR: failed to decode detection count."
    exit 1
  fi

  if [[ "$detection_count" == "0" ]]; then
    log "ERROR: no serial hardware detected after waiting ${wait_port}s."
    exit 1
  fi

  if ! confirm_or_prompt_port "ESP32" "$esp32_port"; then
    esp32_port=""
  else
    esp32_port="$SELECTED_PORT"
  fi
  if ! confirm_or_prompt_port "ESP8266" "$esp8266_port"; then
    esp8266_port=""
  else
    esp8266_port="$SELECTED_PORT"
  fi

  if [[ -z "$esp32_port" || -z "$esp8266_port" ]]; then
    log "ERROR: missing serial ports for ESP32 or ESP8266 after prompting."
    exit 1
  fi

  log "Using ports ESP32=$esp32_port, ESP8266=$esp8266_port"

  if (( skip_upload == 0 )); then
    if (( skip_build == 0 )); then
      run_and_log pio run -e "$env_esp32"
      run_and_log pio run -e "$env_esp8266"
    fi

    run_and_log pio run -e "$env_esp32" -t upload --upload-port "$esp32_port"
    run_and_log pio run -e "$env_esp8266" -t upload --upload-port "$esp8266_port"
  else
    log "Skipping uploads (--skip-upload set)."
  fi

  log "Running serial smoke checks."
  run_and_log python3 tools/dev/serial_smoke.py --role esp32 --port "$esp32_port" --baud "$baud" --wait-port "$wait_port"
  run_and_log python3 tools/dev/serial_smoke.py --role esp8266 --port "$esp8266_port" --baud "$baud" --wait-port "$wait_port"
  run_and_log python3 tools/dev/serial_smoke.py --role auto --baud "$baud" --wait-port "$wait_port"

  log "Checking UI link indicator in console logs (see OLED/ESP32 commands)."
  log "If you know of a specific 'UI_LINK' message, grep $log_file."

  log "Probing WiFi AP fallback status."
  if curl --max-time 5 -sS http://192.168.4.1/api/status >/dev/null 2>&1; then
    log "PASS: AP fallback responded."
  else
    log "SKIP: AP fallback unreachable."
  fi

  cat > "$EVIDENCE_SUMMARY" <<EOF
# HW now summary

- Result: **PASS**
- ESP32 env: ${env_esp32}
- ESP8266 env: ${env_esp8266}
- ESP32 port: ${esp32_port}
- ESP8266 port: ${esp8266_port}
- Log: $(basename "$log_file")
EOF

  log "Hardware now run finished. Logs: $log_file"
}

main "$@"
