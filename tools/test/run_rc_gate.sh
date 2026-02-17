#!/usr/bin/env bash
set -euo pipefail

SPRINT=""
PORT_ESP32=""
PORT_ESP8266=""
ALLOW_NO_HARDWARE=0
REQUIRE_HW=0
SKIP_BUILD=0
DRY_RUN=0
INCLUDE_CONSOLE=0
AUTO_PORTS=1
WAIT_PORT=3
PREFER_CU=0
OUTDIR="${ZACUS_OUTDIR:-}"
PHASE="rc_gate"

if [[ "$(uname -s)" == "Darwin" ]]; then
  PREFER_CU=1
fi

usage() {
  cat <<'USAGE'
Usage: bash tools/test/run_rc_gate.sh --sprint s1|s2|s3|s4|s5 [options]

Options:
  --sprint <id>              Sprint gate to run (s1..s5)
  --port-esp32 <port>        Explicit ESP32 serial port
  --port-esp8266 <port>      Explicit ESP8266/OLED serial port
  --port-ui <port>           Alias of --port-esp8266
  --esp32-port <port>        Backward-compatible alias of --port-esp32
  --ui-port <port>           Backward-compatible alias of --port-esp8266
  --prefer-cu                Prefer /dev/cu.* candidates (default ON on macOS)
  --auto-ports               Auto-resolve ports when missing (default)
  --no-auto-ports            Disable auto-resolve
  --wait-port <sec>          Wait window for detection/probe (default: 3)
  --allow-no-hardware        Allow missing ports and convert live checks to SKIP
  --require-hw               Force strict hardware resolution (overrides allow-no-hardware)
  --skip-build               Skip sprint 5 build gate command
  --include-console          Run sprint 5 console sanity command (interactive)
  --dry-run                  Print commands without executing
  --outdir <path>            Evidence output directory (default: artifacts/rc_gate/<timestamp>)
  -h, --help                 Show this help
USAGE
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --sprint)
      SPRINT="${2:-}"
      shift 2
      ;;
    --port-esp32|--esp32-port)
      PORT_ESP32="${2:-}"
      shift 2
      ;;
    --port-esp8266|--port-ui|--ui-port)
      PORT_ESP8266="${2:-}"
      shift 2
      ;;
    --allow-no-hardware)
      ALLOW_NO_HARDWARE=1
      shift
      ;;
    --require-hw)
      REQUIRE_HW=1
      shift
      ;;
    --prefer-cu)
      PREFER_CU=1
      shift
      ;;
    --auto-ports)
      AUTO_PORTS=1
      shift
      ;;
    --no-auto-ports)
      AUTO_PORTS=0
      shift
      ;;
    --wait-port)
      WAIT_PORT="${2:-}"
      shift 2
      ;;
    --skip-build)
      SKIP_BUILD=1
      shift
      ;;
    --include-console)
      INCLUDE_CONSOLE=1
      shift
      ;;
    --dry-run)
      DRY_RUN=1
      shift
      ;;
    --outdir)
      OUTDIR="${2:-}"
      shift 2
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "[fail] unknown option: $1" >&2
      usage >&2
      exit 2
      ;;
  esac
done

if [[ -z "$SPRINT" ]]; then
  echo "[fail] --sprint is required" >&2
  usage >&2
  exit 2
fi

if ! [[ "$WAIT_PORT" =~ ^[0-9]+$ ]]; then
  echo "[fail] --wait-port must be an integer" >&2
  exit 2
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
FW="$REPO_ROOT/hardware/firmware"
RESOLVER="$REPO_ROOT/tools/test/resolve_ports.py"
source "$FW/tools/dev/agent_utils.sh"
if [[ -x "$FW/.venv/bin/python" ]]; then
  PYTHON="$FW/.venv/bin/python"
  export PATH="$FW/.venv/bin:$PATH"
else
  PYTHON="python3"
fi
export PLATFORMIO_CORE_DIR="${PLATFORMIO_CORE_DIR:-$HOME/.platformio}"
export PIP_DISABLE_PIP_VERSION_CHECK=1
cd "$REPO_ROOT"

EVIDENCE_CMDLINE="$0 $*"
export EVIDENCE_CMDLINE
evidence_init "$PHASE" "$OUTDIR"
RUN_LOG="$EVIDENCE_DIR/run_rc_gate.log"
STEP_INDEX=0
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
  cat > "$EVIDENCE_SUMMARY" <<EOF
# RC gate summary

- Result: **${result}**
- Sprint: ${SPRINT}
- ESP32 port: ${PORT_ESP32:-n/a}
- ESP8266 port: ${PORT_ESP8266:-n/a}
- Log: $(basename "$RUN_LOG")
EOF
  echo "RESULT=${result}"
  exit "$rc"
}

trap 'finalize "$?"' EXIT

run_cmd() {
  if [[ "$DRY_RUN" == "1" ]]; then
    echo "[dry-run] $*"
    return 0
  fi
  STEP_INDEX=$((STEP_INDEX + 1))
  local step_log="$EVIDENCE_DIR/step_${STEP_INDEX}.log"
  echo "[step] $*" | tee -a "$RUN_LOG" "$step_log"
  evidence_record_command "$*"
  "$@" >>"$step_log" 2>&1
}

run_cmd_shell() {
  local expr="$1"
  if [[ "$DRY_RUN" == "1" ]]; then
    echo "[dry-run] bash -lc $expr"
    return 0
  fi
  STEP_INDEX=$((STEP_INDEX + 1))
  local step_log="$EVIDENCE_DIR/step_${STEP_INDEX}.log"
  echo "[step] bash -lc $expr" | tee -a "$RUN_LOG" "$step_log"
  evidence_record_command "bash -lc $expr"
  bash -lc "$expr" >>"$step_log" 2>&1
}

resolve_ports() {
  local need_esp32="$1"
  local need_esp8266="$2"
  local allow_no_hw="$ALLOW_NO_HARDWARE"

  if [[ "$REQUIRE_HW" == "1" ]]; then
    allow_no_hw=0
  fi
  if [[ "$DRY_RUN" == "1" ]]; then
    allow_no_hw=1
  fi

  local args=(
    "$RESOLVER"
    "--port-esp32" "$PORT_ESP32"
    "--port-esp8266" "$PORT_ESP8266"
    "--wait-port" "$WAIT_PORT"
    "--ports-resolve-json" "$EVIDENCE_DIR/ports_resolve.json"
  )

  if [[ "$AUTO_PORTS" == "1" ]]; then
    args+=("--auto-ports")
  fi
  if [[ "$PREFER_CU" == "1" ]]; then
    args+=("--prefer-cu")
  fi
  if [[ "$need_esp32" == "1" ]]; then
    args+=("--need-esp32")
  fi
  if [[ "$need_esp8266" == "1" ]]; then
    args+=("--need-esp8266")
  fi
  if [[ "$allow_no_hw" == "1" ]]; then
    args+=("--allow-no-hardware")
  fi
  if [[ -t 0 ]]; then
    args+=("--interactive")
  fi

  local resolve_json
  evidence_record_command "$PYTHON ${args[*]}"
  if ! resolve_json="$($PYTHON "${args[@]}")"; then
    echo "[fail] port resolution failed" >&2
    return 2
  fi

  eval "$(
    RESOLVE_JSON="$resolve_json" "$PYTHON" -c '
import json, os, shlex
data = json.loads(os.environ["RESOLVE_JSON"])
ports = data.get("ports", {})
reasons = data.get("reasons", {})
status = data.get("status", "fail")
notes = data.get("notes", [])
print("RESOLVE_STATUS=" + shlex.quote(status))
print("RESOLVE_PORT_ESP32=" + shlex.quote(str(ports.get("esp32", ""))))
print("RESOLVE_PORT_ESP8266=" + shlex.quote(str(ports.get("esp8266", ""))))
print("RESOLVE_REASON_ESP32=" + shlex.quote(str(reasons.get("esp32", ""))))
print("RESOLVE_REASON_ESP8266=" + shlex.quote(str(reasons.get("esp8266", ""))))
print("RESOLVE_NOTES=" + shlex.quote(" | ".join(str(n) for n in notes)))
')"

  PORT_ESP32="$RESOLVE_PORT_ESP32"
  PORT_ESP8266="$RESOLVE_PORT_ESP8266"
  if [[ -n "$PORT_ESP32" ]]; then
    echo "[port] ESP32=$PORT_ESP32 ($RESOLVE_REASON_ESP32)"
  fi
  if [[ -n "$PORT_ESP8266" ]]; then
    echo "[port] ESP8266=$PORT_ESP8266 ($RESOLVE_REASON_ESP8266)"
  fi
  if [[ -n "$RESOLVE_NOTES" ]]; then
    echo "[port] notes: $RESOLVE_NOTES"
  fi

  if [[ "$RESOLVE_STATUS" == "fail" ]]; then
    echo "[fail] unable to resolve required hardware ports." >&2
    return 2
  fi
}

python_cmd() {
  local script="$1"
  shift
  run_cmd "$PYTHON" "$script" "$@"
}

live_suite() {
  local suite="$1"
  local role="$2"
  local args=(--suite "$suite" --role "$role")
  if [[ -n "$PORT_ESP32" ]]; then
    args+=(--port "$PORT_ESP32")
  elif [[ "$ALLOW_NO_HARDWARE" == "1" ]]; then
    args+=(--allow-no-hardware)
  fi
  python_cmd tools/test/run_serial_suite.py "${args[@]}"
}

live_menu_smoke() {
  local args=(--action smoke --role esp32)
  if [[ -n "$PORT_ESP32" ]]; then
    args+=(--port "$PORT_ESP32")
  elif [[ "$ALLOW_NO_HARDWARE" == "1" ]]; then
    args+=(--allow-no-hardware)
  fi
  python_cmd tools/test/zacus_menu.py "${args[@]}"
}

live_ui_link() {
  local script_args=(--script "NEXT:click,OK:long,MODE:click")
  if [[ -n "$PORT_ESP8266" ]]; then
    script_args=(--port "$PORT_ESP8266" "${script_args[@]}")
  elif [[ "$ALLOW_NO_HARDWARE" == "1" ]]; then
    script_args+=(--allow-no-hardware)
  fi
  python_cmd tools/test/ui_link_sim.py "${script_args[@]}"

  local menu_args=(--action ui_link --script "NEXT:click,OK:long")
  if [[ -n "$PORT_ESP8266" ]]; then
    menu_args+=(--port "$PORT_ESP8266")
  elif [[ "$ALLOW_NO_HARDWARE" == "1" ]]; then
    menu_args+=(--allow-no-hardware)
  fi
  python_cmd tools/test/zacus_menu.py "${menu_args[@]}"
}

live_console_sanity() {
  local args=(--action console --timeout 1.5)
  if [[ -n "$PORT_ESP32" ]]; then
    args+=(--port "$PORT_ESP32")
  elif [[ "$ALLOW_NO_HARDWARE" == "1" ]]; then
    args+=(--allow-no-hardware)
  fi

  if [[ "$DRY_RUN" == "1" ]]; then
    echo "[dry-run] printf '/quit\\n' | python3 tools/test/zacus_menu.py ${args[*]}"
    return 0
  fi

  echo "[step] console sanity (/quit)"
  printf '/quit\n' | python3 tools/test/zacus_menu.py "${args[@]}"
}

run_s1() {
  echo "=== Sprint 1 gate ==="
  resolve_ports 1 0
  run_cmd "$PYTHON" -m compileall tools/test
  run_cmd bash -n tools/test/run_content_checks.sh
  run_cmd bash tools/test/run_content_checks.sh
  run_cmd bash tools/test/run_content_checks.sh --check-clean-git
  live_suite smoke_plus esp32
  live_menu_smoke
}

run_s2() {
  echo "=== Sprint 2 gate ==="
  resolve_ports 1 0
  run_cmd "$PYTHON" tools/test/run_serial_suite.py --list-suites
  run_cmd "$PYTHON" tools/test/run_serial_suite.py --suite mp3_basic --allow-no-hardware
  run_cmd "$PYTHON" tools/test/run_serial_suite.py --suite mp3_fx --allow-no-hardware
  live_suite mp3_basic esp32
  live_suite mp3_fx esp32
}

run_s3() {
  echo "=== Sprint 3 gate ==="
  resolve_ports 1 0
  run_cmd "$PYTHON" tools/test/run_serial_suite.py --suite story_v2_basic --allow-no-hardware
  run_cmd "$PYTHON" tools/test/run_serial_suite.py --suite story_v2_metrics --allow-no-hardware
  live_suite story_v2_basic esp32
  live_suite story_v2_metrics esp32
}

run_s4() {
  echo "=== Sprint 4 gate ==="
  resolve_ports 0 1
  run_cmd "$PYTHON" tools/test/zacus_menu.py --help
  run_cmd "$PYTHON" tools/test/zacus_menu.py --action content
  run_cmd "$PYTHON" tools/test/zacus_menu.py --action suite --suite smoke_plus --allow-no-hardware
  run_cmd "$PYTHON" tools/test/ui_link_sim.py --allow-no-hardware --script "NEXT:click,OK:long"
  live_ui_link
}

run_s5() {
  echo "=== Sprint 5 gate ==="
  resolve_ports 1 1
  run_cmd "$PYTHON" -m compileall tools/test
  run_cmd bash tools/test/run_content_checks.sh --check-clean-git
  run_cmd "$PYTHON" tools/test/run_serial_suite.py --list-suites
  run_cmd "$PYTHON" tools/test/zacus_menu.py --help

  if [[ "$SKIP_BUILD" == "1" ]]; then
    echo "[warn] build gate skipped (--skip-build)"
  else
    run_cmd_shell "cd \"$FW\" && ZACUS_SKIP_IF_BUILT=1 ./tools/dev/run_matrix_and_smoke.sh"
  fi

  live_suite smoke_plus esp32
  live_suite mp3_basic esp32
  live_suite mp3_fx esp32
  live_suite story_v2_basic esp32
  live_suite story_v2_metrics esp32
  live_ui_link

  if [[ "$INCLUDE_CONSOLE" == "1" ]]; then
    live_console_sanity
  else
    echo "[info] console sanity skipped (use --include-console to run it)"
  fi
}

case "$SPRINT" in
  s1) run_s1 ;;
  s2) run_s2 ;;
  s3) run_s3 ;;
  s4) run_s4 ;;
  s5) run_s5 ;;
  *)
    echo "[fail] invalid sprint: $SPRINT (expected s1..s5)" >&2
    exit 2
    ;;
esac

echo "[ok] sprint gate complete: $SPRINT"
