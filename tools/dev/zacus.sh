#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
FW_ROOT="$REPO_ROOT/hardware/firmware"
RC_RUNNER="$FW_ROOT/tools/dev/run_matrix_and_smoke.sh"
SMOKE_SCRIPT="$FW_ROOT/tools/dev/serial_smoke.py"
ZEROCLAW_PREFLIGHT="$SCRIPT_DIR/zeroclaw_hw_preflight.sh"
PROMPT_DIR="$SCRIPT_DIR/codex_prompts"
ARTIFACT_ROOT="$REPO_ROOT/artifacts"
PORTS_ARTIFACT_ROOT="$ARTIFACT_ROOT/ports"
LATEST_PORTS_JSON="$PORTS_ARTIFACT_ROOT/latest_ports_resolve.json"
CODEX_ARTIFACT_ROOT="$ARTIFACT_ROOT/codex"
DEFAULT_PORTS_FIXTURE="$REPO_ROOT/tools/test/fixtures/ports_list_macos.txt"
LAST_RESOLVED_JSON=""

info() {
  printf '[zacus] %s\n' "$*"
}

die() {
  printf '[zacus] ERROR: %s\n' "$*" >&2
  exit 1
}

die_codex() {
  printf '[zacus] ERROR: %s\n' "$*" >&2
  exit 2
}

usage() {
  cat <<'USAGE'
Usage: zacus.sh <command> [args]
Commands:
  rc       Run the RC live gate (always strict)
  smoke    Run the serial smoke helper (pass --role ... args)
  ports    Resolve ports once and show summary
  zeroclaw-preflight  Run ZeroClaw USB preflight before hardware actions
  codex    Invoke Codex CLI with a prompt
  menu     Show the Zacus cockpit menu
  help     Show this usage
USAGE
}

print_ports_summary() {
  local json_file="$1"
  if [[ ! -f "$json_file" ]]; then
    printf '[ports] no resolved data yet (run "zacus.sh ports" to resolve)\n'
    return 1
  fi
  python3 - "$json_file" <<'PY'
import datetime
import json
import pathlib
import sys

path = pathlib.Path(sys.argv[1])
try:
    data = json.loads(path.read_text(encoding='utf-8'))
    ports = data.get('ports', {})
    reasons = data.get('reasons', {})
    notes = data.get('notes', [])
    labels = [('esp32', 'esp32_audio'), ('esp8266', 'esp8266_oled')]
    timestamp = path.parent.name
    age = 'unknown'
    try:
        dt = datetime.datetime.strptime(timestamp, '%Y%m%d-%H%M%S')
        dt = dt.replace(tzinfo=datetime.timezone.utc)
        age_delta = datetime.datetime.now(datetime.timezone.utc) - dt
        age = str(age_delta).split('.')[0]
    except Exception:
        pass
    print(f'[ports] summary (json={path.name}) timestamp={timestamp} age={age}')
    for role, label in labels:
        port = ports.get(role) or 'n/a'
        reason = reasons.get(role) or 'unset'
        print(f'  - {label}: {port} (reason={reason})')
    print('  - rp2040_tft: not auto-resolved (manual selection)')
    print(f'[ports] json path: {path}')
    print('[ports] notes:', ' | '.join(notes) if notes else 'none')
except Exception as exc:
    print(f'[ports] summary unavailable: {exc}')
PY
}

resolve_ports_once() {
  mkdir -p "$PORTS_ARTIFACT_ROOT"
  local timestamp
  timestamp=$(date -u +%Y%m%d-%H%M%S)
  local dir="$PORTS_ARTIFACT_ROOT/$timestamp"
  mkdir -p "$dir"
  local json="$dir/ports_resolve.json"
  local resolver_args=(python3 "$REPO_ROOT/tools/test/resolve_ports.py" \
    --need-esp32 --need-esp8266 --ports-resolve-json "$json" --wait-port 3)
  local use_fixture=0
  local fixture_path=""
  if [[ "${ZACUS_MOCK_PORTS:-0}" == "1" ]]; then
    use_fixture=1
    fixture_path="${ZACUS_PORTS_FIXTURE:-$DEFAULT_PORTS_FIXTURE}"
  elif [[ -n "${ZACUS_PORTS_FIXTURE:-}" ]]; then
    use_fixture=1
    fixture_path="${ZACUS_PORTS_FIXTURE}"
  fi
  if (( use_fixture == 1 )); then
    resolver_args+=(--mock)
    if [[ -n "$fixture_path" && -f "$fixture_path" ]]; then
      resolver_args+=(--ports-fixture "$fixture_path")
    else
      info "ports fixture not found: $fixture_path"
    fi
  fi
  info "resolving ports (output -> $json)"
  set +e
  "${resolver_args[@]}"
  local rc=$?
  set -e
  if [[ -f "$json" ]]; then
    cp "$json" "$LATEST_PORTS_JSON" 2>/dev/null || true
    LAST_RESOLVED_JSON="$json"
  fi
  return $rc
}

show_ports_context() {
  local candidate_json
  if [[ -n "${LAST_RESOLVED_JSON}" && -f "$LAST_RESOLVED_JSON" ]]; then
    candidate_json="$LAST_RESOLVED_JSON"
  elif [[ -f "$LATEST_PORTS_JSON" ]]; then
    candidate_json="$LATEST_PORTS_JSON"
  else
    printf '[ports] no resolution data yet\n'
    return 1
  fi
  print_ports_summary "$candidate_json" || true
  if [[ -f "$LATEST_PORTS_JSON" ]]; then
    printf '[ports] pointer: %s\n' "$LATEST_PORTS_JSON"
  fi
}

cmd_ports() {
  resolve_ports_once || true
  local json_file=""
  if [[ -n "${LAST_RESOLVED_JSON}" && -f "$LAST_RESOLVED_JSON" ]]; then
    json_file="$LAST_RESOLVED_JSON"
  elif [[ -f "$LATEST_PORTS_JSON" ]]; then
    json_file="$LATEST_PORTS_JSON"
  fi
  if [[ -n "$json_file" ]]; then
    print_ports_summary "$json_file" || true
    printf '[ports] data file: %s\n' "$json_file"
  else
    printf '[ports] resolver did not produce a JSON file\n'
  fi
  if [[ -f "$LATEST_PORTS_JSON" ]]; then
    printf '[ports] latest pointer: %s\n' "$LATEST_PORTS_JSON"
  fi
}

cmd_rc() {
  info "running RC live"
  (cd "$FW_ROOT" && "$RC_RUNNER")
}

cmd_smoke() {
  info "running serial smoke"
  (cd "$FW_ROOT" && python3 "$SMOKE_SCRIPT" "$@")
}

cmd_zeroclaw_preflight() {
  if [[ ! -x "$ZEROCLAW_PREFLIGHT" ]]; then
    die "missing preflight script: $ZEROCLAW_PREFLIGHT"
  fi
  info "running zeroclaw hardware preflight"
  "$ZEROCLAW_PREFLIGHT" "$@"
}

ensure_codex_ready() {
  if ! command -v codex >/dev/null 2>&1; then
    die_codex "codex CLI not found. Install it from https://app.codex.com/docs/cli"
  fi
  if ! codex login status >/dev/null 2>&1; then
    die_codex "codex CLI is not logged in. Run 'codex login' before continuing."
  fi
}

cmd_codex() {
  local prompt_file=""
  local sandbox="workspace-write"
  local auto_mode="0"
  while [[ $# -gt 0 ]]; do
    case "$1" in
      --prompt)
        shift
        prompt_file="${1:-}"
        ;;
      --sandbox)
        shift
        sandbox="${1:-}"
        ;;
      --auto)
        auto_mode="1"
        ;;
      --help|-h)
        cat <<'HELP'
Usage: zacus.sh codex --prompt <file> [--sandbox <policy>] [--auto]
Runs codex exec with the provided prompt. Defaults to --sandbox workspace-write.
Use --auto to add --full-auto to the codex exec command.
HELP
        return 0
        ;;
      *)
        die "unexpected option: $1"
        ;;
    esac
    shift
  done
  if [[ -z "$prompt_file" ]]; then
    die "--prompt <file> is required"
  fi
  if [[ ! -f "$prompt_file" ]]; then
    die "prompt file $prompt_file not found"
  fi
  ensure_codex_ready
  resolve_ports_once || true
  local ports_json="${LAST_RESOLVED_JSON:-$LATEST_PORTS_JSON}"
  if [[ -n "$ports_json" && -f "$ports_json" ]]; then
    print_ports_summary "$ports_json" || true
  fi
  mkdir -p "$CODEX_ARTIFACT_ROOT"
  local ts
  ts=$(date -u +%Y%m%d-%H%M%S)
  local art_dir="$CODEX_ARTIFACT_ROOT/$ts"
  mkdir -p "$art_dir"
  if [[ -n "$ports_json" && -f "$ports_json" ]]; then
    cp "$ports_json" "$art_dir/ports_resolve.json" 2>/dev/null || true
  fi
  printf 'prompt=%s\nsandbox=%s\nauto=%s\n' "$prompt_file" "$sandbox" "$auto_mode" > "$art_dir/metadata.txt"
  local exec_cmd=(codex exec --sandbox "$sandbox")
  if [[ "$auto_mode" == "1" ]]; then
    exec_cmd+=(--full-auto)
  fi
  exec_cmd+=(-)
  local log_file="$art_dir/exec.log"
  info "running codex exec (log=$log_file)"
  set +e
  "${exec_cmd[@]}" < "$prompt_file" 2>&1 | tee "$log_file"
  local codex_rc=${PIPESTATUS[0]:-1}
  set -e
  printf '[codex] artifact directory: %s\n' "$art_dir"
  return $codex_rc
}

list_prompts() {
  local -a prompts=()
  if [[ -d "$PROMPT_DIR" ]]; then
    while IFS= read -r -d $'\0' file; do
      prompts+=("$file")
    done < <(find "$PROMPT_DIR" -maxdepth 1 -name '*.md' -print0 | sort -z)
  fi
  printf '%s\n' "${prompts[@]:-}"
}

menu_select_prompt() {
  mapfile -t choices < <(find "$PROMPT_DIR" -maxdepth 1 -name '*.md' | sort)
  if [[ ${#choices[@]} -eq 0 ]]; then
    die "no prompt files found under $PROMPT_DIR"
  fi
  printf 'Available prompts:\n'
  local idx=1
  for prompt in "${choices[@]}"; do
    printf '%2d) %s\n' "$idx" "$(basename "$prompt")"
    ((idx++))
  done
  while true; do
    read -rp 'Select prompt number: ' sel
    if [[ ! "$sel" =~ ^[0-9]+$ ]]; then
      printf 'invalid choice\n'
      continue
    fi
    if (( sel >= 1 && sel <= ${#choices[@]} )); then
      printf '%s' "${choices[sel-1]}"
      return 0
    fi
    printf 'choice out of range\n'
  done
}

menu_run_codex() {
  local prompt_file
  if [[ -n "$1" ]]; then
    prompt_file="$1"
  else
    prompt_file=$(menu_select_prompt)
  fi
  local sandbox_choice
  read -rp 'Sandbox (w=workspace-write [default], d=danger-full-access): ' sandbox_choice
  sandbox_choice=${sandbox_choice:-w}
  local sandbox='workspace-write'
  if [[ "$sandbox_choice" == 'd' ]]; then
    sandbox='danger-full-access'
  fi
  local auto_choice
  read -rp 'Auto-run (y/N)? ' auto_choice
  auto_choice=${auto_choice:-n}
  local auto_flag=''
  if [[ "$auto_choice" =~ ^[Yy]$ ]]; then
    auto_flag='--auto'
  fi
  cmd_codex --prompt "$prompt_file" --sandbox "$sandbox" $auto_flag
}

cmd_menu() {
  local non_tui=0
  while [[ $# -gt 0 ]]; do
    case "$1" in
      --no-tui)
        non_tui=1
        ;;
      --help|-h)
        usage
        return 0
        ;;
      *)
        die "unknown menu option: $1"
        ;;
    esac
    shift
  done
  if (( non_tui == 1 )); then
    cmd_ports
    return 0
  fi
  while true; do
    printf '\n=== Zacus cockpit ===\n'
    show_ports_context || true
    printf '[menu] options:\n'
    printf '  1) RC live\n'
    printf '  2) Serial smoke\n'
    printf '  3) Codex (run prompt)\n'
    printf '  4) Ports: resolve now\n'
    printf '  5) ZeroClaw preflight\n'
    printf '  0) Exit\n'
    read -rp 'Choice: ' choice
    case "$choice" in
      1)
        cmd_rc
        ;;
      2)
        cmd_smoke
        ;;
      3)
        menu_run_codex
        ;;
      4)
        cmd_ports
        ;;
      5)
        cmd_zeroclaw_preflight --require-port
        ;;
      0)
        return 0
        ;;
      *)
        printf 'unknown choice\n'
        ;;
    esac
  done
}

command="${1:-help}"
shift || true
case "$command" in
  rc)
    cmd_rc
    ;;
  smoke)
    cmd_smoke "$@"
    ;;
  ports)
    cmd_ports
    ;;
  zeroclaw-preflight|zc-preflight)
    cmd_zeroclaw_preflight "$@"
    ;;
  codex)
    cmd_codex "$@"
    ;;
  menu)
    cmd_menu "$@"
    ;;
  help|-h|--help)
    usage
    ;;
  *)
    usage
    exit 1
    ;;
esac
