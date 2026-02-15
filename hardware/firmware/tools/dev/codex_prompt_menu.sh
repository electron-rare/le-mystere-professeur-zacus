#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(git -C "$SCRIPT_DIR/.." rev-parse --show-toplevel)"
FW_ROOT="$REPO_ROOT/hardware/firmware"
PROMPT_DIR="$FW_ROOT/tools/dev/codex_prompts"
ARTIFACTS_DIR="$FW_ROOT/artifacts/rc_live"
LAST_MESSAGE_FILE="$ARTIFACTS_DIR/_codex_last_message.md"

collect_prompts() {
  local glob
  prompt_files=()
  for glob in "$PROMPT_DIR"/*.prompt*.md "$FW_ROOT/tools/dev"/*.prompt*.md; do
    [[ -f "$glob" ]] || continue
    prompt_files+=("$glob")
  done
}

main_menu() {
  collect_prompts
  if [[ ${#prompt_files[@]} -eq 0 ]]; then
    echo "Unable to find prompt files under $PROMPT_DIR"
    exit 1
  fi
  while true; do
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
