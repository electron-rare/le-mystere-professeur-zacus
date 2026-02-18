#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../../.." && pwd)"

# Empêche la récursion infinie
if [[ "$SCRIPT_DIR" != "$REPO_ROOT/tools/test" ]]; then
	TARGET_SCRIPT="${REPO_ROOT}/tools/test/run_rc_gate.sh"
	if [[ -f "$TARGET_SCRIPT" ]]; then
		exec bash "$TARGET_SCRIPT" "$@"
	else
		echo "Erreur : $TARGET_SCRIPT introuvable."
		exit 1
	fi
fi

# ...section principale du script...
echo "Script lancé depuis \"$SCRIPT_DIR\""
echo "Répertoire racine : \"$REPO_ROOT\""
# Ajoutez ici la logique principale du gate RC
