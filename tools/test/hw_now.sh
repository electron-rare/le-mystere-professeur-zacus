#!/usr/bin/env bash
set -euo pipefail

ENV_ESP32="esp32dev"
ENV_ESP8266="esp8266_oled"
PORT_ESP32=""
PORT_ESP8266=""
EXPLICIT_ESP32=0
EXPLICIT_ESP8266=0
AUTO_PORTS=1
PREFER_CU=0
WAIT_PORT=20
SKIP_BUILD=0
SKIP_UPLOAD=0
DRY_RUN=0
SMOKE_BAUD_OVERRIDE=""
COUNTDOWN_SEC=20

if [[ "$(uname -s)" == "Darwin" ]]; then
  PREFER_CU=1
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
FW="$REPO_ROOT/hardware/firmware"
RESOLVER="$REPO_ROOT/tools/test/resolve_ports.py"
GATE_SCRIPT="$REPO_ROOT/tools/test/run_rc_gate.sh"
SMOKE_SCRIPT="$FW/tools/dev/serial_smoke.py"

if [[ ! -f "$FW/platformio.ini" ]]; then
  echo "[fail] firmware workspace not found: $FW" >&2
  exit 2
fi

if [[ -x "$FW/.venv/bin/python" ]]; then
  PYTHON="$FW/.venv/bin/python"
  export PATH="$FW/.venv/bin:$PATH"
else
  PYTHON="python3"
fi
if [[ -x "$FW/.venv/bin/pio" ]]; then
  PIO="$FW/.venv/bin/pio"
else
  PIO="pio"
fi

export PLATFORMIO_CORE_DIR="${PLATFORMIO_CORE_DIR:-$HOME/.platformio}"
export PIP_DISABLE_PIP_VERSION_CHECK=1

usage() {
  cat <<'EOF'
Usage: bash tools/test/hw_now.sh [options]

Options:
  --env-esp32 <env>        ESP32 PlatformIO env (default: esp32dev)
  --env-esp8266 <env>      ESP8266 PlatformIO env (default: esp8266_oled)
  --port-esp32 <path>      Explicit ESP32 port
  --port-esp8266 <path>    Explicit ESP8266/OLED port
  --port-ui <path>         Alias of --port-esp8266
  --auto-ports             Auto resolve ports (default)
  --no-auto-ports          Disable auto port detection
  --prefer-cu              Prefer /dev/cu.* on macOS
  --wait-port <sec>        Detection wait window (default: 20)
  --skip-build             Skip explicit build phase
  --skip-upload            Skip upload phase
  --smoke-baud <n>         Force single smoke baud (no fallback)
  --dry-run                Print intended commands without execution
  -h, --help               Show this help
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --env-esp32) ENV_ESP32="${2:-}"; shift 2 ;;
    --env-esp8266) ENV_ESP8266="${2:-}"; shift 2 ;;
    --port-esp32) PORT_ESP32="${2:-}"; shift 2 ;;
    --port-esp8266|--port-ui) PORT_ESP8266="${2:-}"; shift 2 ;;
    --auto-ports) AUTO_PORTS=1; shift ;;
    --no-auto-ports) AUTO_PORTS=0; shift ;;
    --prefer-cu) PREFER_CU=1; shift ;;
    --wait-port) WAIT_PORT="${2:-}"; shift 2 ;;
    --skip-build) SKIP_BUILD=1; shift ;;
    --skip-upload) SKIP_UPLOAD=1; shift ;;
    --smoke-baud) SMOKE_BAUD_OVERRIDE="${2:-}"; shift 2 ;;
    --dry-run) DRY_RUN=1; shift ;;
    -h|--help) usage; exit 0 ;;
    *) echo "[fail] unknown option: $1" >&2; usage >&2; exit 2 ;;
  esac
done

if [[ -n "$PORT_ESP32" ]]; then
  EXPLICIT_ESP32=1
fi
if [[ -n "$PORT_ESP8266" ]]; then
  EXPLICIT_ESP8266=1
fi

if ! [[ "$WAIT_PORT" =~ ^[0-9]+$ ]]; then
  echo "[fail] --wait-port must be an integer" >&2
  exit 2
fi
if [[ -n "$SMOKE_BAUD_OVERRIDE" ]] && ! [[ "$SMOKE_BAUD_OVERRIDE" =~ ^[0-9]+$ ]]; then
  echo "[fail] --smoke-baud must be numeric" >&2
  exit 2
fi

timestamp="$(date -u +"%Y%m%d-%H%M%S")"
artifact_dir="$FW/artifacts/rc_live/$timestamp"
log_dir="$FW/logs"
run_log="$log_dir/hw_now_${timestamp}.log"
steps_tsv="$artifact_dir/steps.tsv"
mkdir -p "$artifact_dir" "$log_dir"
: > "$steps_tsv"

action_log() {
  local msg="$1"
  echo "$msg" | tee -a "$run_log"
}

append_step() {
  local name="$1"
  local status="$2"
  local code="$3"
  local log_file="$4"
  local details="$5"
  printf '%s\t%s\t%s\t%s\t%s\n' "$name" "$status" "$code" "$log_file" "$details" >> "$steps_tsv"
}

run_step() {
  local step_name="$1"
  local log_file="$2"
  shift 2
  mkdir -p "$(dirname "$log_file")"
  if [[ "$DRY_RUN" == "1" ]]; then
    printf '[dry-run] %s\n' "$*" | tee -a "$log_file" "$run_log"
    append_step "$step_name" "SKIP" "0" "$log_file" "dry-run"
    return 0
  fi
  printf '[step] %s\n' "$*" | tee -a "$log_file" "$run_log"
  if "$@" >>"$log_file" 2>&1; then
    append_step "$step_name" "PASS" "0" "$log_file" ""
    return 0
  else
    local rc=$?
    append_step "$step_name" "FAIL" "$rc" "$log_file" ""
    return "$rc"
  fi
}

resolve_ports() {
  local force_fresh="${1:-0}"
  local arg_esp32="$PORT_ESP32"
  local arg_esp8266="$PORT_ESP8266"
  if [[ "$force_fresh" == "1" ]]; then
    if [[ "$EXPLICIT_ESP32" == "0" ]]; then
      arg_esp32=""
    fi
    if [[ "$EXPLICIT_ESP8266" == "0" ]]; then
      arg_esp8266=""
    fi
  fi
  local resolver_args=(
    "$RESOLVER"
    "--port-esp32" "$arg_esp32"
    "--port-esp8266" "$arg_esp8266"
    "--wait-port" "$WAIT_PORT"
    "--need-esp32"
    "--need-esp8266"
    "--ports-resolve-json" "$artifact_dir/ports_resolve.json"
  )
  if [[ "$AUTO_PORTS" == "1" ]]; then
    resolver_args+=("--auto-ports")
  else
    resolver_args+=("--no-auto-ports")
  fi
  if [[ "$PREFER_CU" == "1" ]]; then
    resolver_args+=("--prefer-cu")
  fi
  if [[ "$DRY_RUN" == "1" ]]; then
    resolver_args+=("--allow-no-hardware")
  fi
  if [[ -t 0 ]]; then
    resolver_args+=("--interactive")
  fi

  local resolve_json
  if ! resolve_json="$("$PYTHON" "${resolver_args[@]}")"; then
    echo "[fail] port resolution failed" | tee -a "$run_log" >&2
    return 2
  fi
  printf '%s\n' "$resolve_json" > "$artifact_dir/ports_resolve.json"

  eval "$(RESOLVE_JSON="$resolve_json" "$PYTHON" -c '
import json, os, shlex
v = json.loads(os.environ["RESOLVE_JSON"])
print("RESOLVE_STATUS=" + shlex.quote(str(v.get("status", "fail"))))
print("RESOLVE_PORT_ESP32=" + shlex.quote(str(v.get("ports", {}).get("esp32", ""))))
print("RESOLVE_PORT_ESP8266=" + shlex.quote(str(v.get("ports", {}).get("esp8266", ""))))
print("RESOLVE_REASON_ESP32=" + shlex.quote(str(v.get("reasons", {}).get("esp32", ""))))
print("RESOLVE_REASON_ESP8266=" + shlex.quote(str(v.get("reasons", {}).get("esp8266", ""))))
print("RESOLVE_LOCATION_ESP32=" + shlex.quote(str(v.get("details", {}).get("esp32", {}).get("location", ""))))
print("RESOLVE_LOCATION_ESP8266=" + shlex.quote(str(v.get("details", {}).get("esp8266", {}).get("location", ""))))
print("RESOLVE_ROLE_ESP32=" + shlex.quote(str(v.get("details", {}).get("esp32", {}).get("role", ""))))
print("RESOLVE_ROLE_ESP8266=" + shlex.quote(str(v.get("details", {}).get("esp8266", {}).get("role", ""))))
print("RESOLVE_NOTES=" + shlex.quote(" | ".join(v.get("notes", []))))
')"

  PORT_ESP32="$RESOLVE_PORT_ESP32"
  PORT_ESP8266="$RESOLVE_PORT_ESP8266"
  action_log "[port] ESP32 = ${PORT_ESP32:-n/a} location=${RESOLVE_LOCATION_ESP32:-unknown} role=${RESOLVE_ROLE_ESP32:-esp32} (${RESOLVE_REASON_ESP32:-unresolved})"
  action_log "[port] ESP8266 = ${PORT_ESP8266:-n/a} location=${RESOLVE_LOCATION_ESP8266:-unknown} role=${RESOLVE_ROLE_ESP8266:-esp8266_usb} (${RESOLVE_REASON_ESP8266:-unresolved})"
  if [[ -n "$RESOLVE_NOTES" ]]; then
    action_log "[port] notes: $RESOLVE_NOTES"
  fi
  [[ "$RESOLVE_STATUS" != "fail" ]]
}

countdown_usb_prompt() {
  if [[ "$DRY_RUN" == "1" || "${ZACUS_NO_CONFIRM:-0}" == "1" ]]; then
    action_log "[info] USB confirm prompt skipped"
    return
  fi
  local i
  for i in 1 2 3; do
    action_log "⚠️ BRANCHE L’USB MAINTENANT ⚠️"
    printf '\a' >>"$run_log"
  done
  read -r -p "USB branchés ? Appuie sur Entrée pour continuer..." < /dev/tty
  action_log "[info] USB confirmation received"
}

run_smoke_with_policy() {
  local role="$1"
  local port="$2"
  local log_file="$3"
  local bauds=()

  if [[ -n "$SMOKE_BAUD_OVERRIDE" ]]; then
    bauds=("$SMOKE_BAUD_OVERRIDE")
  elif [[ "$role" == "esp32" ]]; then
    bauds=("115200" "19200")
  elif [[ "$role" == "esp8266_usb" ]]; then
    bauds=("115200")
  else
    bauds=("115200")
  fi

  local b
  for b in "${bauds[@]}"; do
    if [[ "$DRY_RUN" == "1" ]]; then
      printf '[dry-run] %s %s --role %s --port %s --baud %s --wait-port %s\n' "$PYTHON" "$SMOKE_SCRIPT" "$role" "$port" "$b" "$WAIT_PORT" | tee -a "$log_file" "$run_log"
      append_step "smoke_${role}" "SKIP" "0" "$log_file" "dry-run"
      return 0
    fi
    printf '[step] smoke role=%s port=%s baud=%s\n' "$role" "$port" "$b" | tee -a "$log_file" "$run_log"
    if "$PYTHON" "$SMOKE_SCRIPT" --role "$role" --port "$port" --baud "$b" --wait-port "$WAIT_PORT" >>"$log_file" 2>&1; then
      append_step "smoke_${role}" "PASS" "0" "$log_file" "baud=$b"
      echo "$b" > "$artifact_dir/smoke_${role}_baud.txt"
      cp "$log_file" "$log_dir/smoke_${role}_${timestamp}.log"
      return 0
    fi
  done

  append_step "smoke_${role}" "FAIL" "1" "$log_file" "tried=$(IFS=,; echo "${bauds[*]}")"
  cp "$log_file" "$log_dir/smoke_${role}_${timestamp}.log"
  return 1
}

check_ui_link_status() {
  local log_file="$artifact_dir/ui_link.log"
  if [[ "$DRY_RUN" == "1" ]]; then
    append_step "ui_link" "SKIP" "0" "$log_file" "dry-run"
    return 0
  fi
  if "$PYTHON" - "$PORT_ESP32" >"$log_file" 2>&1 <<'PY'
import re
import sys
import time
import serial

port = sys.argv[1]
ser = serial.Serial(port, 115200, timeout=0.25)
try:
    ser.reset_input_buffer()
    ser.write(b"UI_LINK_STATUS\n")
    deadline = time.time() + 2.5
    found = ""
    connected = None
    while time.time() < deadline:
        line = ser.readline().decode("utf-8", errors="ignore").strip()
        if not line:
            continue
        print(line)
        if "UI_LINK_STATUS" in line:
            found = line
            m = re.search(r"connected=(\d)", line)
            if m:
                connected = int(m.group(1))
            break
    if not found:
        print("UI_LINK_STATUS missing")
        raise SystemExit(1)
    if connected == 1:
        raise SystemExit(0)
    print("UI link connected=0")
    raise SystemExit(3)
finally:
    ser.close()
PY
  then
    append_step "ui_link" "PASS" "0" "$log_file" "connected=1"
  else
    rc=$?
    if [[ "$rc" == "3" ]]; then
      append_step "ui_link" "FAIL" "3" "$log_file" "connected=0"
    else
      append_step "ui_link" "FAIL" "$rc" "$log_file" "status unavailable"
    fi
  fi
}

cd "$FW"
action_log "[info] FW=$FW"
action_log "[info] artifacts: $artifact_dir"

if [[ "$AUTO_PORTS" == "1" ]]; then
  while true; do
    countdown_usb_prompt
    if ! resolve_ports 1; then
      append_step "resolve_ports" "FAIL" "2" "$artifact_dir/ports_resolve.json" "port resolution failed"
      if [[ "${ZACUS_REQUIRE_HW:-1}" == "1" && "${ZACUS_NO_CONFIRM:-0}" != "1" && "$DRY_RUN" != "1" ]]; then
        action_log "[warn] hardware required: reconnect USB and retry"
        continue
      fi
      exit 2
    fi
    if [[ "${ZACUS_REQUIRE_HW:-1}" == "1" && ( -z "$PORT_ESP32" || -z "$PORT_ESP8266" ) ]]; then
      append_step "resolve_ports" "FAIL" "2" "$artifact_dir/ports_resolve.json" "missing required roles"
      if [[ "${ZACUS_NO_CONFIRM:-0}" != "1" && "$DRY_RUN" != "1" ]]; then
        action_log "[warn] missing role ports, reconnect then confirm again"
        continue
      fi
      exit 2
    fi
    append_step "resolve_ports" "PASS" "0" "$artifact_dir/ports_resolve.json" "ok"
    break
  done
else
  if ! resolve_ports; then
    append_step "resolve_ports" "FAIL" "2" "$artifact_dir/ports_resolve.json" "port resolution failed"
    exit 2
  fi
  append_step "resolve_ports" "PASS" "0" "$artifact_dir/ports_resolve.json" "ok"
fi

if [[ "$SKIP_UPLOAD" == "0" ]]; then
  if [[ "$SKIP_BUILD" == "0" ]]; then
    run_step "build_esp32" "$artifact_dir/esp32_upload.log" "$PIO" run -e "$ENV_ESP32" || true
    run_step "build_esp8266" "$artifact_dir/esp8266_upload.log" "$PIO" run -e "$ENV_ESP8266" || true
  else
    append_step "build_esp32" "SKIP" "0" "$artifact_dir/esp32_upload.log" "--skip-build"
    append_step "build_esp8266" "SKIP" "0" "$artifact_dir/esp8266_upload.log" "--skip-build"
  fi
  run_step "upload_esp32" "$artifact_dir/esp32_upload.log" "$PIO" run -e "$ENV_ESP32" -t upload --upload-port "$PORT_ESP32" || action_log "[warn] ESP32 upload failed: cable/driver/BOOT button"
  run_step "upload_esp8266" "$artifact_dir/esp8266_upload.log" "$PIO" run -e "$ENV_ESP8266" -t upload --upload-port "$PORT_ESP8266" || action_log "[warn] ESP8266 upload failed: cable + CH340/CP210x driver"
else
  append_step "build_esp32" "SKIP" "0" "$artifact_dir/esp32_upload.log" "--skip-upload"
  append_step "build_esp8266" "SKIP" "0" "$artifact_dir/esp8266_upload.log" "--skip-upload"
  append_step "upload_esp32" "SKIP" "0" "$artifact_dir/esp32_upload.log" "--skip-upload"
  append_step "upload_esp8266" "SKIP" "0" "$artifact_dir/esp8266_upload.log" "--skip-upload"
fi

run_smoke_with_policy "esp32" "$PORT_ESP32" "$artifact_dir/smoke_esp32.log" || true
run_smoke_with_policy "esp8266_usb" "$PORT_ESP8266" "$artifact_dir/smoke_esp8266.log" || true
check_ui_link_status || true

run_step "gate_s1" "$artifact_dir/gate_s1.log" bash "$GATE_SCRIPT" --sprint s1 --port-esp32 "$PORT_ESP32" --port-esp8266 "$PORT_ESP8266" --no-auto-ports --require-hw || true
run_step "gate_s2" "$artifact_dir/gate_s2.log" bash "$GATE_SCRIPT" --sprint s2 --port-esp32 "$PORT_ESP32" --port-esp8266 "$PORT_ESP8266" --no-auto-ports --require-hw || true

"$PYTHON" - "$artifact_dir" "$steps_tsv" "$timestamp" "$PORT_ESP32" "$PORT_ESP8266" "$ENV_ESP32" "$ENV_ESP8266" <<'PY'
import json
import sys
from pathlib import Path

artifact_dir = Path(sys.argv[1])
steps_tsv = Path(sys.argv[2])
timestamp = sys.argv[3]
port_esp32 = sys.argv[4]
port_esp8266 = sys.argv[5]
env_esp32 = sys.argv[6]
env_esp8266 = sys.argv[7]
ports_resolve = artifact_dir / "ports_resolve.json"
resolve_data = {}
if ports_resolve.exists():
    resolve_data = json.loads(ports_resolve.read_text(encoding="utf-8"))

steps = []
for raw in steps_tsv.read_text(encoding="utf-8").splitlines():
    if not raw.strip():
        continue
    name, status, exit_code, log_file, details = raw.split("\t", 4)
    steps.append({"name": name, "status": status, "exit_code": int(exit_code), "log_file": log_file, "details": details})

critical = {"resolve_ports", "upload_esp32", "upload_esp8266", "smoke_esp32", "smoke_esp8266_usb", "gate_s1", "gate_s2"}
overall = "PASS"
if any(s["name"] in critical and s["status"] == "FAIL" for s in steps):
    overall = "FAIL"
elif all(s["status"] == "SKIP" for s in steps):
    overall = "SKIP"

ui_status = next((s for s in steps if s["name"] == "ui_link"), None)
summary = {
    "timestamp": timestamp,
    "ports": {
        "esp32": {
            "port": port_esp32,
            "location": resolve_data.get("details", {}).get("esp32", {}).get("location", ""),
            "role": "esp32",
            "reason": resolve_data.get("details", {}).get("esp32", {}).get("reason", ""),
        },
        "esp8266_usb": {
            "port": port_esp8266,
            "location": resolve_data.get("details", {}).get("esp8266", {}).get("location", ""),
            "role": "esp8266_usb",
            "reason": resolve_data.get("details", {}).get("esp8266", {}).get("reason", ""),
        },
    },
    "envs": {"esp32": env_esp32, "esp8266": env_esp8266},
    "steps": steps,
    "result": overall,
    "ui_link": ui_status,
    "notes": [
        "If upload fails: verify USB data cable, CP210x/CH340 driver, and power/reset.",
        "For ESP32 sync errors: hold BOOT while flashing.",
    ],
}
(artifact_dir / "summary.json").write_text(json.dumps(summary, indent=2), encoding="utf-8")

rows = [
    "# RC live summary",
    "",
    f"- Result: **{overall}**",
    f"- ESP32 port: `{port_esp32}` (location `{resolve_data.get('details', {}).get('esp32', {}).get('location', '')}`)",
    f"- ESP8266 USB port: `{port_esp8266}` (location `{resolve_data.get('details', {}).get('esp8266', {}).get('location', '')}`)",
    f"- UI link: `{ui_status['status'] if ui_status else 'n/a'}`",
    "",
    "| Step | Status | Exit | Log | Details |",
    "|---|---|---:|---|---|",
]
for s in steps:
    rows.append(f"| {s['name']} | {s['status']} | {s['exit_code']} | `{Path(s['log_file']).name}` | {s['details']} |")
(artifact_dir / "summary.md").write_text("\n".join(rows) + "\n", encoding="utf-8")
PY

action_log "[done] summary: $artifact_dir/summary.md"
