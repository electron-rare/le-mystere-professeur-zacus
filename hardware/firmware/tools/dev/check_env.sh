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

# Vérification des modules Python critiques
if python3 -c "import serial" 2>/dev/null; then
  ok "pyserial installé"
else
  ko "pyserial manquant (python3 -m pip install pyserial)"
  missing=1
fi

exit "$missing"
