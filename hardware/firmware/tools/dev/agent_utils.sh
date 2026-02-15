# --- Menu TUI harmonisé, i18n, AGENT ---

# Détection automatique de la langue
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

# Menu interactif harmonisé
menu_select() {
  local title="$1"; shift
  local options=("$@")
  local n=${#options[@]}
  local TUI_CMD=""
  if command -v fzf >/dev/null 2>&1; then
    TUI_CMD="fzf"
  elif command -v dialog >/dev/null 2>&1; then
    TUI_CMD="dialog"
  elif command -v whiptail >/dev/null 2>&1; then
    TUI_CMD="whiptail"
  fi
  if [[ "$TUI_CMD" == "fzf" ]]; then
    local reversed=()
    for ((i=n-1; i>=0; i--)); do reversed+=("$((i+1)): ${options[i]}"); done
    local choice=$(printf '%s\n' "${reversed[@]}" | fzf --ansi --prompt="❯ $title : " --header="[Flèches] naviguer, [Entrée] valider, [Échap] quitter" --height=15 --border)
    [[ -z "$choice" ]] && echo 0 && return
    echo "${choice%%:*}"
  elif [[ "$TUI_CMD" == "dialog" || "$TUI_CMD" == "whiptail" ]]; then
    local menu_args=()
    for ((i=0; i<n; i++)); do menu_args+=($((i+1)) "${options[i]}"); done
    local choice
    if [[ "$TUI_CMD" == "dialog" ]]; then
      choice=$(dialog --clear --title "$title" --menu "Sélectionnez :" 20 70 15 "${menu_args[@]}" 3>&1 1>&2 2>&3)
    else
      choice=$(whiptail --title "$title" --menu "Sélectionnez :" 20 70 15 "${menu_args[@]}" 3>&1 1>&2 2>&3)
    fi
    [[ -z "$choice" ]] && echo 0 && return
    echo "$choice"
  else
    echo -e "\033[1;33m$title\033[0m"
    for ((i=0; i<n; i++)); do echo "  $((i+1))) ${options[i]}"; done
    echo -en "Numéro du choix (ou Entrée pour annuler) : "
    read -r idx
    [[ -z "$idx" ]] && echo 0 && return
    if [[ "$idx" =~ ^[0-9]+$ ]] && (( idx >= 1 && idx <= n )); then
      echo "$idx"
    else
      echo 0
    fi
  fi
}
#!/usr/bin/env bash
set -euo pipefail
# Utilitaires AGENT pour build/smoke/logs/artefacts stricts

log() {
  local msg="$1"; echo "[AGENT] $msg" >&2
}

fail() {
  local msg="$1"; echo "[AGENT][FAIL] $msg" >&2; exit 1;
}

# Gate: build (sortie stricte, log)
build_gate() {
  log "Build gate: $*"
  "$@" 2>&1 | tee "${AGENT_LOG:-logs/agent_build.log}" || fail "Build failed ($*)"
}

# Gate: smoke (sortie stricte, log)
smoke_gate() {
  log "Smoke gate: $*"
  "$@" 2>&1 | tee "${AGENT_LOG:-logs/agent_smoke.log}" || fail "Smoke failed ($*)"
}

# Gate: artefact (copie stricte)
artefact_gate() {
  local src="$1"; local dest="$2"
  log "Artefact: $src -> $dest"
  cp -a "$src" "$dest" || fail "Artefact copy failed"
}

# Gate: logs (archive stricte)
logs_gate() {
  local src="$1"; local dest="$2"
  log "Logs: $src -> $dest"
  cp -a "$src" "$dest" || fail "Logs copy failed"
}

# Fast loop (parallélisation)
fast_loop() {
  log "Fast loop: $*"
  parallel "$@"
}
