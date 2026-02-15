#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
FW_ROOT="$REPO_ROOT/hardware/firmware"
RC_GATE="$FW_ROOT/tools/dev/run_matrix_and_smoke.sh"
SMOKE_CMD="$FW_ROOT/tools/dev/serial_smoke.py"

usage() {
  cat <<'USAGE'
Usage: zacus.sh <command> [args]
Commands:
  rc      Run the RC live gate (requires hardware)
  smoke   Invoke the serial smoke helper (passes args through)
  help    Show this usage
USAGE
}

command="${1:-help}"
shift || true
case "$command" in
  rc)
    (cd "$FW_ROOT" && "$RC_GATE")
    ;;
  smoke)
    (cd "$FW_ROOT" && python3 "$SMOKE_CMD" "$@")
    ;;
  help|-h|--help)
    usage
    ;;
  *)
    usage
    exit 1
    ;;
esac
