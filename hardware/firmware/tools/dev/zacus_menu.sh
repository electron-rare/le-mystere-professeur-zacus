#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(git -C "$SCRIPT_DIR/../.." rev-parse --show-toplevel)"
FW_ROOT="$REPO_ROOT/hardware/firmware"
ARTIFACTS_ROOT="$FW_ROOT/artifacts/rc_live"
PROMPT_DIR="$FW_ROOT/tools/dev/codex_prompts"
RC_PROMPT="$PROMPT_DIR/rc_live_fail.prompt.md"

run_bootstrap() {
  (cd "$REPO_ROOT" && ./tools/dev/bootstrap_local.sh)
}

run_build_all() {
  (cd "$FW_ROOT" && ./build_all.sh)
}

run_rc_live() {
  local artifacts=""
  if ! (cd "$FW_ROOT" && ./tools/dev/run_matrix_and_smoke.sh); then
    artifacts="$(latest_artifacts)"
    if [[ -n "$artifacts" && -f "$RC_PROMPT" ]]; then
      ARTIFACT_PATH="$artifacts" codex exec - < "$RC_PROMPT"
    elif [[ -f "$RC_PROMPT" ]]; then
      codex exec - < "$RC_PROMPT"
    fi
    return 1
  fi
}

ports_watch() {
  echo "Press Ctrl+C to exit ports watch."
  while true; do
    echo "=== $(date -R) ==="
    python3 -m serial.tools.list_ports -v || true
    sleep 5
  done
}

latest_artifacts() {
  if [[ ! -d "$ARTIFACTS_ROOT" ]]; then
    echo ""
    return
  fi
  ls -1d "$ARTIFACTS_ROOT"/*/ 2>/dev/null | sort | tail -n1
}

menu() {
  cat <<'EOF'
Firmware cockpit
1) bootstrap (tools/dev/bootstrap_local.sh)
2) build all firmware (hardware/firmware/build_all.sh)
3) rc live gate (ZACUS_REQUIRE_HW unset)
4) rc live gate (ZACUS_REQUIRE_HW=1)
5) watch serial ports
6) run codex prompt menu
0) exit
EOF
}

run_codex_prompts() {
  "$FW_ROOT/tools/dev/codex_prompt_menu.sh"
}

while true; do
  menu
  read -rp "Choice: " choice
  case "$choice" in
    1) run_bootstrap ;;
    2) run_build_all ;;
    3) ZACUS_REQUIRE_HW=0 run_rc_live ;;
    4) ZACUS_REQUIRE_HW=1 run_rc_live ;;
    5) ports_watch ;;
    6) run_codex_prompts ;;
    0) exit 0 ;;
    *) echo "Unknown option: $choice" ;;
  esac
done
