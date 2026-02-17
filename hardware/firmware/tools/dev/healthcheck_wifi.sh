#!/bin/bash
# healthcheck_wifi.sh - Diagnostic automatisé AP/HTTP pour ESP32 Zacus
# Placez ce script dans tools/dev/

set -euo pipefail

LOGDIR="$(dirname "$0")/../../artifacts/rc_live"
mkdir -p "$LOGDIR"
LOGFILE="$LOGDIR/healthcheck_$(date +%Y%m%d-%H%M%S).log"

SSID_PATTERN="ZACUS"
ESP_IP="192.168.4.1"

{
  echo "[HEALTHCHECK] $(date)"
  echo "---"
  echo "[1] Scan WiFi..."
  if command -v airport &>/dev/null; then
    SSIDS=$(airport -s | grep "$SSID_PATTERN" || true)
  elif command -v nmcli &>/dev/null; then
    SSIDS=$(nmcli dev wifi | grep "$SSID_PATTERN" || true)
  else
    SSIDS="[WARN] Aucun outil de scan WiFi compatible trouvé."
  fi
  echo "$SSIDS"
  echo "---"
  echo "[2] Ping $ESP_IP..."
  if ping -c 2 -W 2 "$ESP_IP" &>/dev/null; then
    echo "[OK] Ping $ESP_IP réussi."
  else
    echo "[FAIL] Ping $ESP_IP échoué."
  fi
  echo "---"
  echo "[3] Test HTTP /api/status..."
  if curl -s --max-time 3 "http://$ESP_IP/api/status" | grep -q '"wifi"'; then
    echo "[OK] /api/status répond."
  else
    echo "[FAIL] /api/status ne répond pas."
  fi
  echo "---"
  echo "[END] Log archivé dans $LOGFILE"
} | tee "$LOGFILE"

exit 0
