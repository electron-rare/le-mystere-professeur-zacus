#!/bin/bash
# collect_serial_bootlog.sh - Dump série boot ESP32 dans artifacts/rc_live/
# Usage: ./tools/dev/collect_serial_bootlog.sh <PORT> [duree_sec]

set -euo pipefail

PORT="${1:-/dev/cu.SLAB_USBtoUART}"
DUREE="${2:-10}"
LOGDIR="$(dirname "$0")/../../artifacts/rc_live"
mkdir -p "$LOGDIR"
LOGFILE="$LOGDIR/bootlog_$(date +%Y%m%d-%H%M%S).log"

if ! command -v cat &>/dev/null; then
  echo "[ERREUR] cat non disponible."
  exit 1
fi

echo "[INFO] Capture série $PORT pendant $DUREE sec..."
cat "$PORT" | head -c 100000 | tee "$LOGFILE" &
PID=$!
sleep "$DUREE"
kill $PID 2>/dev/null || true

echo "[INFO] Log archivé : $LOGFILE"
exit 0
