#!/usr/bin/env bash

set -euo pipefail

# DEBUG: Affichage de début de script
echo "[DEBUG] Lancement du cockpit Zacus..." >&2
sleep 0.1
export LC_ALL=C
export LANG=C


# --- Zacus Menu TUI ---
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(git -C "$SCRIPT_DIR/../.." rev-parse --show-toplevel)"
FW_ROOT="$REPO_ROOT/hardware/firmware"
ARTIFACTS_ROOT="$FW_ROOT/artifacts/rc_live"
PROMPT_DIR="$FW_ROOT/tools/dev/codex_prompts"
RC_PROMPT="$PROMPT_DIR/rc_live_fail.prompt.md"



# Utilitaire de menu factorisé (centralisé dans agent_utils.sh)
source "$(dirname "$0")/agent_utils.sh"



run_bootstrap() {
  "$FW_ROOT/tools/dev/bootstrap_local.sh"
}

run_build_all() {
  "$FW_ROOT/build_all.sh"
}

run_sync_report() {
  generate_sync_report "$FW_ROOT/logs/audit_sync_report.md"
}

run_cleanup() {
  cleanup_audit_files
}

run_codex_check() {
  codex_cli_audit
}

run_audit_all() {
  run_build_all
  if [[ "${ZACUS_REQUIRE_HW:-0}" == "1" ]]; then
    ZACUS_REQUIRE_HW=1 ZACUS_SKIP_PIO=1 "$FW_ROOT/tools/dev/run_matrix_and_smoke.sh"
  else
    ZACUS_REQUIRE_HW=0 ZACUS_SKIP_PIO=1 ZACUS_NO_COUNTDOWN=1 "$FW_ROOT/tools/dev/run_matrix_and_smoke.sh"
  fi
  analyse_artefacts_logs
  local platform
  for platform in esp32dev esp32_release esp8266_oled ui_rp2040_ili9488 ui_rp2040_ili9486; do
    drivers_audit "$platform"
    tests_audit "$platform"
  done
  run_codex_check
  run_sync_report
}

run_rc_live() {
  local artifacts=""
  if ! "$FW_ROOT/tools/dev/run_matrix_and_smoke.sh"; then
    artifacts="$(latest_artifacts)"
    if [[ -n "$artifacts" && -f "$RC_PROMPT" ]]; then
      local codex_last="$artifacts/codex_last_message.md"
      local cmd="codex exec --output-last-message $codex_last - < $RC_PROMPT"
      if [[ -f "$artifacts/commands.txt" ]]; then
        printf '%s\n' "$cmd" >> "$artifacts/commands.txt"
      fi
      ARTIFACT_PATH="$artifacts" codex exec --output-last-message "$codex_last" - < "$RC_PROMPT"
    elif [[ -f "$RC_PROMPT" ]]; then
      codex exec - < "$RC_PROMPT"
    fi
    return 1
  fi
}

ports_watch() {
  echo "Press Ctrl+C to exit ports watch."
  local PYTHON_VENV3="$FW_ROOT/.venv/bin/python3"
  local PYTHON_VENV="$FW_ROOT/.venv/bin/python"
  if [[ -x "$PYTHON_VENV3" ]]; then
    PYTHON_EXEC="$PYTHON_VENV3"
  elif [[ -x "$PYTHON_VENV" ]]; then
    PYTHON_EXEC="$PYTHON_VENV"
  else
    echo "[AGENT][FAIL] Aucun interpréteur Python .venv trouvé pour la détection des ports." >&2
    exit 1
  fi
  while true; do
    echo "=== $(date -R) ==="
    "$PYTHON_EXEC" -m serial.tools.list_ports -v || true
    sleep 5
  done
}

latest_artifacts() {
  if [[ ! -d "$ARTIFACTS_ROOT" ]]; then
    echo ""
    return
  fi
  ls -1d "$ARTIFACTS_ROOT"/*/ 2>/dev/null | sort | tail -n1
}


show_menu() {
  local options=(
    "RC live gate (ZACUS_REQUIRE_HW unset)"
    "RC live gate (ZACUS_REQUIRE_HW=1)"
    "Build all firmware (hardware/firmware/build_all.sh)"
    "Bootstrap (outils/dev/bootstrap_local.sh)"
    "Watch serial ports"
    "Monitor WiFi debug (serial ESP32)"
    "Run codex prompt menu"
    "Audit full (build + rc + drivers/tests)"
    "Generate sync report"
    "Cleanup logs/artefacts"
    "Codex CLI check"
    "Afficher logs (firmware/logs/)"
    "Aide"
    "Quitter"
  )
  while true; do
    local idx=$(menu_select "Zacus Firmware Cockpit" "${options[@]}")
    case "$idx" in
      1) ZACUS_REQUIRE_HW=0 run_rc_live ;;
      2) ZACUS_REQUIRE_HW=1 run_rc_live ;;
      3) run_build_all ;;
      4) run_bootstrap ;;
      5) ports_watch ;;
      6) monitor_wifi_debug_serial ;;
      7) run_codex_prompts ;;
      8) run_audit_all ;;
      9) run_sync_report ;;
      10) run_cleanup ;;
      11) run_codex_check ;;
      12) afficher_logs_menu ;;
      13) afficher_aide ;;
      14|0) exit 0 ;;
    esac
  done
}

# --- WiFi Debug Serial Monitor (ESP32) ---
monitor_wifi_debug_serial() {
  echo -e "\n\033[1;36m[WiFi Debug Serial] Monitoring ESP32 serial port (logs, WiFi, scan, connect, errors)\033[0m"
  local PYTHON_VENV3="$FW_ROOT/.venv/bin/python3"
  local PYTHON_VENV="$FW_ROOT/.venv/bin/python"
  if [[ -x "$PYTHON_VENV3" ]]; then
    PYTHON_EXEC="$PYTHON_VENV3"
  elif [[ -x "$PYTHON_VENV" ]]; then
    PYTHON_EXEC="$PYTHON_VENV"
  else
    echo "[AGENT][FAIL] Aucun interpréteur Python .venv trouvé pour le debug série." >&2
    exit 1
  fi
  # Lancement du script serial_smoke.py en mode monitor-only sur l'ESP32
  "$PYTHON_EXEC" "$FW_ROOT/tools/dev/serial_smoke.py" --role esp32 --baud 115200 --timeout 1.0
  echo -e "\033[1;36m--- Fin du monitoring série ---\033[0m"
  read -n 1 -s -r -p "Appuyez sur une touche pour revenir au menu..."
}

afficher_aide() {
  echo -e "\n\033[1;36mAide Zacus Cockpit\033[0m"
  echo "- Utilisez les flèches ou le numéro pour naviguer."
  echo "- 'Monitor WiFi debug (serial ESP32)' : lance le monitoring live du port série ESP32 (logs WiFi, scan, connect, erreurs)."
  echo "- Installez fzf/dialog/whiptail pour une meilleure expérience."
  echo "- [Entrée] pour valider, [Échap] ou [Entrée] vide pour annuler."
  echo "- Logs : 15 dernières lignes de chaque fichier dans logs/"
  echo "- Pour toute question, voir README.md."
  read -n 1 -s -r -p "Appuyez sur une touche pour revenir au menu..."
}

run_codex_prompts() {
  "$FW_ROOT/tools/dev/codex_prompt_menu.sh"
}

run_git() {
  local action="${1:-status}"
  shift || true
  case "$action" in
    status)
      git_cmd status "$@"
      ;;
    diff)
      git_cmd diff "$@"
      ;;
    log)
      local count="20"
      if [[ "${1:-}" =~ ^[0-9]+$ ]]; then
        count="$1"
        shift
      fi
      git_cmd log --oneline -n "$count" "$@"
      ;;
    branch)
      git_cmd branch -vv
      ;;
    show)
      git_cmd show "$@"
      ;;
    add)
      git_add "$@"
      ;;
    commit)
      git_commit "$@"
      ;;
    stash)
      git_stash "$@"
      ;;
    push)
      git_push "$@"
      ;;
    *)
      fail "Unknown git action: $action. Use: status, diff, log, branch, show, add, commit, stash, push"
      ;;
  esac
}




# Sous-menu pour afficher les logs (doit être défini avant la boucle principale)
afficher_logs_menu() {
  local logs_dir="$FW_ROOT/logs"
  local files=()
  local file
  # Liste les fichiers de log (récents d'abord)
  while IFS= read -r -d '' f; do
    files+=("$f")
  done < <(find "$logs_dir" -type f -print0 | sort -z -r)
  if [[ ${#files[@]} -eq 0 ]]; then
    echo "Aucun fichier de log trouvé dans $logs_dir." >&2
    sleep 2
    return
  fi
  local choix
  if [[ "$TUI_CMD" == "fzf" ]]; then
    choix=$(printf '%s\n' "${files[@]}" | fzf --ansi --prompt="❯ Sélectionnez un log : " --header="[Flèches] naviguer, [Entrée] afficher, [Échap] retour" --height=15 --border)
    [[ -z "$choix" ]] && return
  elif [[ "$TUI_CMD" == "dialog" || "$TUI_CMD" == "whiptail" ]]; then
    local menu_args=()
    local idx=1
    for f in "${files[@]}"; do
      menu_args+=("$idx" "$(basename "$f")")
      ((idx++))
    done
    if [[ "$TUI_CMD" == "dialog" ]]; then
      idx=$(dialog --clear --title "Logs firmware" --menu "Sélectionnez un log :" 20 70 15 "${menu_args[@]}" 3>&1 1>&2 2>&3)
    else
      idx=$(whiptail --title "Logs firmware" --menu "Sélectionnez un log :" 20 70 15 "${menu_args[@]}" 3>&1 1>&2 2>&3)
    fi
    [[ -z "$idx" ]] && return
    choix="${files[idx-1]}"
  else
    echo -e "\033[1;36mFichiers de log disponibles :\033[0m" >&2
    local i=1
    for f in "${files[@]}"; do
      echo "  $i) $(basename "$f")" >&2
      ((i++))
    done
    echo -en "Numéro du log à afficher (ou Entrée pour retour) : " >&2
    read -r idx
    [[ -z "$idx" ]] && return
    if [[ "$idx" =~ ^[0-9]+$ ]] && (( idx >= 1 && idx <= ${#files[@]} )); then
      choix="${files[idx-1]}"
    else
      echo "Entrée invalide." >&2
      sleep 1
      return
    fi
  fi
  echo -e "\033[1;33m--- Dernières lignes de : $choix ---\033[0m"
  tail -n 15 "$choix"
  echo -e "\033[1;36m--- Appuyez sur Entrée pour revenir au menu ---\033[0m"
  read -r _
}



# Mode CLI (compatibilité zacus.sh)
command=${1:-}
if [[ -n "$command" ]]; then
  case "$command" in
    bootstrap)
      run_bootstrap; exit $? ;;
    build)
      run_build_all; exit $? ;;
    drivers)
      # Audit des drivers pour la plateforme passée en 2e argument
      platform=${2:-}
      if [[ -z "$platform" ]]; then echo "Usage: cockpit.sh drivers <platform>"; exit 1; fi
      log "[AGENT] Audit drivers $platform"
      # Appel d’un helper centralisé ou d’un script spécifique si existant
      if [[ -f "$FW_ROOT/tools/dev/agent_utils.sh" ]]; then
        source "$FW_ROOT/tools/dev/agent_utils.sh"
        drivers_audit "$platform"
      else
        echo "[TODO] Implémenter drivers_audit pour $platform"; exit 1
      fi
      exit $? ;;
    test)
      # Audit des tests hardware pour la plateforme passée en 2e argument
      platform=${2:-}
      if [[ -z "$platform" ]]; then echo "Usage: cockpit.sh test <platform>"; exit 1; fi
      log "[AGENT] Audit tests $platform"
      if [[ -f "$FW_ROOT/tools/dev/agent_utils.sh" ]]; then
        source "$FW_ROOT/tools/dev/agent_utils.sh"
        tests_audit "$platform"
      else
        echo "[TODO] Implémenter tests_audit pour $platform"; exit 1
      fi
      exit $? ;;
    flash)
      flash_all; exit $? ;;
    rc)
      ZACUS_REQUIRE_HW=1 run_rc_live; exit $? ;;
    rc-autofix)
      "$FW_ROOT/tools/dev/zacus.sh" rc-autofix; exit $? ;;
    ports)
      ports_watch; exit $? ;;

    git)
      shift || true
      run_git "$@"; exit $? ;;
    latest)
      latest_artifacts; exit $? ;;
    audit)
      run_audit_all; exit $? ;;
    report)
      run_sync_report; exit $? ;;
    cleanup)
      run_cleanup; exit $? ;;
    codex-check)
      run_codex_check; exit $? ;;
    help|--help|-h)
      afficher_aide; exit 0 ;;
    wifi-debug)
      monitor_wifi_debug_serial; exit $? ;;
    *)
      echo "Usage: cockpit.sh <command>"; exit 1 ;;
  esac
fi

# Boucle principale : tout est dans show_menu
show_menu
