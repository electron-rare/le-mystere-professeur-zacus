#!/usr/bin/env bash
# Workaround for the electron-server docker bridge (192.168.0.0/20)
# colliding with the WiFi LAN where the ESP32 boards live.
#
# Run this from GrosMac (or any host on the room WiFi) to:
#   1. Ask the gateway to compile the scenario → IR
#   2. Fetch the IR
#   3. POST it directly to the board
#
# Once the gateway moves to GrosMac (or the Docker bridge is renumbered),
# the app's /v1/flash/{board} endpoint will work end-to-end and this
# script becomes obsolete.
#
# Usage:
#   ./flash_bridge.sh <scenario.yaml> <board-host>
#   e.g. ./flash_bridge.sh zacus_cond_demo.yaml zacus-master.local
set -euo pipefail

SCENARIO="${1:-zacus_cond_demo.yaml}"
BOARD="${2:-zacus-master.local}"
GATEWAY="${GATEWAY_URL:-http://electron-server:8400}"
TOKEN="${ZACUS_HUB_TOKEN:?set ZACUS_HUB_TOKEN env var to the gateway bearer}"

TMPDIR=$(mktemp -d)
trap 'rm -rf "$TMPDIR"' EXIT

echo "→ compile $SCENARIO via gateway"
curl -fsS -H "Authorization: Bearer $TOKEN" -X POST \
  "$GATEWAY/v1/studio/scenario/$SCENARIO/compile" > "$TMPDIR/compile.json"
cat "$TMPDIR/compile.json" | python3 -m json.tool

# The /compile endpoint writes the IR on the gateway host. Fetch it via the
# gateway's own /v1/studio/scenario route (returns YAML, not JSON) won't work.
# Instead, scp the .ir.json file back.
IR_NAME="${SCENARIO%.yaml}.ir.json"
echo "→ scp ir.json"
scp "electron-server:/home/electron/zacus-hub/game/scenarios/$IR_NAME" "$TMPDIR/ir.json"

echo "→ POST to $BOARD"
curl -fsS -X POST -H "Content-Type: application/json" \
  --data @"$TMPDIR/ir.json" "http://$BOARD/game/scenario"
echo
echo "✓ done"
