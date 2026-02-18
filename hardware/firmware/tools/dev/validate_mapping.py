#!/usr/bin/env python3
# Script de validation croisée du mapping hardware USB Zacus
# Vérifie la cohérence entre platformio.ini, ui_freenove_config.h, RC_FINAL_BOARD.md, resolve_ports.py

import sys
import os
from pathlib import Path

def get_path(arg):
    p = sys.argv[sys.argv.index(arg)+1]
    return Path(p) if os.path.isabs(p) else Path(os.getcwd()) / p

platformio = get_path('--platformio')
rc = get_path('--rc')
config = get_path('--config')
output = get_path('--output')

    # Bloc minimal : log chaque ligne pour valider l’accès fichier
    pass

errors = []
errors = []

# Extraction des environnements PlatformIO
envs = set()
for line in platformio.read_text(encoding="utf-8").splitlines():
    m = re.match(r"\[env:([\w_]+)\]", line)
    if m:
        envs.add(m.group(1))

# Extraction des macros hardware dans config.h
macros = set()
for line in config_h.read_text(encoding="utf-8").splitlines():
    m = re.match(r"#define\s+(UI_FREENOVE_ESP32S3|UI_ESP32|UI_ESP8266)", line)
    if m:
        macros.add(m.group(1))

# Extraction des mappings dans RC_FINAL_BOARD.md
mapping = set()
for line in rc_board.read_text(encoding="utf-8").splitlines():
    if "Freenove ESP32-S3" in line:
        mapping.add("UI_FREENOVE_ESP32S3")
    if "ESP32" in line and "Freenove" not in line:
        mapping.add("UI_ESP32")
    if "ESP8266" in line:
        mapping.add("UI_ESP8266")

# Extraction des ports dans resolve_ports.py
ports = set()
for line in resolve_ports.read_text(encoding="utf-8").splitlines():
    if "location-map" in line:
        if "esp32" in line:
            ports.add("UI_ESP32")
        if "esp8266_usb" in line:
            ports.add("UI_ESP8266")
        if "freenove" in line or "esp32s3" in line:
            ports.add("UI_FREENOVE_ESP32S3")

# Validation croisée
if not envs:
    errors.append("Aucun environnement PlatformIO détecté.")
if not macros:
    errors.append("Aucune macro hardware détectée dans config.h.")
if not mapping:
    errors.append("Aucun mapping hardware détecté dans RC_FINAL_BOARD.md.")
if not ports:
    errors.append("Aucun port hardware détecté dans resolve_ports.py.")

for hw in ("UI_FREENOVE_ESP32S3", "UI_ESP32", "UI_ESP8266"):
    if hw not in envs:
        errors.append(f"Environnement PlatformIO manquant pour {hw}.")
    if hw not in macros:
        errors.append(f"Macro hardware manquante dans config.h pour {hw}.")
    if hw not in mapping:
        errors.append(f"Mapping hardware manquant dans RC_FINAL_BOARD.md pour {hw}.")
    if hw not in ports:
        errors.append(f"Port hardware manquant dans resolve_ports.py pour {hw}.")

if errors:
    print("[FAIL] Mapping hardware incohérent :")
    for err in errors:
        print(" -", err)
    sys.exit(1)
else:
    print("[PASS] Mapping hardware cohérent sur toutes les sources.")
    sys.exit(0)
