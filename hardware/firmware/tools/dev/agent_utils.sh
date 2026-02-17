# Shared helpers for firmware tooling

log() {
  local msg="$1"
  echo "[AGENT] $msg" >&2
}

fail() {
  local msg="$1"
  echo "[AGENT][FAIL] $msg" >&2
  exit 1
}

require_cmd() {
  if ! command -v "$1" >/dev/null 2>&1; then
    fail "missing command: $1"
  fi
}

resolve_ports_for_flash() {
  local fw_root
  fw_root=$(get_fw_root)
  local python_exec="$fw_root/.venv/bin/python3"
  if [[ ! -x "$python_exec" ]]; then
    python_exec="python3"
  fi
  local wait_secs="${ZACUS_PORT_WAIT:-5}"
  local allow_no_hw="${ZACUS_ALLOW_NO_HW:-0}"
  local require_rp2040="${ZACUS_REQUIRE_RP2040:-0}"
  local resolver="$fw_root/tools/test/resolve_ports.py"
  local ports_json="$1"

  local args=("--auto-ports" "--need-esp32" "--need-esp8266" "--wait-port" "$wait_secs" "--ports-resolve-json" "$ports_json")
  if [[ "$require_rp2040" == "1" ]]; then
    args+=("--need-rp2040")
  fi
  if [[ "$allow_no_hw" == "1" ]]; then
    args+=("--allow-no-hardware")
  fi
  if [[ -n "${ZACUS_PORT_ESP32:-}" ]]; then
    args+=("--port-esp32" "$ZACUS_PORT_ESP32")
  fi
  if [[ -n "${ZACUS_PORT_ESP8266:-}" ]]; then
    args+=("--port-esp8266" "$ZACUS_PORT_ESP8266")
  fi
  if [[ -n "${ZACUS_PORT_RP2040:-}" ]]; then
    args+=("--port-rp2040" "$ZACUS_PORT_RP2040")
  fi

  "$python_exec" "$resolver" "${args[@]}"
}

flash_all() {
  local fw_root
  fw_root=$(get_fw_root)
  require_cmd python3
  require_cmd pio

  local artifacts_root="$fw_root/artifacts/rc_live"
  local logs_dir="$fw_root/logs"
  local timestamp
  timestamp=$(date -u +"%Y%m%d-%H%M%S")
  local run_dir="$artifacts_root/flash-$timestamp"
  local log_file="$logs_dir/flash_$timestamp.log"
  local ports_json="$run_dir/ports_resolve.json"

  mkdir -p "$run_dir" "$logs_dir"
  log "[step] resolve ports"
  resolve_ports_for_flash "$ports_json" 2>&1 | tee "$log_file"

  local port_esp32 port_esp8266 port_rp2040
  local python_exec="$fw_root/.venv/bin/python3"
  if [[ ! -x "$python_exec" ]]; then
    python_exec="python3"
  fi
  # Correction : utilise un script Python temporaire pour éviter le bug de heredoc
  local tmp_py
  tmp_py=$(mktemp)
  cat > "$tmp_py" <<'PY'
import json, sys
try:
    data = json.load(open(sys.argv[1]))
except Exception:
    data = {}
ports = data.get("ports", {})
values = [ports.get("esp32", ""), ports.get("esp8266", ""), ports.get("rp2040", "")]
print(" ".join(values))
PY
  ports_out=$("$python_exec" "$tmp_py" "$ports_json")
  rm -f "$tmp_py"
  read -r port_esp32 port_esp8266 port_rp2040 <<< "$ports_out"

  if [[ -z "$port_esp32" || -z "$port_esp8266" ]]; then
    fail "port resolution failed (esp32=$port_esp32 esp8266=$port_esp8266)"
  fi

  local esp32_envs
  esp32_envs="${ZACUS_FLASH_ESP32_ENVS:-esp32dev}"
  local esp8266_env="${ZACUS_FLASH_ESP8266_ENV:-esp8266_oled}"
  local rp2040_envs
  rp2040_envs="${ZACUS_FLASH_RP2040_ENVS:-ui_rp2040_ili9488 ui_rp2040_ili9486}"
  local require_rp2040="${ZACUS_REQUIRE_RP2040:-0}"

  log "[step] flash esp32 ($esp32_envs)"
  for env in $esp32_envs; do
    log "[step] pio run -e $env -t upload --upload-port $port_esp32"
    (cd "$fw_root" && pio run -e "$env" -t upload --upload-port "$port_esp32") 2>&1 | tee -a "$log_file"
  done

  log "[step] flash esp8266 ($esp8266_env)"
  log "[step] pio run -e $esp8266_env -t upload --upload-port $port_esp8266"
  (cd "$fw_root" && pio run -e "$esp8266_env" -t upload --upload-port "$port_esp8266") 2>&1 | tee -a "$log_file"

  if [[ -n "$port_rp2040" ]]; then
    log "[step] flash rp2040 ($rp2040_envs)"
    for env in $rp2040_envs; do
      log "[step] pio run -e $env -t upload --upload-port $port_rp2040"
      (cd "$fw_root" && pio run -e "$env" -t upload --upload-port "$port_rp2040") 2>&1 | tee -a "$log_file"
    done
  elif [[ "$require_rp2040" == "1" ]]; then
    fail "rp2040 port missing (set ZACUS_PORT_RP2040 or connect board)"
  else
    log "[step] rp2040 not found; skipping rp2040 flash"
  fi

  log "flash artifacts: $run_dir"
  log "flash log: $log_file"
}

get_repo_root() {
  if [[ -n "${REPO_ROOT:-}" && -d "${REPO_ROOT}/.git" ]]; then
    printf '%s' "$REPO_ROOT"
    return 0
  fi
  git -C "$(dirname "${BASH_SOURCE[0]}")/../.." rev-parse --show-toplevel
}

get_fw_root() {
  if [[ -n "${FW_ROOT:-}" && -d "$FW_ROOT" ]]; then
    printf '%s' "$FW_ROOT"
    return 0
  fi
  printf '%s' "$(get_repo_root)/hardware/firmware"
}

write_git_info() {
  local repo_root="$1"
  local dest="$2"
  {
    echo "branch: $(git -C "$repo_root" rev-parse --abbrev-ref HEAD 2>/dev/null || echo n/a)"
    echo "commit: $(git -C "$repo_root" rev-parse HEAD 2>/dev/null || echo n/a)"
    echo "status:"
    git -C "$repo_root" status --porcelain 2>/dev/null || true
  } > "$dest"
}

write_meta_json() {
  local dest="$1"
  local phase="$2"
  local cmdline="$3"
  local fw_root
  fw_root=$(get_fw_root)
  local repo_root
  repo_root=$(get_repo_root)
  local now
  now=$(date -u +"%Y-%m-%dT%H:%M:%SZ")
  python3 - "$dest" <<'PY'
import json
import os
import sys

dest = sys.argv[1]
payload = {
    "timestamp": os.environ.get("EVIDENCE_TIMESTAMP", ""),
    "phase": os.environ.get("EVIDENCE_PHASE", ""),
    "utc": os.environ.get("EVIDENCE_UTC", ""),
    "command": os.environ.get("EVIDENCE_CMDLINE", ""),
    "cwd": os.getcwd(),
    "repo_root": os.environ.get("EVIDENCE_REPO_ROOT", ""),
    "fw_root": os.environ.get("EVIDENCE_FW_ROOT", ""),
}
with open(dest, "w", encoding="utf-8") as fp:
    json.dump(payload, fp, indent=2)
PY
}

evidence_init() {
  local phase="$1"
  local outdir="${2:-}"
  local fw_root
  fw_root=$(get_fw_root)
  local repo_root
  repo_root=$(get_repo_root)
  local stamp
  stamp=$(date -u +"%Y%m%d-%H%M%S")

  if [[ -n "$outdir" ]]; then
    if [[ "$outdir" = /* ]]; then
      EVIDENCE_DIR="$outdir"
    else
      EVIDENCE_DIR="$fw_root/$outdir"
    fi
  else
    EVIDENCE_DIR="$fw_root/artifacts/$phase/$stamp"
  fi

  EVIDENCE_PHASE="$phase"
  EVIDENCE_TIMESTAMP="$stamp"
  EVIDENCE_META="$EVIDENCE_DIR/meta.json"
  EVIDENCE_GIT="$EVIDENCE_DIR/git.txt"
  EVIDENCE_COMMANDS="$EVIDENCE_DIR/commands.txt"
  EVIDENCE_SUMMARY="$EVIDENCE_DIR/summary.md"
  EVIDENCE_UTC="$(date -u +"%Y-%m-%dT%H:%M:%SZ")"
  EVIDENCE_REPO_ROOT="$repo_root"
  EVIDENCE_FW_ROOT="$fw_root"
  export EVIDENCE_DIR EVIDENCE_PHASE EVIDENCE_TIMESTAMP EVIDENCE_META EVIDENCE_GIT EVIDENCE_COMMANDS EVIDENCE_SUMMARY EVIDENCE_UTC EVIDENCE_REPO_ROOT EVIDENCE_FW_ROOT

  mkdir -p "$EVIDENCE_DIR"
  write_git_info "$repo_root" "$EVIDENCE_GIT"
  printf '# Commands\n' > "$EVIDENCE_COMMANDS"
  write_meta_json "$EVIDENCE_META" "$phase" "${EVIDENCE_CMDLINE:-}"
}

evidence_record_command() {
  local cmd="$1"
  if [[ -n "${EVIDENCE_COMMANDS:-}" ]]; then
    printf '%s\n' "$cmd" >> "$EVIDENCE_COMMANDS"
  fi
}

git_cmd() {
  local repo_root
  repo_root=$(get_repo_root)
  if [[ -n "${EVIDENCE_COMMANDS:-}" ]]; then
    evidence_record_command "git $*"
  fi
  git -C "$repo_root" "$@"
}

# Git write operations: require ZACUS_GIT_ALLOW_WRITE=1
git_write_check() {
  if [[ "${ZACUS_GIT_ALLOW_WRITE:-0}" != "1" ]]; then
    fail "Git write operation requires ZACUS_GIT_ALLOW_WRITE=1"
  fi
  if [[ "${ZACUS_GIT_NO_CONFIRM:-0}" != "1" ]]; then
    local prompt="$1"
    echo "[WARN] $prompt" >&2
    echo -n "Continue? (y/N): " >&2
    local answer
    read -r answer
    if [[ "$answer" != "y" && "$answer" != "Y" ]]; then
      fail "Cancelled by user"
    fi
  fi
}

git_add() {
  git_write_check "git add $*"
  git_cmd add "$@"
}

git_commit() {
  git_write_check "git commit $*"
  git_cmd commit "$@"
}

git_stash() {
  git_write_check "git stash $*"
  git_cmd stash "$@"
}

git_push() {
  git_write_check "git push $*"
  git_cmd push "$@"
}

# Gate: build (strict, log)
build_gate() {
  log "Build gate: $*"
  "$@" 2>&1 | tee "${AGENT_LOG:-logs/agent_build.log}" || fail "Build failed ($*)"
}

# Gate: smoke (strict, log)
smoke_gate() {
  log "Smoke gate: $*"
  "$@" 2>&1 | tee "${AGENT_LOG:-logs/agent_smoke.log}" || fail "Smoke failed ($*)"
}

# Gate: artefact (strict copy)
artefact_gate() {
  local src="$1"
  local dest="$2"
  log "Artefact: $src -> $dest"
  cp -a "$src" "$dest" || fail "Artefact copy failed"
}

# Gate: logs (strict copy)
logs_gate() {
  local src="$1"
  local dest="$2"
  log "Logs: $src -> $dest"
  cp -a "$src" "$dest" || fail "Logs copy failed"
}

# Fast loop (parallel)
fast_loop() {
  log "Fast loop: $*"
  parallel "$@"
}

# Automated artefact/log analysis
analyse_artefacts_logs() {
  local fw_root
  fw_root=$(get_fw_root)
  local artefacts_dir="${1:-$fw_root/artifacts/rc_live}"
  local logs_dir="${2:-$fw_root/logs}"
  echo "[AGENT] Analyse artefacts/logs..."
  local last_flash_dir
  last_flash_dir=$(ls -1dt "$artefacts_dir"/flash-* 2>/dev/null | head -n1)
  if [[ -n "$last_flash_dir" && -f "$last_flash_dir/ports_resolve.json" ]]; then
    echo "--- Last port map (ports_resolve.json) ---"
    if command -v jq >/dev/null 2>&1; then
      jq . "$last_flash_dir/ports_resolve.json" || cat "$last_flash_dir/ports_resolve.json"
    else
      cat "$last_flash_dir/ports_resolve.json"
    fi
  else
    echo "[WARN] No ports_resolve.json artefact found."
  fi
  local build_log="$logs_dir/agent_build.log"
  if [[ -f "$build_log" ]]; then
    echo "--- Build summary (agent_build.log) ---"
    grep -E 'SUCCESS|FAIL|error|panic|Guru Meditation|rst:|abort|assert' "$build_log" || echo "[OK] No critical failure detected."
  else
    echo "[WARN] No build log found."
  fi
  find "$logs_dir" -maxdepth 1 -type f -name 'smoke_*' -print0 2>/dev/null \
    | xargs -0 ls -1t 2>/dev/null \
    | head -n 5 \
    | while IFS= read -r logf; do
        [[ -n "$logf" ]] || continue
        echo "--- Analyse $logf ---"
        grep -E 'FAIL|error|panic|Guru Meditation|rst:|abort|assert' "$logf" || echo "[OK] No critical failure detected."
      done
  echo "[AGENT] Analyse artefacts/logs done."
}

# Sync audit report
generate_sync_report() {
  local output_path="$1"
  local repo_root
  repo_root=$(get_repo_root)
  local fw_root
  fw_root=$(get_fw_root)
  local out="${output_path:-$fw_root/logs/audit_sync_report.md}"
  local base_ref
  base_ref=$(git -C "$repo_root" symbolic-ref --short refs/remotes/origin/HEAD 2>/dev/null || true)
  if [[ -z "$base_ref" ]]; then
    base_ref="origin/main"
  fi
  local now
  now=$(date -u +"%Y-%m-%d")
  local diffstat
  diffstat=$(git -C "$repo_root" diff --stat "$base_ref...HEAD" 2>/dev/null || true)
  local totals
  totals=$(printf '%s\n' "$diffstat" | tail -n1)
  local top_files
  top_files=$(git -C "$repo_root" diff --stat --stat-count=25 "$base_ref...HEAD" 2>/dev/null || true)
  mkdir -p "$(dirname "$out")"
  cat > "$out" <<EOF
# Sync audit report

Date: $now
Base: $base_ref
Branch: $(git -C "$repo_root" rev-parse --abbrev-ref HEAD)

## Summary
- Totals: ${totals:-no changes detected}

## Top changes (stat)
${top_files:-no diffstat available}

## Notes
- Generated by tools/dev/agent_utils.sh
- Scope: repo root and hardware/firmware tree
EOF
  log "Sync report written: $out"
}

# Cleanup old logs/artefacts
cleanup_audit_files() {
  local fw_root
  fw_root=$(get_fw_root)
  local logs_dir="${1:-$fw_root/logs}"
  local artifacts_dir="${2:-$fw_root/artifacts/rc_live}"
  local keep_days="${ZACUS_CLEANUP_KEEP_DAYS:-7}"
  local stamp
  stamp=$(date -u +"%Y%m%d-%H%M%S")
  local logs_archive="$logs_dir/archive/$stamp"
  local artifacts_archive="$fw_root/artifacts/archive/$stamp"
  mkdir -p "$logs_archive" "$artifacts_archive"
  local moved=0
  local log_count
  log_count=$(find "$logs_dir" -type f -mtime +"$keep_days" -print 2>/dev/null | wc -l | tr -d ' ')
  if [[ "$log_count" -gt 0 ]]; then
    find "$logs_dir" -type f -mtime +"$keep_days" -print0 2>/dev/null \
      | while IFS= read -r -d '' f; do
          mv "$f" "$logs_archive/"
        done
    moved=1
  fi
  local artifacts_count
  artifacts_count=$(find "$artifacts_dir" -mindepth 1 -maxdepth 1 -mtime +"$keep_days" -print 2>/dev/null | wc -l | tr -d ' ')
  if [[ "$artifacts_count" -gt 0 ]]; then
    find "$artifacts_dir" -mindepth 1 -maxdepth 1 -mtime +"$keep_days" -print0 2>/dev/null \
      | while IFS= read -r -d '' f; do
          mv "$f" "$artifacts_archive/"
        done
    moved=1
  fi
  if [[ "$moved" == "0" ]]; then
    log "cleanup: nothing to archive (keep_days=$keep_days)"
  else
    log "cleanup: archived logs to $logs_archive"
    log "cleanup: archived artefacts to $artifacts_archive"
  fi
}

prune_rc_live_runs() {
  local fw_root
  fw_root=$(get_fw_root)
  local rc_dir="${1:-$fw_root/artifacts/rc_live}"
  local keep_runs="${2:-2}"

  if [[ ! -d "$rc_dir" ]]; then
    return 0
  fi
  if ! [[ "$keep_runs" =~ ^[0-9]+$ ]]; then
    keep_runs=2
  fi

  local runs=()
  local run
  local had_nullglob=0
  if shopt -q nullglob; then
    had_nullglob=1
  else
    shopt -s nullglob
  fi

  for run in "$rc_dir"/[0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9]-[0-9][0-9][0-9][0-9][0-9][0-9]; do
    [[ -d "$run" ]] || continue
    runs+=("$(basename "$run")")
  done

  if [[ "$had_nullglob" == "0" ]]; then
    shopt -u nullglob
  fi

  if (( ${#runs[@]} <= keep_runs )); then
    return 0
  fi

  IFS=$'\n' runs=($(printf '%s\n' "${runs[@]}" | sort -r))
  unset IFS

  local to_prune=("${runs[@]:$keep_runs}")
  for run in "${to_prune[@]}"; do
    rm -rf "$rc_dir/$run" "$rc_dir/${run}_agent" "$rc_dir/${run}_logs"
  done
  log "rc_live prune: kept $keep_runs, removed ${#to_prune[@]}"
}

# Codex CLI check (prompts + command)
codex_cli_audit() {
  local fw_root
  fw_root=$(get_fw_root)
  local prompt_dir="$fw_root/tools/dev/codex_prompts"
  if command -v codex >/dev/null 2>&1; then
    log "codex: found"
  else
    log "codex: missing"
  fi
  if [[ -d "$prompt_dir" ]]; then
    log "codex prompts: $(ls -1 "$prompt_dir"/*.prompt*.md 2>/dev/null | wc -l | tr -d ' ') files"
  else
    log "codex prompts: missing dir $prompt_dir"
  fi
}

# Audit drivers per platform (placeholders for now)
drivers_audit() {
  local platform="$1"
  log "Audit drivers for $platform"
  case "$platform" in
    esp32dev|esp32_release)
      local drivers_ok=1
      local fw_root
      fw_root=$(get_fw_root)
      local req_files=(
        "$fw_root/esp32_audio/src/services/ui_serial/ui_serial.cpp"
        "$fw_root/esp32_audio/src/services/serial/serial_router.cpp"
        "$fw_root/protocol/ui_link_v2.h"
      )
      local f
      for f in "${req_files[@]}"; do
        if [[ ! -f "$f" ]]; then
          log "[WARN] missing driver file: $f"
          drivers_ok=0
        fi
      done
      if [[ "$drivers_ok" == "1" ]]; then
        log "[OK] drivers ESP32 ($platform) files present"
      fi
      ;;
    esp8266_oled)
      local drivers_ok=1
      local fw_root
      fw_root=$(get_fw_root)
      local req_files=(
        "$fw_root/ui/esp8266_oled/src/gfx/u8g2_display_backend.cpp"
        "$fw_root/ui/esp8266_oled/src/core/stat_parser.cpp"
        "$fw_root/ui/esp8266_oled/src/core/text_parser.cpp"
      )
      local f
      for f in "${req_files[@]}"; do
        if [[ ! -f "$f" ]]; then
          log "[WARN] missing driver file: $f"
          drivers_ok=0
        fi
      done
      if [[ "$drivers_ok" == "1" ]]; then
        log "[OK] drivers ESP8266 ($platform) files present"
      fi
      ;;
    ui_rp2040_ili9488|ui_rp2040_ili9486)
      local drivers_ok=1
      local fw_root
      fw_root=$(get_fw_root)
      local req_files=(
        "$fw_root/ui/rp2040_tft/src/lvgl_port.cpp"
        "$fw_root/ui/rp2040_tft/src/ui_renderer.cpp"
        "$fw_root/ui/rp2040_tft/src/uart_link.cpp"
      )
      local f
      for f in "${req_files[@]}"; do
        if [[ ! -f "$f" ]]; then
          log "[WARN] missing driver file: $f"
          drivers_ok=0
        fi
      done
      if [[ "$drivers_ok" == "1" ]]; then
        log "[OK] drivers RP2040 ($platform) files present"
      fi
      ;;
    *)
      fail "unknown platform for drivers_audit: $platform" ;;
  esac
}

# Audit tests per platform (placeholders for now)
tests_audit() {
  local platform="$1"
  log "Audit tests for $platform"
  case "$platform" in
    esp32dev|esp32_release)
      local fw_root
      fw_root=$(get_fw_root)
      local smoke_script="$fw_root/esp32_audio/tools/qa/story_v2_ci.sh"
      local rc_script="$fw_root/esp32_audio/tools/qa/live_story_v2_smoke.sh"
      local rtos_script="$fw_root/tools/dev/rtos_wifi_health.sh"
      local ok=1
      if [[ ! -f "$smoke_script" ]]; then
        log "[WARN] missing smoke script: $smoke_script"
        ok=0
      fi
      if [[ ! -f "$rc_script" ]]; then
        log "[WARN] missing RC smoke script: $rc_script"
        ok=0
      fi
      if [[ ! -f "$rtos_script" ]]; then
        log "[WARN] missing RTOS/WiFi health script: $rtos_script"
        ok=0
      fi
      if [[ "$ok" == "1" ]]; then
        log "[OK] tests ESP32 ($platform) scripts present"
      fi
      ;;
    esp8266_oled)
      local fw_root
      fw_root=$(get_fw_root)
      local suite="$fw_root/tools/test/run_serial_suite.py"
      local sim="$fw_root/tools/test/ui_link_sim.py"
      local ok=1
      if [[ ! -f "$suite" ]]; then
        log "[WARN] missing serial suite: $suite"
        ok=0
      fi
      if [[ ! -f "$sim" ]]; then
        log "[WARN] missing UI link sim: $sim"
        ok=0
      fi
      if [[ "$ok" == "1" ]]; then
        log "[OK] tests ESP8266 ($platform) scripts present"
      fi
      ;;
    ui_rp2040_ili9488|ui_rp2040_ili9486)
      local fw_root
      fw_root=$(get_fw_root)
      local runbook="$fw_root/ui/rp2040_tft/README.md"
      local spec="$fw_root/ui/rp2040_tft/UI_SPEC.md"
      local ok=1
      if [[ ! -f "$runbook" ]]; then
        log "[WARN] missing runbook: $runbook"
        ok=0
      fi
      if [[ ! -f "$spec" ]]; then
        log "[WARN] missing UI spec: $spec"
        ok=0
      fi
      if [[ "$ok" == "1" ]]; then
        log "[OK] tests RP2040 ($platform) docs present"
      fi
      ;;
    *)
      fail "unknown platform for tests_audit: $platform" ;;
  esac
}

# --- Menu TUI helpers ---
ZACUS_LANG=${ZACUS_LANG:-}
if [[ -z "$ZACUS_LANG" ]]; then
  case "${LANG:-}" in
    fr*|FR*) ZACUS_LANG=fr ;;
    en*|EN*) ZACUS_LANG=en ;;
    *) ZACUS_LANG=fr ;;
  esac
fi

menu_str() {
  local key="$1"
  case "$ZACUS_LANG" in
    fr)
      case "$key" in
        menu_title) echo "Zacus CLI" ;;
        opt_bootstrap) echo "Lancer RC Live" ;;
        opt_build) echo "Flasher + RC Live" ;;
        opt_flash) echo "RC Live + AutoFix" ;;
        opt_logs) echo "Voir les logs" ;;
        opt_help) echo "Aide" ;;
        opt_quit) echo "Quitter" ;;
        pause) echo "Appuyez sur une touche pour revenir au menu..." ;;
        bye) echo "Au revoir !" ;;
        *) echo "$key" ;;
      esac
      ;;
    en)
      case "$key" in
        menu_title) echo "Zacus CLI" ;;
        opt_bootstrap) echo "Run RC Live" ;;
        opt_build) echo "Flash + RC Live" ;;
        opt_flash) echo "RC Live + AutoFix" ;;
        opt_logs) echo "Show logs" ;;
        opt_help) echo "Help" ;;
        opt_quit) echo "Quit" ;;
        pause) echo "Press any key to return to menu..." ;;
        bye) echo "Goodbye!" ;;
        *) echo "$key" ;;
      esac
      ;;
    *)
      echo "$key" ;;
  esac
}

menu_select() {
  local title="$1"
  shift
  local options=("$@")
  local n=${#options[@]}
  local tui_cmd=""
  if command -v fzf >/dev/null 2>&1; then
    tui_cmd="fzf"
  elif command -v dialog >/dev/null 2>&1; then
    tui_cmd="dialog"
  elif command -v whiptail >/dev/null 2>&1; then
    tui_cmd="whiptail"
  fi
  if [[ "$tui_cmd" == "fzf" ]]; then
    local reversed=()
    for ((i=n-1; i>=0; i--)); do reversed+=("$((i+1)): ${options[i]}"); done
    local choice
    choice=$(printf '%s\n' "${reversed[@]}" | fzf --ansi --prompt="❯ $title : " --header="[Arrows] navigate, [Enter] select, [Esc] exit" --height=15 --border)
    [[ -z "$choice" ]] && echo 0 && return
    echo "${choice%%:*}"
  elif [[ "$tui_cmd" == "dialog" || "$tui_cmd" == "whiptail" ]]; then
    local menu_args=()
    for ((i=0; i<n; i++)); do menu_args+=($((i+1)) "${options[i]}"); done
    local choice
    if [[ "$tui_cmd" == "dialog" ]]; then
      choice=$(dialog --clear --title "$title" --menu "Select:" 20 70 15 "${menu_args[@]}" 3>&1 1>&2 2>&3)
    else
      choice=$(whiptail --title "$title" --menu "Select:" 20 70 15 "${menu_args[@]}" 3>&1 1>&2 2>&3)
    fi
    [[ -z "$choice" ]] && echo 0 && return
    echo "$choice"
  else
    echo -e "\033[1;33m$title\033[0m"
    for ((i=0; i<n; i++)); do echo "  $((i+1))) ${options[i]}"; done
    echo -en "Selection (or Enter to cancel): "
    read -r idx
    [[ -z "$idx" ]] && echo 0 && return
    if [[ "$idx" =~ ^[0-9]+$ ]] && (( idx >= 1 && idx <= n )); then
      echo "$idx"
    else
      echo 0
    fi
  fi
}
