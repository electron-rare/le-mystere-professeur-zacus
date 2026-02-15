#!/usr/bin/env bash
set -e

# Script d'onboarding/détection pour nouveaux contributeurs Zacus

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

# 1. Vérification de l'environnement
"$SCRIPT_DIR/check_env.sh"

# 2. Présentation rapide du repo
cat <<EOF

\033[1;36mBienvenue sur le firmware Zacus !\033[0m

- Structure :
  * esp32_audio/ : firmware principal audio
  * ui/esp8266_oled/ : UI OLED légère
  * ui/rp2040_tft/ : UI TFT tactile
  * tools/dev/ : scripts d'automatisation, build, tests, menus
  * docs/ : documentation, quickstart, protocoles

- Commandes clés :
  * ./tools/dev/bootstrap_local.sh : installation dépendances
  * ./build_all.sh : build global
  * ./tools/dev/zacus.sh : cockpit interactif
  * ./tools/dev/check_env.sh : diagnostic environnement

- Documentation :
  * docs/QUICKSTART.md
  * docs/RC_FINAL_BOARD.md
  * protocol/ui_link_v2.md

- Pour toute question, voir README.md ou demander sur le canal projet.

EOF

# 3. Conseils post-install
cat <<EOT
\033[1;33mConseil :\033[0m
- Pour une expérience optimale, installez fzf/dialog/whiptail.
- Pour tester le build : ./build_all.sh
- Pour tester le cockpit : ./tools/dev/zacus.sh
EOT

exit 0
