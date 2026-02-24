#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=tools/dev/agent_utils.sh
source "$SCRIPT_DIR/agent_utils.sh"

require_cmd curl
require_cmd python3

FW_ROOT="$(get_fw_root)"
PHASE="rtos_wifi_health"
OUTDIR="${ZACUS_OUTDIR:-}"
SERIAL_DEBUG=0
SERIAL_DEBUG_SECONDS=600
SERIAL_DEBUG_REGEX="(wifi|wlan|ssid|rssi|disconnect|reconnect|dhcp|ip=|got ip|sta|ap)"
SERIAL_ROLE="esp32"
while [[ $# -gt 0 ]]; do
  case "$1" in
    --outdir)
      OUTDIR="${2:-}"
      shift 2
      ;;
    --serial-debug)
      SERIAL_DEBUG=1
      shift
      ;;
    --serial-debug-seconds)
      SERIAL_DEBUG_SECONDS="${2:-600}"
      shift 2
      ;;
    --serial-debug-regex)
      SERIAL_DEBUG_REGEX="${2:-}"
      shift 2
      ;;
    -h|--help)
      echo "Usage: ./tools/dev/rtos_wifi_health.sh [--outdir <path>] [--serial-debug] [--serial-debug-seconds <sec>] [--serial-debug-regex <regex>]"
      exit 0
      ;;
    *)
      echo "[error] unknown option: $1" >&2
      echo "Usage: ./tools/dev/rtos_wifi_health.sh [--outdir <path>] [--serial-debug] [--serial-debug-seconds <sec>] [--serial-debug-regex <regex>]" >&2
      exit 2
      ;;
  esac
done
EVIDENCE_CMDLINE="$0 $*"
export EVIDENCE_CMDLINE
evidence_init "$PHASE" "$OUTDIR"

STAMP="$EVIDENCE_TIMESTAMP"
ARTIFACT="$EVIDENCE_DIR/rtos_wifi_health.log"
SERIAL_DEBUG_LOG="$EVIDENCE_DIR/wifi_serial_debug.log"
ESP_URL="${ESP_URL:-http://192.168.1.100:8080}"
CURL_AUTH_ARGS=()
CURL_AUTH_NOTE=""
if [[ -n "${ZACUS_WEB_TOKEN:-}" ]]; then
  CURL_AUTH_ARGS=(-H "Authorization: Bearer ${ZACUS_WEB_TOKEN}")
  CURL_AUTH_NOTE=" -H 'Authorization: Bearer ***'"
fi

mkdir -p "$EVIDENCE_DIR"
EXIT_CODE=0

{
  echo "[RTOS_WIFI_HEALTH] timestamp=${STAMP}"
  echo "[RTOS_WIFI_HEALTH] url=${ESP_URL}"
  echo ""
  echo "== GET /api/status =="
  evidence_record_command "curl -sS -m 5${CURL_AUTH_NOTE} ${ESP_URL}/api/status"
  if ! curl -sS -m 5 "${CURL_AUTH_ARGS[@]}" "${ESP_URL}/api/status"; then
    echo "FAIL"
    EXIT_CODE=1
  fi
  echo ""
  echo "== GET /api/wifi =="
  evidence_record_command "curl -sS -m 5${CURL_AUTH_NOTE} ${ESP_URL}/api/wifi"
  if ! curl -sS -m 5 "${CURL_AUTH_ARGS[@]}" "${ESP_URL}/api/wifi"; then
    echo "FAIL"
    EXIT_CODE=1
  fi
  echo ""
  echo "== GET /api/rtos =="
  evidence_record_command "curl -sS -m 5${CURL_AUTH_NOTE} ${ESP_URL}/api/rtos"
  if ! curl -sS -m 5 "${CURL_AUTH_ARGS[@]}" "${ESP_URL}/api/rtos"; then
    echo "FAIL"
    EXIT_CODE=1
  fi
  echo ""
  echo "== GET /api/story/status =="
  evidence_record_command "curl -sS -m 5${CURL_AUTH_NOTE} ${ESP_URL}/api/story/status"
  if ! curl -sS -m 5 "${CURL_AUTH_ARGS[@]}" "${ESP_URL}/api/story/status"; then
    echo "FAIL"
    EXIT_CODE=1
  fi
} | tee "$ARTIFACT"

if [[ "$SERIAL_DEBUG" == "1" || "${ZACUS_WIFI_SERIAL_DEBUG:-0}" == "1" ]]; then
  log "[step] wifi serial debug"
  ports_json="$EVIDENCE_DIR/ports_resolve.json"
  wait_secs="${ZACUS_PORT_WAIT:-5}"
  resolver="$FW_ROOT/tools/test/resolve_ports.py"
  args=("--auto-ports" "--need-${SERIAL_ROLE}" "--wait-port" "$wait_secs" "--ports-resolve-json" "$ports_json")
  if [[ -n "${ZACUS_PORT_ESP32:-}" ]]; then
    args+=("--port-esp32" "$ZACUS_PORT_ESP32")
  fi
  if [[ -n "${ZACUS_PORT_ESP8266:-}" ]]; then
    args+=("--port-esp8266" "$ZACUS_PORT_ESP8266")
  fi
  if [[ -n "${ZACUS_PORT_RP2040:-}" ]]; then
    args+=("--port-rp2040" "$ZACUS_PORT_RP2040")
  fi

  evidence_record_command "python3 $resolver ${args[*]}"
  if ! python3 "$resolver" "${args[@]}"; then
    echo "[serial-debug] port resolution failed" | tee -a "$SERIAL_DEBUG_LOG"
    EXIT_CODE=1
  else
    port_esp32=$(python3 - "$ports_json" <<'PY'
import json, sys
data = json.load(open(sys.argv[1]))
ports = data.get("ports", {})
print(ports.get("esp32", ""))
PY
)
    if [[ -z "$port_esp32" ]]; then
      echo "[serial-debug] ESP32 port missing" | tee -a "$SERIAL_DEBUG_LOG"
      EXIT_CODE=1
    else
      evidence_record_command "python3 tools/dev/serial_smoke.py --wifi-debug --wifi-debug-seconds $SERIAL_DEBUG_SECONDS --wifi-debug-regex '$SERIAL_DEBUG_REGEX' --role esp32 --port $port_esp32 --baud 115200 --no-evidence"
      if ! python3 "$FW_ROOT/tools/dev/serial_smoke.py" \
        --wifi-debug \
        --wifi-debug-seconds "$SERIAL_DEBUG_SECONDS" \
        --wifi-debug-regex "$SERIAL_DEBUG_REGEX" \
        --role esp32 \
        --port "$port_esp32" \
        --baud 115200 \
        --no-evidence \
        | tee "$SERIAL_DEBUG_LOG"; then
        EXIT_CODE=1
      fi
    fi
  fi
fi

log "RTOS/WiFi health artifact: $ARTIFACT"

result="PASS"
if [[ "$EXIT_CODE" != "0" ]]; then
  result="FAIL"
fi

cat > "$EVIDENCE_SUMMARY" <<EOF
# RTOS/WiFi health summary

- Result: **${result}**
- URL: ${ESP_URL}
- Log: $(basename "$ARTIFACT")
 - Serial debug: $(if [[ "$SERIAL_DEBUG" == "1" || "${ZACUS_WIFI_SERIAL_DEBUG:-0}" == "1" ]]; then echo "enabled"; else echo "disabled"; fi)
 - Serial log: $(if [[ "$SERIAL_DEBUG" == "1" || "${ZACUS_WIFI_SERIAL_DEBUG:-0}" == "1" ]]; then basename "$SERIAL_DEBUG_LOG"; else echo "n/a"; fi)
EOF

echo "RESULT=${result}"
exit "$EXIT_CODE"
