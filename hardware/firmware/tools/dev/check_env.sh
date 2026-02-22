#!/usr/bin/env bash
set -euo pipefail

# Vérification des dépendances critiques pour le repo Zacus

ok() { echo -e "\033[1;32m[OK]\033[0m $*"; }
ko() { echo -e "\033[1;31m[KO]\033[0m $*"; }
warn() { echo -e "\033[1;33m[WARN]\033[0m $*"; }

has_cmd() {
  command -v "$1" >/dev/null 2>&1
}

check_required() {
  local cmd="$1"
  local name="$2"
  if has_cmd "$cmd"; then
    ok "$name : $("$cmd" --version 2>&1 | head -n1)"
  else
    ko "$name ($cmd) non trouvé."
    return 1
  fi
}

check_optional() {
  local cmd="$1"
  local name="$2"
  if has_cmd "$cmd"; then
    ok "$name : $("$cmd" --version 2>&1 | head -n1)"
  else
    warn "$name ($cmd) non trouvé (optionnel)."
  fi
}

missing=0
check_required "python3" "Python 3" || missing=1

if has_cmd pip; then
  ok "pip : $(pip --version 2>&1 | head -n1)"
elif has_cmd pip3; then
  ok "pip3 (fallback pip) : $(pip3 --version 2>&1 | head -n1)"
else
  ko "pip/pip3 non trouvé."
  missing=1
fi

check_required "platformio" "PlatformIO" || missing=1
check_required "make" "make" || missing=1
check_optional "fzf" "fzf"
check_optional "dialog" "dialog"
check_optional "whiptail" "whiptail"
check_optional "gh" "GitHub CLI"
check_optional "codex" "Codex CLI"
check_optional "curl" "curl"

# Vérification des modules Python critiques
if python3 -c "import serial" 2>/dev/null; then
  ok "pyserial installé"
else
  ko "pyserial manquant (python3 -m pip install pyserial)"
  missing=1
fi

# Vérifications d'intégration (optionnelles)
if has_cmd gh; then
  if gh auth status -h github.com >/dev/null 2>&1; then
    ok "GitHub API auth (gh) opérationnelle"
  else
    warn "GitHub API auth (gh) indisponible (exécuter: gh auth login)"
  fi
fi

if has_cmd codex; then
  codex_status="$(codex login status 2>&1 || true)"
  if [[ "$codex_status" == *"Logged in"* ]]; then
    ok "Codex login: $codex_status"
  else
    warn "Codex non connecté (exécuter: codex login)"
  fi

  mcp_list="$(codex mcp list 2>/dev/null || true)"
  if echo "$mcp_list" | grep -q "^MCP_DOCKER"; then
    if has_cmd docker; then
      if docker info >/dev/null 2>&1; then
        ok "MCP_DOCKER prêt (daemon Docker actif)"
      else
        warn "MCP_DOCKER configuré mais daemon Docker inactif (démarrer Docker Desktop)"
      fi
    else
      warn "MCP_DOCKER configuré mais commande docker absente"
    fi
  fi
fi

if has_cmd curl; then
  if [[ -n "${OPENAI_API_KEY:-}" ]]; then
    openai_code="$(curl -sS -o /dev/null -w "%{http_code}" -H "Authorization: Bearer ${OPENAI_API_KEY}" https://api.openai.com/v1/models || true)"
    if [[ "$openai_code" == "200" ]]; then
      ok "OpenAI API accessible avec OPENAI_API_KEY"
    else
      warn "OpenAI API inaccessible avec OPENAI_API_KEY (HTTP $openai_code)"
    fi
  else
    openai_code="$(curl -sS -o /dev/null -w "%{http_code}" https://api.openai.com/v1/models || true)"
    if [[ "$openai_code" == "401" ]]; then
      ok "OpenAI API joignable (HTTP 401 attendu sans clé)"
    else
      warn "OpenAI API non joignable ou réponse inattendue (HTTP $openai_code)"
    fi
  fi
fi

exit "$missing"
