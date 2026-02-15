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


# Menu interactif harmonisé pour choisir un prompt (fzf/dialog/texte)

main_menu() {
  collect_prompts
  if [[ ${#prompt_files[@]} -eq 0 ]]; then
    echo "Unable to find prompt files under $PROMPT_DIR"
    exit 1
  fi
  while true; do
    local options=(
      "Prompt 1"
      "Prompt 2"
      "Prompt 3"
      "Aide"
      "Quitter"
    )
    local idx=$(menu_select "Menu Codex" "${options[@]}")
    case "$idx" in
      1) run_prompt 1 ;;
      2) run_prompt 2 ;;
      3) run_prompt 3 ;;
      4) afficher_aide ;;
      5|0) break ;;
    esac
  done

  afficher_aide() {
    echo -e "\n\033[1;36mAide Codex\033[0m"
    echo "- Utilisez les flèches ou le numéro pour naviguer."
    echo "- Installez fzf/dialog/whiptail pour une meilleure expérience."
    echo "- [Entrée] pour valider, [Échap] ou [Entrée] vide pour annuler."
    echo "- Pour toute question, voir README.md."
    read -n 1 -s -r -p "Appuyez sur une touche pour revenir au menu..."
  }
  done
}

run_prompt() {
  local prompt_path="$1"
  echo
  echo "Running $(basename "$prompt_path")"
  printf "Log (write-only): %s\n" "$LAST_MESSAGE_FILE"
  mkdir -p "$ARTIFACTS_DIR"
  (
    cd "$FW_ROOT"
    codex exec --sandbox workspace-write --output-last-message "$LAST_MESSAGE_FILE" - < "$prompt_path"
  )
}

main_menu
