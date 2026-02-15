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

run_rc_live() {
  local artifacts=""
  if ! "$FW_ROOT/tools/dev/run_matrix_and_smoke.sh"; then
    artifacts="$(latest_artifacts)"
    if [[ -n "$artifacts" && -f "$RC_PROMPT" ]]; then
      ARTIFACT_PATH="$artifacts" codex exec - < "$RC_PROMPT"
    elif [[ -f "$RC_PROMPT" ]]; then
      codex exec - < "$RC_PROMPT"
    fi
    return 1
  fi
}

ports_watch() {
  echo "Press Ctrl+C to exit ports watch."
  while true; do
    echo "=== $(date -R) ==="
    python3 -m serial.tools.list_ports -v || true
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
    "Run codex prompt menu"
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
      6) run_codex_prompts ;;
      7) afficher_logs_menu ;;
      8) afficher_aide ;;
      9|0) exit 0 ;;
    esac
  done
}

afficher_aide() {
  echo -e "\n\033[1;36mAide Zacus Cockpit\033[0m"
  echo "- Utilisez les flèches ou le numéro pour naviguer."
  echo "- Installez fzf/dialog/whiptail pour une meilleure expérience."
  echo "- [Entrée] pour valider, [Échap] ou [Entrée] vide pour annuler."
  echo "- Logs : 15 dernières lignes de chaque fichier dans logs/"
  echo "- Pour toute question, voir README.md."
  read -n 1 -s -r -p "Appuyez sur une touche pour revenir au menu..."
}

run_codex_prompts() {
  "$FW_ROOT/tools/dev/codex_prompt_menu.sh"
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
    flash)
      # Appel de la logique de flash de zacus.sh (à intégrer ici)
      echo "[TODO] Implémenter la logique de flash ici (voir zacus.sh)"; exit 1 ;;
    rc)
      ZACUS_REQUIRE_HW=1 run_rc_live; exit $? ;;
    rc-autofix)
      echo "[TODO] Implémenter la logique rc-autofix ici (voir zacus.sh)"; exit 1 ;;
    ports)
      ports_watch; exit $? ;;
    latest)
      latest_artifacts; exit $? ;;
    help|--help|-h)
      afficher_aide; exit 0 ;;
    *)
      echo "Usage: cockpit.sh <command>"; exit 1 ;;
  esac
fi

# Boucle principale : tout est dans show_menu
show_menu
