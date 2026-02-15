#!/usr/bin/env bash
# Chaînes de menu Zacus (français/anglais)

# Détection automatique de la langue
ZACUS_LANG=${ZACUS_LANG:-}
if [[ -z "$ZACUS_LANG" ]]; then
  case "${LANG:-}" in
    fr*|FR*) ZACUS_LANG=fr ;;
    en*|EN*) ZACUS_LANG=en ;;
    *) ZACUS_LANG=fr ;;
  esac
fi

# Fonctions d'accès aux chaînes
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
