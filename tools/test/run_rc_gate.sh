#!/usr/bin/env bash
set -euo pipefail

SPRINT=""
ESP32_PORT=""
UI_PORT=""
ALLOW_NO_HARDWARE=0
SKIP_BUILD=0
DRY_RUN=0
INCLUDE_CONSOLE=0

usage() {
  cat <<'USAGE'
Usage: bash tools/test/run_rc_gate.sh --sprint s1|s2|s3|s4|s5 [options]

Options:
  --sprint <id>         Sprint gate to run (s1..s5)
  --esp32-port <port>   Explicit ESP32 serial port for live gates
  --ui-port <port>      Explicit UI serial port for UI link live gates
  --allow-no-hardware   Add --allow-no-hardware to live commands
  --skip-build          Skip sprint 5 build gate command
  --include-console     Run sprint 5 console sanity command (interactive)
  --dry-run             Print commands without executing
  -h, --help            Show this help
USAGE
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --sprint)
      SPRINT="${2:-}"
      shift 2
      ;;
    --esp32-port)
      ESP32_PORT="${2:-}"
      shift 2
      ;;
    --ui-port)
      UI_PORT="${2:-}"
      shift 2
      ;;
    --allow-no-hardware)
      ALLOW_NO_HARDWARE=1
      shift
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

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
cd "$REPO_ROOT"

run_cmd() {
  if [[ "$DRY_RUN" == "1" ]]; then
    echo "[dry-run] $*"
    return 0
  fi
  echo "[step] $*"
  "$@"
}

run_cmd_shell() {
  local expr="$1"
  if [[ "$DRY_RUN" == "1" ]]; then
    echo "[dry-run] bash -lc $expr"
    return 0
  fi
  echo "[step] bash -lc $expr"
  bash -lc "$expr"
}

require_live_port() {
  local value="$1"
  local label="$2"
  if [[ -n "$value" ]]; then
    return 0
  fi
  if [[ "$ALLOW_NO_HARDWARE" == "1" ]]; then
    return 0
  fi
  echo "[fail] missing $label for live gate; set $label or use --allow-no-hardware" >&2
  exit 2
}

python_cmd() {
  local script="$1"
  shift
  local args=("$@")
  if [[ "$script" == "tools/test/run_serial_suite.py" || "$script" == "tools/test/zacus_menu.py" || "$script" == "tools/test/ui_link_sim.py" ]]; then
    :
  fi
  run_cmd python3 "$script" "${args[@]}"
}

live_suite() {
  local suite="$1"
  local role="$2"
  local args=(--suite "$suite" --role "$role")
  if [[ -n "$ESP32_PORT" ]]; then
    args+=(--port "$ESP32_PORT")
  elif [[ "$ALLOW_NO_HARDWARE" == "1" ]]; then
    args+=(--allow-no-hardware)
  fi
  python_cmd tools/test/run_serial_suite.py "${args[@]}"
}

live_menu_smoke() {
  local args=(--action smoke --role esp32)
  if [[ -n "$ESP32_PORT" ]]; then
    args+=(--port "$ESP32_PORT")
  elif [[ "$ALLOW_NO_HARDWARE" == "1" ]]; then
    args+=(--allow-no-hardware)
  fi
  python_cmd tools/test/zacus_menu.py "${args[@]}"
}

live_ui_link() {
  local script_args=(--script "NEXT:click,OK:long,MODE:click")
  if [[ -n "$UI_PORT" ]]; then
    script_args=(--port "$UI_PORT" "${script_args[@]}")
  elif [[ "$ALLOW_NO_HARDWARE" == "1" ]]; then
    script_args+=(--allow-no-hardware)
  fi
  python_cmd tools/test/ui_link_sim.py "${script_args[@]}"

  local menu_args=(--action ui_link --script "NEXT:click,OK:long")
  if [[ -n "$UI_PORT" ]]; then
    menu_args+=(--port "$UI_PORT")
  elif [[ "$ALLOW_NO_HARDWARE" == "1" ]]; then
    menu_args+=(--allow-no-hardware)
  fi
  python_cmd tools/test/zacus_menu.py "${menu_args[@]}"
}

live_console_sanity() {
  local args=(--action console --timeout 1.5)
  if [[ -n "$ESP32_PORT" ]]; then
    args+=(--port "$ESP32_PORT")
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
  run_cmd python3 -m compileall tools/test
  run_cmd bash -n tools/test/run_content_checks.sh
  run_cmd bash tools/test/run_content_checks.sh
  run_cmd bash tools/test/run_content_checks.sh --check-clean-git

  require_live_port "$ESP32_PORT" "--esp32-port"
  live_suite smoke_plus esp32
  live_menu_smoke
}

run_s2() {
  echo "=== Sprint 2 gate ==="
  run_cmd python3 tools/test/run_serial_suite.py --list-suites
  run_cmd python3 tools/test/run_serial_suite.py --suite mp3_basic --allow-no-hardware
  run_cmd python3 tools/test/run_serial_suite.py --suite mp3_fx --allow-no-hardware

  require_live_port "$ESP32_PORT" "--esp32-port"
  live_suite mp3_basic esp32
  live_suite mp3_fx esp32
}

run_s3() {
  echo "=== Sprint 3 gate ==="
  run_cmd python3 tools/test/run_serial_suite.py --suite story_v2_basic --allow-no-hardware
  run_cmd python3 tools/test/run_serial_suite.py --suite story_v2_metrics --allow-no-hardware

  require_live_port "$ESP32_PORT" "--esp32-port"
  live_suite story_v2_basic esp32
  live_suite story_v2_metrics esp32
}

run_s4() {
  echo "=== Sprint 4 gate ==="
  run_cmd python3 tools/test/zacus_menu.py --help
  run_cmd python3 tools/test/zacus_menu.py --action content
  run_cmd python3 tools/test/zacus_menu.py --action suite --suite smoke_plus --allow-no-hardware
  run_cmd python3 tools/test/ui_link_sim.py --allow-no-hardware --script "NEXT:click,OK:long"

  require_live_port "$UI_PORT" "--ui-port"
  live_ui_link
}

run_s5() {
  echo "=== Sprint 5 gate ==="
  run_cmd python3 -m compileall tools/test
  run_cmd bash tools/test/run_content_checks.sh --check-clean-git
  run_cmd python3 tools/test/run_serial_suite.py --list-suites
  run_cmd python3 tools/test/zacus_menu.py --help

  if [[ "$SKIP_BUILD" == "1" ]]; then
    echo "[warn] build gate skipped (--skip-build)"
  else
    run_cmd_shell "cd hardware/firmware && ZACUS_SKIP_IF_BUILT=1 ./tools/dev/run_matrix_and_smoke.sh"
  fi

  require_live_port "$ESP32_PORT" "--esp32-port"
  require_live_port "$UI_PORT" "--ui-port"

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
