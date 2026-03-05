#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
FW_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
DEFAULT_ENV="${ZACUS_PIO_ENV:-freenove_esp32s3_full_with_ui}"

usage() {
  cat <<USAGE
Usage: ./tools/dev/cockpit.sh <command> [env] [options]

PIO-only firmware cockpit.

Commands:
  help                     Show this help.
  envs                     List available PlatformIO environments.
  ports                    List serial devices (pio device list).
  build [env]              Build env (default: ${DEFAULT_ENV}).
  flash [env] [--port P]   Upload firmware for env.
  monitor [env] [--port P] [--baud B]
                           Open serial monitor for env.
  go [env] [--port P] [--baud B]
                           Build + flash + monitor.

Examples:
  ./tools/dev/cockpit.sh envs
  ./tools/dev/cockpit.sh build freenove_esp32s3
  ./tools/dev/cockpit.sh flash freenove_esp32s3 --port /dev/cu.usbmodemXXXX
  ./tools/dev/cockpit.sh monitor freenove_esp32s3 --baud 115200
USAGE
}

require_cmd() {
  if ! command -v "$1" >/dev/null 2>&1; then
    echo "[fail] missing command: $1" >&2
    exit 127
  fi
}

run_pio() {
  local -a cmd=(pio "$@")
  printf '$'
  printf ' %q' "${cmd[@]}"
  printf '\n'
  "${cmd[@]}"
}

ENV_NAME=""
PORT=""
BAUD=""
EXTRA_ARGS=()

parse_env_and_options() {
  ENV_NAME=""
  PORT=""
  BAUD=""
  EXTRA_ARGS=()

  while [[ $# -gt 0 ]]; do
    case "$1" in
      --port)
        if [[ $# -lt 2 ]]; then
          echo "[fail] --port requires a value" >&2
          exit 2
        fi
        PORT="$2"
        shift 2
        ;;
      --baud)
        if [[ $# -lt 2 ]]; then
          echo "[fail] --baud requires a value" >&2
          exit 2
        fi
        BAUD="$2"
        shift 2
        ;;
      -h|--help)
        usage
        exit 0
        ;;
      --)
        shift
        if [[ $# -gt 0 ]]; then
          EXTRA_ARGS+=("$@")
        fi
        break
        ;;
      *)
        if [[ -z "$ENV_NAME" ]]; then
          ENV_NAME="$1"
        else
          EXTRA_ARGS+=("$1")
        fi
        shift
        ;;
    esac
  done

  if [[ -z "$ENV_NAME" ]]; then
    ENV_NAME="$DEFAULT_ENV"
  fi
}

ensure_no_extra_args() {
  if [[ ${#EXTRA_ARGS[@]} -gt 0 ]]; then
    echo "[fail] unknown argument(s): ${EXTRA_ARGS[*]}" >&2
    exit 2
  fi
}

list_envs() {
  local json
  json="$(pio project config -d "$FW_ROOT" --json-output)"
  echo "$json" | grep -o '"env:[^"]*"' | sed -E 's/"env:(.*)"/\1/' | sort -u
}

cmd_build() {
  run_pio run -d "$FW_ROOT" -e "$ENV_NAME"
}

cmd_flash() {
  local -a args=(run -d "$FW_ROOT" -e "$ENV_NAME" -t upload)
  if [[ -n "$PORT" ]]; then
    args+=(--upload-port "$PORT")
  fi
  run_pio "${args[@]}"
}

cmd_monitor() {
  local -a args=(device monitor -d "$FW_ROOT" -e "$ENV_NAME")
  if [[ -n "$PORT" ]]; then
    args+=(--port "$PORT")
  fi
  if [[ -n "$BAUD" ]]; then
    args+=(--baud "$BAUD")
  fi
  run_pio "${args[@]}"
}

main() {
  require_cmd pio

  local command="${1:-help}"
  shift || true

  case "$command" in
    help|-h|--help)
      usage
      ;;
    envs)
      list_envs
      ;;
    ports)
      run_pio device list
      ;;
    build)
      parse_env_and_options "$@"
      ensure_no_extra_args
      cmd_build
      ;;
    flash)
      parse_env_and_options "$@"
      ensure_no_extra_args
      cmd_flash
      ;;
    monitor)
      parse_env_and_options "$@"
      ensure_no_extra_args
      cmd_monitor
      ;;
    go)
      parse_env_and_options "$@"
      ensure_no_extra_args
      cmd_build
      cmd_flash
      cmd_monitor
      ;;
    *)
      echo "[fail] unknown command: $command" >&2
      usage >&2
      exit 2
      ;;
  esac
}

main "$@"
