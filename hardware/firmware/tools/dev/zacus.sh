#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
ARTIFACT_ROOT="$REPO_ROOT/artifacts/rc_live"
RESOLVER="$REPO_ROOT/tools/test/resolve_ports.py"
RC_RUNNER="$REPO_ROOT/tools/dev/run_matrix_and_smoke.sh"
PROMPT_DIR="$SCRIPT_DIR/codex_prompts"

require_cmd() {
  if ! command -v "$1" >/dev/null 2>&1; then
    echo "[zacus] missing command: $1" >&2
    exit 127
  fi
}

latest_artifact_dir() {
  if [[ ! -d "$ARTIFACT_ROOT" ]]; then
    return 1
  fi
  local candidate
  candidate=$(ls -1dt "$ARTIFACT_ROOT"/*/ 2>/dev/null | head -n 1 || true)
  if [[ -n "$candidate" ]]; then
    printf '%s' "${candidate%/}"
    return 0
  fi
  return 1
}

log() {
  printf '[zacus] %s\n' "$*"
}

die() {
  printf '[zacus] ERROR: %s\n' "$*" >&2
  exit 1
}

run_rc_gate() {
  log "running RC live (strict, auto-wait)"
  (cd "$REPO_ROOT" && ZACUS_REQUIRE_HW=1 "$RC_RUNNER")
}

choose_autofix_prompt() {
  local summary_file="$1"
  local esp8266_log="$2"
  local port_status=""
  local ui_status=""
  if [[ -f "$summary_file" ]]; then
    read -r port_status ui_status < <(python3 - "$summary_file" <<'PY'
import json
import sys
try:
    data = json.load(open(sys.argv[1]))
except Exception:
    data = {}
print(data.get('port_status', ''))
print(data.get('ui_link_status', ''))
PY
)
  fi
  local prompt="$PROMPT_DIR/auto_fix_generic.prompt.md"
  local reason="generic"
  if [[ "${port_status^^}" == "FAILED" ]]; then
    prompt="$PROMPT_DIR/auto_fix_ports.prompt.md"
    reason="port_status"
  elif [[ "${ui_status^^}" == "FAILED" ]]; then
    prompt="$PROMPT_DIR/auto_fix_ui_link.prompt.md"
    reason="ui_link"
  elif [[ -f "$esp8266_log" && $(grep -i "panic" "$esp8266_log" 2>/dev/null || true) ]]; then
    prompt="$PROMPT_DIR/auto_fix_esp8266_panic.prompt.md"
    reason="esp8266_panic"
  fi
  printf '%s\n%s\n' "$prompt" "$reason"
}

cmd_rc() {
  if run_rc_gate; then
    log "RC live finished"
    return 0
  fi
  log "RC live failed"
  return 1
}

cmd_rc_autofix() {
  if cmd_rc; then
    return 0
  fi
  local artifact
  artifact=$(latest_artifact_dir) || die "no artifact directory available after RC"
  log "artifact path: $artifact"
  require_cmd codex
  local prompt_info=($(choose_autofix_prompt "$artifact/summary.json" "$artifact/smoke_esp8266_usb.log"))
  local prompt_file="${prompt_info[0]}"
  local prompt_reason="${prompt_info[1]:-generic}"
  if [[ ! -f "$prompt_file" ]]; then
    die "prompt file not found: $prompt_file"
  fi
  local codex_output="$artifact/codex_last_message.md"
  log "auto-fix triggered (reason=$prompt_reason) using $prompt_file"
  codex exec --sandbox workspace-write --output-last-message "$codex_output" - < "$prompt_file"
  cat <<'LOGEOF' >> "$artifact/rc_autofix.log"
prompt=$prompt_file
reason=$prompt_reason
codex_output=$codex_output
LOGEOF
  log "rerunning RC live after fix"
  run_rc_gate
}

cmd_flash() {
  require_cmd python3
  require_cmd pio
  mkdir -p "$ARTIFACT_ROOT"
  local timestamp
  timestamp=$(date -u +"%Y%m%d-%H%M%S")
  local run_dir="$ARTIFACT_ROOT/flash-$timestamp"
  mkdir -p "$run_dir"
  local ports_json="$run_dir/ports_resolve.json"
  log "resolving ports for flash"
  (cd "$REPO_ROOT" && python3 "$RESOLVER" --auto-ports --need-esp32 --need-esp8266 --wait-port 5 --ports-resolve-json "$ports_json")
  local port_esp32 port_esp8266
  read -r port_esp32 port_esp8266 < <(python3 - "$ports_json" <<'PY'
import json, sys
try:
    data = json.load(open(sys.argv[1]))
except Exception:
    data = {}
ports = data.get('ports', {})
print(ports.get('esp32', ''))
print(ports.get('esp8266', ''))
PY
)
  if [[ -z "$port_esp32" || -z "$port_esp8266" ]]; then
    die "could not resolve both ports (esp32=$port_esp32 esp8266=$port_esp8266)"
  fi
  log "ESP32 port=$port_esp32 ESP8266 port=$port_esp8266"
  (cd "$REPO_ROOT" && pio run -e esp32dev -t upload --upload-port "$port_esp32")
  (cd "$REPO_ROOT" && pio run -e esp8266_oled -t upload --upload-port "$port_esp8266")
}

cmd_ports() {
  require_cmd python3
  mkdir -p "$ARTIFACT_ROOT"
  local snapshot="$ARTIFACT_ROOT/.ports_watch.json"
  log "ports watch (refresh every 15s, Ctrl+C to stop)"
  while true; do
    printf '=== %s ===\n' "$(date -u +"%Y-%m-%dT%H:%M:%SZ")"
    if python3 "$RESOLVER" --auto-ports --need-esp32 --need-esp8266 --wait-port 5 --allow-no-hardware --ports-resolve-json "$snapshot" >/dev/null; then
      python3 -m json.tool "$snapshot"
    else
      log "port resolution returned fail; retrying"
    fi
    sleep 15
  done
}

cmd_latest() {
  if artifact=$(latest_artifact_dir); then
    printf '%s\n' "$artifact"
    local summary="$artifact/summary.md"
    [[ -f "$summary" ]] && sed -n '1,40p' "$summary"
    return 0
  fi
  die "no rc artifacts yet"
}

cmd_bootstrap() {
  log "running bootstrap"
  (cd "$REPO_ROOT" && ./tools/dev/bootstrap_local.sh)
}

cmd_build() {
  log "running build_all"
  (cd "$REPO_ROOT" && ./build_all.sh)
}

usage() {
  cat <<'HELP'
Usage: zacus.sh <command>
Commands:
  bootstrap     bootstrap tooling
  build         run build_all.sh
  flash         upload esp32 + esp8266 via resolved ports
  rc            strict RC live (ZACUS_REQUIRE_HW=1)
  rc-autofix    RC + codex autofix loop
  ports         ports watch (15s)
  latest        show latest RC artifact path
HELP
}

mkdir -p "$ARTIFACT_ROOT"

command=${1:-}
if [[ -z "$command" ]]; then
  usage
  exit 1
fi

case "$command" in
  bootstrap)
    cmd_bootstrap
    ;;
  build)
    cmd_build
    ;;
  flash)
    cmd_flash
    ;;
  rc)
    cmd_rc
    ;;
  rc-autofix)
    cmd_rc_autofix
    ;;
  ports)
    cmd_ports
    ;;
  latest)
    cmd_latest
    ;;
  *)
    usage
    exit 1
    ;;
esac
