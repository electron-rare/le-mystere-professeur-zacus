#!/usr/bin/env bash
set -euo pipefail


# --- Zacus.sh TUI ---
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
ARTIFACT_ROOT="$REPO_ROOT/artifacts/rc_live"
RESOLVER="$REPO_ROOT/tools/test/resolve_ports.py"
RC_RUNNER="$REPO_ROOT/tools/dev/run_matrix_and_smoke.sh"
PROMPT_DIR="$SCRIPT_DIR/codex_prompts"

# Shared helpers (agent_utils.sh)
source "$SCRIPT_DIR/agent_utils.sh"

# Détection TUI (dialog/whiptail)
if command -v dialog >/dev/null 2>&1; then
  TUI_CMD="dialog"
elif command -v whiptail >/dev/null 2>&1; then
  TUI_CMD="whiptail"
else
  TUI_CMD=""
fi

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
  local summary_file="$1"
  local esp8266_log="$2"
  local port_status=""
  local ui_status=""
  if [[ -f "$summary_file" ]]; then
    read -r port_status ui_status < <(python3 - "$summary_file" <<'PY'
  if run_rc_gate; then
    log "RC live finished"
    return 0
  fi
  log "RC live failed"
  return 1
}

cmd_rc_autofix() {
    )
  fi
  local prompt="$PROMPT_DIR/auto_fix_generic.prompt.md"
  local reason="generic"
  # Utilise tr pour la compatibilité Bash (équivalent à ^^)
  if [[ "$(echo "$port_status" | tr '[:lower:]' '[:upper:]')" == "FAILED" ]]; then
    prompt="$PROMPT_DIR/auto_fix_ports.prompt.md"
    reason="port_status"
  elif [[ "$(echo "$ui_status" | tr '[:lower:]' '[:upper:]')" == "FAILED" ]]; then
    prompt="$PROMPT_DIR/auto_fix_ui_link.prompt.md"
    reason="ui_link"
  elif [[ -f "$esp8266_log" && $(grep -i "panic" "$esp8266_log" 2>/dev/null || true) ]]; then
    prompt="$PROMPT_DIR/auto_fix_esp8266_panic.prompt.md"
    reason="esp8266_panic"
  fi
  printf '%s\n%s\n' "$prompt" "$reason"
  if cmd_rc; then
    return 0
  fi
  local artifact
  artifact=$(latest_artifact_dir) || die "no artifact directory available after RC"
  log "artifact path: $artifact"
  require_cmd codex
  local prompt_info
  IFS=$'\n' read -r -d '' prompt_file prompt_reason < <(choose_autofix_prompt "$artifact/summary.json" "$artifact/smoke_esp8266_usb.log" && printf '\0')
  prompt_file="${prompt_file:-$PROMPT_DIR/auto_fix_generic.prompt.md}"
  prompt_reason="${prompt_reason:-generic}"
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
  # Optional: auto-commit changes if ZACUS_GIT_AUTOCOMMIT=1
  if [[ "${ZACUS_GIT_AUTOCOMMIT:-0}" == "1" ]]; then
    log "auto-committing changes (ZACUS_GIT_AUTOCOMMIT=1)"
    export ZACUS_GIT_ALLOW_WRITE=1
    if git_auto_commit "Auto-fix: $prompt_reason (via Codex)" "$artifact"; then
      log "committed changes successfully"
      printf '%s\n' "git_autocommit=success" >> "$artifact/rc_autofix.log"
    else
      log "auto-commit failed; continuing without commit"
      printf '%s\n' "git_autocommit=failed" >> "$artifact/rc_autofix.log"
    fi
  fi
  log "rerunning RC live after fix"
  run_rc_gate
}

git_auto_commit() {
  local msg="$1"
  local artifact="$2"
  
  # Check for modified files
  if ! git_cmd diff --quiet 2>/dev/null; then
    log "detected changes; staging and committing"
    git_add -A || return 1
    git_commit -m "$msg" || return 1
    printf '%s\n' "git_commit_msg=$msg" >> "$artifact/rc_autofix.log"
    return 0
  else
    log "no changes detected; skipping commit"
    return 0
  fi
}

cmd_flash() {
  flash_all
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
  rc-autofix    RC + codex autofix loop [+ optional git commit]
  ports         ports watch (15s)
  latest        show latest RC artifact path

Environment Variables:
  ZACUS_REQUIRE_HW=1          RC live requires hardware (wait for ports)
  ZACUS_GIT_AUTOCOMMIT=1      Auto-commit changes after codex fix
  ZACUS_GIT_ALLOW_WRITE=1     Allow git write operations (required for autocommit)
  ZACUS_GIT_NO_CONFIRM=1      Skip confirmation prompts (for CI/CD)

Examples:
  # Auto-fix with automatic git commit (CI/CD mode)
  ZACUS_GIT_AUTOCOMMIT=1 ZACUS_GIT_ALLOW_WRITE=1 ZACUS_GIT_NO_CONFIRM=1 zacus.sh rc-autofix

  # Auto-fix with manual commit approval
  ZACUS_GIT_AUTOCOMMIT=1 ZACUS_GIT_ALLOW_WRITE=1 zacus.sh rc-autofix
HELP
}

mkdir -p "$ARTIFACT_ROOT"





command=${1:-}
if [[ -z "$command" ]]; then
  usage
  exit 1
fi
if [[ -z "$command" ]]; then
  show_menu
  exit 0
fi


case "$command" in
  bootstrap)
    cmd_bootstrap; exit $? ;;
  build)
    cmd_build; exit $? ;;
  flash)
    cmd_flash; exit $? ;;
  rc)
    cmd_rc; exit $? ;;
  rc-autofix)
    cmd_rc_autofix; exit $? ;;
  ports)
    cmd_ports; exit $? ;;
  latest)
    cmd_latest; exit $? ;;
  afficher_aide)
    afficher_aide; exit 0 ;;
  *)
    usage
    exit 1
    ;;
esac

afficher_aide() {
  echo -e "\n\033[1;36mAide Zacus\033[0m"
  echo "- Utilisez les flèches ou le numéro pour naviguer."
  echo "- Installez fzf/dialog/whiptail pour une meilleure expérience."
  echo "- [Entrée] pour valider, [Échap] ou [Entrée] vide pour annuler."
  echo "- Logs : 15 dernières lignes de chaque fichier dans logs/"
  echo "- Pour toute question, voir README.md."
  read -n 1 -s -r -p "Appuyez sur une touche pour revenir au menu..."
}
