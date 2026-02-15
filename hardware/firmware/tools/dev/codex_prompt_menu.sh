#!/usr/bin/env bash
set -euo pipefail


# --- Codex Prompt Menu TUI ---
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(git -C "$SCRIPT_DIR/.." rev-parse --show-toplevel)"
FW_ROOT="$REPO_ROOT/hardware/firmware"
PROMPT_DIR="$FW_ROOT/tools/dev/codex_prompts"
ARTIFACTS_DIR="$FW_ROOT/artifacts/rc_live"
LAST_MESSAGE_FILE="$ARTIFACTS_DIR/_codex_last_message.md"


# Centralise la logique TUI via agent_utils.sh
source "$(dirname "$0")/agent_utils.sh"

collect_prompts() {
  local glob
  prompt_files=()
  for glob in "$PROMPT_DIR"/*.prompt*.md "$FW_ROOT/tools/dev"/*.prompt*.md; do
    [[ -f "$glob" ]] || continue
    prompt_files+=("$glob")
  done
}


usage() {
  cat <<'HELP'
Usage: codex_prompt_menu.sh [--list] [--run <prompt_path>]
  --list              List available prompt files.
  --run <prompt_path> Run a specific prompt file.
  --help              Show this help.
HELP
}

afficher_aide() {
  echo -e "\n\033[1;36mAide Codex\033[0m"
  echo "- Utilisez les fleches ou le numero pour naviguer."
  echo "- Installez fzf/dialog/whiptail pour une meilleure experience."
  echo "- [Entree] pour valider, [Echap] ou [Entree] vide pour annuler."
  echo "- Pour toute question, voir README.md."
  read -n 1 -s -r -p "Appuyez sur une touche pour revenir au menu..."
}

list_prompts() {
  collect_prompts
  for f in "${prompt_files[@]}"; do
    printf '%s\n' "$f"
  done
}

main_menu() {
  collect_prompts
  if [[ ${#prompt_files[@]} -eq 0 ]]; then
    echo "Unable to find prompt files under $PROMPT_DIR"
    exit 1
  fi
  while true; do
    local options=()
    local f
    for f in "${prompt_files[@]}"; do
      options+=("$(basename "$f")")
    done
    options+=("Aide" "Quitter")
    local idx
    idx=$(menu_select "Menu Codex" "${options[@]}")
    if [[ "$idx" == "0" ]]; then
      break
    fi
    local total_prompts=${#prompt_files[@]}
    if (( idx >= 1 && idx <= total_prompts )); then
      run_prompt "${prompt_files[idx-1]}"
      continue
    fi
    if (( idx == total_prompts + 1 )); then
      afficher_aide
      continue
    fi
    break
  done
}

run_prompt() {
  local prompt_path="$1"
  if [[ -z "$prompt_path" || ! -f "$prompt_path" ]]; then
    echo "Prompt file not found: $prompt_path" >&2
    exit 1
  fi
  echo
  echo "Running $(basename "$prompt_path")"
  printf "Log (write-only): %s\n" "$LAST_MESSAGE_FILE"
  mkdir -p "$ARTIFACTS_DIR"
  (
    cd "$FW_ROOT"
    codex exec --sandbox workspace-write --output-last-message "$LAST_MESSAGE_FILE" - < "$prompt_path"
  )
}

case "${1:-}" in
  --help|-h)
    usage
    exit 0
    ;;
  --list)
    list_prompts
    exit 0
    ;;
  --run)
    run_prompt "${2:-}"
    exit 0
    ;;
esac

main_menu
