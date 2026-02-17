#!/usr/bin/env bash
set -euo pipefail

tagmove(){
  echo "plan_runner usage: $0 --agent <name> [--dry-run] [--plan-only]" >&2
  exit 1
}
agent=""
dry_run=false
plan_only=false
while [[ $# -gt 0 ]]; do
  case "$1" in
    --agent)
      agent="$2"
      shift 2
      ;;
    --dry-run)
      dry_run=true
      shift
      ;;
    --plan-only)
      plan_only=true
      shift
      ;;
    --help)
      tagmove
      ;;
    *)
      tagmove
      ;;
  esac
done
if [[ -z "$agent" ]]; then
  tagmove
fi
file=".github/agents/${agent}.md"
if [[ ! -f "$file" ]]; then
  echo "Error: agent file $file not found" >&2
  exit 2
fi
plan=$(awk 'BEGIN {in_plan=0} /## Plan d’action/ {in_plan=1; next} /^## / && in_plan {exit} in_plan==1 {print}' "$file")
commands=()
while IFS= read -r line; do
  trimmed="${line#${line%%[![:space:]]*}}"
  if [[ "$trimmed" =~ ^-\ run:\ (.+)$ ]]; then
    commands+=("${BASH_REMATCH[1]}")
  fi
done <<< "$plan"
if [[ ${#commands[@]} -eq 0 ]]; then
  echo "No commands (- run:) found in plan section for $agent" >&2
  exit 3
fi
if [[ "$plan_only" == true ]]; then
  echo "Plan preview for agent: $agent"
  for idx in "${!commands[@]}"; do
    printf '[%d] %s\n' "$((idx+1))" "${commands[idx]}"
  done
  exit 0
fi
log_entry="$(date --iso-8601=seconds) plan_runner --agent $agent"
log_target="docs/AGENT_TODO.md"
for idx in "${!commands[@]}"; do
  cmd="${commands[idx]}"
  echo "[step $((idx+1))] $cmd"
  if [[ "$dry_run" == false ]]; then
    if ! bash -c "$cmd"; then
      echo "[fail] command failed: $cmd" >&2
      exit 5
    fi
  fi
done
if [[ -w "$log_target" ]]; then
  printf '%s - executed %d commands\n' "$log_entry" "${#commands[@]}" >> "$log_target"
fi
