#!/usr/bin/env bash
set -euo pipefail


# --- Codex Prompt Menu TUI ---
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(git -C "$SCRIPT_DIR/.." rev-parse --show-toplevel)"
FW_ROOT="$REPO_ROOT/hardware/firmware"
PROMPT_DIR="$FW_ROOT/tools/dev/codex_prompts"
ARTIFACTS_DIR="$FW_ROOT/artifacts/rc_live"
LAST_MESSAGE_FILE="$ARTIFACTS_DIR/_codex_last_message.md"

# Détection TUI (dialog/whiptail)
if command -v dialog >/dev/null 2>&1; then
  TUI_CMD="dialog"
elif command -v whiptail >/dev/null 2>&1; then
  TUI_CMD="whiptail"
else
  TUI_CMD=""
fi

collect_prompts() {
  local glob
  prompt_files=()
  for glob in "$PROMPT_DIR"/*.prompt*.md "$FW_ROOT/tools/dev"/*.prompt*.md; do
    [[ -f "$glob" ]] || continue
    prompt_files+=("$glob")
  done
}


# Menu interactif pour choisir un prompt (TUI si possible)
main_menu() {
  collect_prompts
  if [[ ${#prompt_files[@]} -eq 0 ]]; then
    echo "Unable to find prompt files under $PROMPT_DIR"
    exit 1
  fi
  while true; do
    local choice
    if [[ -n "$TUI_CMD" ]]; then
      local menu_args=()
      for idx in "${!prompt_files[@]}"; do
        menu_args+=("$idx" "$(basename "${prompt_files[idx]}")")
      done
      choice=$( \
        $TUI_CMD --clear --title "Codex Prompt Menu" \
          --menu "Sélectionnez un prompt :" 20 70 12 \
          "${menu_args[@]}" \
          3>&1 1>&2 2>&3
      )
      if [[ -z "$choice" ]]; then
        exit 0
      fi
      run_prompt "${prompt_files[choice]}"
    else
      echo
      echo "Available prompts:"
      for idx in "${!prompt_files[@]}"; do
        printf "%2d) %s\n" $((idx + 1)) "$(basename "${prompt_files[idx]}")"
      done
      read -rp "Choice (q to quit): " choice
      if [[ "$choice" =~ ^[Qq]$ ]]; then
        exit 0
      fi
      if ! [[ "$choice" =~ ^[0-9]+$ ]]; then
        echo "Please enter a number or 'q'."
        continue
      fi
      choice=$((choice - 1))
      if (( choice < 0 || choice >= ${#prompt_files[@]} )); then
        echo "Selection out of range."
        continue
      fi
      run_prompt "${prompt_files[choice]}"
    fi
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
