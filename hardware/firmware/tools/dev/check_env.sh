#!/usr/bin/env bash
set -e

# Vérification des dépendances critiques pour le repo Zacus

check() {
  local cmd="$1"
  local name="$2"
  if ! command -v "$cmd" >/dev/null 2>&1; then
    echo -e "\033[1;31m[KO]\033[0m $name ($cmd) non trouvé."
    return 1
  else
    echo -e "\033[1;32m[OK]\033[0m $name : $($cmd --version 2>&1 | head -n1)"
  fi
}

check "python3" "Python 3"
check "pip" "pip"
check "platformio" "PlatformIO"
check "make" "make"
check "fzf" "fzf (optionnel)"
check "dialog" "dialog (optionnel)"
check "whiptail" "whiptail (optionnel)"

# Vérification des modules Python critiques
python3 -c "import serial" 2>/dev/null && echo -e "\033[1;32m[OK]\033[0m pyserial installé" || echo -e "\033[1;31m[KO]\033[0m pyserial manquant (pip install pyserial)"

exit 0
