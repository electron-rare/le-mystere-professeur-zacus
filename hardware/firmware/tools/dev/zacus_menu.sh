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

# Détection TUI (fzf > dialog > whiptail)
if command -v fzf >/dev/null 2>&1; then
  TUI_CMD="fzf"
elif command -v dialog >/dev/null 2>&1; then
  TUI_CMD="dialog"
elif command -v whiptail >/dev/null 2>&1; then
  TUI_CMD="whiptail"
else
  TUI_CMD=""
fi

# Message d'invitation à installer fzf si aucun TUI
if [[ -z "$TUI_CMD" ]]; then
  echo -e "\033[1;33m[Astuce] Pour une navigation moderne (flèches + Entrée/Échap), installez fzf :\033[0m" >&2
  echo -e "\033[1;36mbrew install fzf\033[0m   (ou sudo apt install fzf sous Linux)" >&2
fi



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


# Affichage du menu (fzf > dialog > texte coloré)
show_menu() {
  local options=(
    "1:RC live gate (ZACUS_REQUIRE_HW unset)"
    "2:RC live gate (ZACUS_REQUIRE_HW=1)"
    "3:Build all firmware (hardware/firmware/build_all.sh)"
    "4:Bootstrap (outils/dev/bootstrap_local.sh)"
    "5:Watch serial ports"
    "6:Run codex prompt menu"
    "7:Afficher logs (firmware/logs/)"
    "0:Exit"
  )
  if [[ "$TUI_CMD" == "fzf" ]]; then
    local choice
    choice=$(printf '%s\n' "${options[@]}" | fzf --ansi --prompt="❯ Zacus Firmware Cockpit : " --header="[Flèches] naviguer, [Entrée] valider, [Échap] quitter" --height=15 --border --cycle)
    [[ -z "$choice" ]] && echo 0 && return
    echo "${choice%%:*}"
  elif [[ "$TUI_CMD" == "dialog" || "$TUI_CMD" == "whiptail" ]]; then
    local menu_args=()
    for opt in "${options[@]}"; do
      menu_args+=("${opt%%:*}" "${opt#*:}")
    done
    local choice
    if [[ "$TUI_CMD" == "dialog" ]]; then
      choice=$(dialog --clear --colors --title "\Zb\Z4Zacus Firmware Cockpit\Zn" \
        --menu "\Z1Sélectionnez une action :\Zn" 20 70 10 \
        "${menu_args[@]}" \
        3>&1 1>&2 2>&3)
    else
      choice=$(whiptail --title "Zacus Firmware Cockpit" \
        --menu "Sélectionnez une action :" 20 70 10 \
        "${menu_args[@]}" \
        3>&1 1>&2 2>&3)
    fi
    [[ -z "$choice" ]] && echo 0 && return
    echo "$choice"
  else
    # Fallback texte coloré, toujours affiché tant qu'un choix valide n'est pas fait
    local Y="\033[1;33m" G="\033[1;32m" C="\033[1;36m" R="\033[1;31m" N="\033[0m"
    echo -e "${R}[Aucun menu TUI détecté : utilisation du mode texte interactif]${N}" >&2; >&2
    echo "[DEBUG] TEST affichage menu texte" >&2; >&2
    trap 'echo; exit 0' INT
    while true; do
      echo -e "${C}╔══════════════════════════════════════════════════════╗${N}" >&2; >&2
      echo -e "${C}║         ${G}Zacus Firmware Cockpit (mode texte)${C}         ║${N}" >&2; >&2
      echo -e "${C}╚══════════════════════════════════════════════════════╝${N}" >&2; >&2
      echo -e "${Y}Navigation : Entrez le numéro puis [Entrée]. Annulez avec Ctrl+C.${N}" >&2; >&2
      echo -e "${Y}Pour une navigation à la souris ou aux flèches, installez fzf !${N}" >&2; >&2
      for opt in "${options[@]}"; do
        printf "  ${Y}%s${N}  %s\n" "${opt%%:*}" "${opt#*:}" >&2; >&2
      done
      echo -en "${G}Votre choix [0-7] : ${N}" >&2; >&2
      read -r choice || { echo; exit 0; }
      [[ -z "$choice" || "$choice" == "0" ]] && echo 0 && return
      if [[ "$choice" =~ ^[1-7]$ ]]; then
        echo "$choice"
        return
      fi
      echo -e "${R}Entrée invalide. Veuillez choisir un numéro entre 0 et 7.${N}" >&2; >&2
    done
  # Sous-menu pour afficher les logs
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
  fi
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

# Boucle principale avec menu TUI amélioré

# Forçage du menu texte si aucun TUI détecté
if [[ -z "$TUI_CMD" ]]; then
  echo "[INFO] Aucun menu TUI détecté, utilisation du menu texte interactif." >&2
fi

while true; do
  echo "[DEBUG] Affichage du menu..." >&2
  choice=$(show_menu)
  echo "[DEBUG] Choix utilisateur: '$choice'" >&2
  case "$choice" in
    1) ZACUS_REQUIRE_HW=0 run_rc_live ;;
    2) ZACUS_REQUIRE_HW=1 run_rc_live ;;
    3) run_build_all ;;
    4) run_bootstrap ;;
    5) ports_watch ;;
    6) run_codex_prompts ;;
    7) afficher_logs_menu ;;
    0|""|"Cancel")
      echo "[DEBUG] Sortie du cockpit Zacus." >&2
      [[ "$TUI_CMD" == "dialog" ]] && dialog --msgbox "Sortie du cockpit Zacus." 6 40
      exit 0 ;;
    *)
      echo "[DEBUG] Entrée non reconnue, relance du menu." >&2
      ;;
  esac
done
