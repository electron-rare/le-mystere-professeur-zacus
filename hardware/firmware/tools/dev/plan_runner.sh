#!/usr/bin/env bash
set -euo pipefail

usage() {
  echo "plan_runner usage: $0 --agent <name> [--dry-run] [--plan-only]" >&2
  exit 1
}

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
FW_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
REPO_ROOT="$(cd "$FW_ROOT/../.." && pwd)"
cd "$REPO_ROOT"

agent=""
dry_run=false
plan_only=false
while [[ $# -gt 0 ]]; do
  case "$1" in
    --agent)
      if [[ $# -lt 2 ]]; then
        usage
      fi
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
      usage
      ;;
    *)
      usage
      ;;
  esac
done
if [[ -z "$agent" ]]; then
  usage
fi

AGENTS_ROOT="$FW_ROOT/.github/agents"
if [[ ! -d "$AGENTS_ROOT" ]]; then
  AGENTS_ROOT="$REPO_ROOT/.github/agents"
fi

agent_input="${agent%.md}"
agent_input="${agent_input#./}"
agent_lower="$(printf '%s' "$agent_input" | tr '[:upper:]' '[:lower:]')"
agent_norm="$(printf '%s' "$agent_lower" | tr '_' '-')"

agent_files=()
while IFS= read -r -d '' agent_file; do
  agent_files+=("$agent_file")
done < <(find "$AGENTS_ROOT" -type f -name '*.md' ! -path "$AGENTS_ROOT/archive/*" -print0 | sort -z)

if [[ ${#agent_files[@]} -eq 0 ]]; then
  echo "Error: no active agent files found under $AGENTS_ROOT" >&2
  exit 2
fi

find_unique_match() {
  local mode="$1"
  local matches=()
  local file rel id base id_lower base_lower id_norm base_norm
  for file in "${agent_files[@]}"; do
    rel="${file#$AGENTS_ROOT/}"
    id="${rel%.md}"
    base="$(basename "$id")"
    id_lower="$(printf '%s' "$id" | tr '[:upper:]' '[:lower:]')"
    base_lower="$(printf '%s' "$base" | tr '[:upper:]' '[:lower:]')"
    id_norm="$(printf '%s' "$id_lower" | tr '_' '-')"
    base_norm="$(printf '%s' "$base_lower" | tr '_' '-')"
    case "$mode" in
      exact_id)
        [[ "$agent_input" == "$id" ]] && matches+=("$file")
        ;;
      exact_id_ci)
        [[ "$agent_lower" == "$id_lower" ]] && matches+=("$file")
        ;;
      basename_ci)
        [[ "$agent_lower" == "$base_lower" ]] && matches+=("$file")
        ;;
      normalized)
        if [[ "$agent_norm" == "$id_norm" || "$agent_norm" == "$base_norm" ]]; then
          matches+=("$file")
        fi
        ;;
    esac
  done

  if [[ ${#matches[@]} -gt 1 ]]; then
    echo "Error: ambiguous agent identifier '$agent_input' (mode: $mode)" >&2
    echo "Candidates:" >&2
    local m rel_m
    for m in "${matches[@]}"; do
      rel_m="${m#$AGENTS_ROOT/}"
      printf '  - %s\n' "${rel_m%.md}" >&2
    done
    exit 4
  fi
  if [[ ${#matches[@]} -eq 1 ]]; then
    printf '%s\n' "${matches[0]}"
    return 0
  fi
  return 1
}

file=""
for mode in exact_id exact_id_ci basename_ci normalized; do
  if file="$(find_unique_match "$mode")"; then
    break
  fi
done

if [[ -z "$file" ]]; then
  echo "Error: no agent found for '$agent_input'" >&2
  echo "Tip: use a canonical id like 'domains/firmware-core'" >&2
  exit 2
fi

resolved_agent="${file#$AGENTS_ROOT/}"
resolved_agent="${resolved_agent%.md}"

plan=$(awk 'BEGIN {in_plan=0} /^## Plan d’action[[:space:]]*$/ || /^## Plan d'\''action[[:space:]]*$/ {in_plan=1; next} /^## / && in_plan {exit} in_plan==1 {print}' "$file")
commands=()
while IFS= read -r line; do
  trimmed="${line#${line%%[![:space:]]*}}"
  if [[ "$trimmed" =~ ^-\ run:\ (.+)$ ]]; then
    commands+=("${BASH_REMATCH[1]}")
  fi
done <<< "$plan"
if [[ ${#commands[@]} -eq 0 ]]; then
  echo "No commands (- run:) found in plan section for $resolved_agent" >&2
  exit 3
fi
if [[ "$plan_only" == true ]]; then
  echo "Plan preview for agent: $resolved_agent"
  for idx in "${!commands[@]}"; do
    printf '[%d] %s\n' "$((idx+1))" "${commands[idx]}"
  done
  exit 0
fi
log_entry="$(date -u +"%Y-%m-%dT%H:%M:%SZ") plan_runner --agent $resolved_agent"
log_target="$FW_ROOT/docs/AGENT_TODO.md"
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
