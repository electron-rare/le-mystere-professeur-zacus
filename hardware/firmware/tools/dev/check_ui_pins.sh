#!/bin/bash
# Diagnostic simplifié: vérifier les pins UI Link
# Usage: ./check_ui_pins.sh

set -euo pipefail

cd "$(dirname "$0")/../.."
source tools/dev/layout_paths.sh

ESP32_SRC_ROOT="$(fw_esp32_src_root)"
CONFIG_FILE="$ESP32_SRC_ROOT/config.h"
RUNTIME_FILE="$ESP32_SRC_ROOT/runtime/runtime_state.cpp"

if [ ! -f "$CONFIG_FILE" ]; then
    echo "❌ Config file not found: $CONFIG_FILE"
    exit 1
fi

# Lire les pins actuels desde config.h
TX_PIN=$(grep -oP 'kUiUartTxPin\s*=\s*\K\d+' "$CONFIG_FILE" | head -1)
RX_PIN=$(grep -oP 'kUiUartRxPin\s*=\s*\K\d+' "$CONFIG_FILE" | head -1)

echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "UI LINK PIN CONFIGURATION"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""
echo "📋 Current firmware config:"
echo "   TX (ESP32 → OLED)  : GPIO ${TX_PIN}"
echo "   RX (OLED → ESP32)  : GPIO ${RX_PIN}"
echo "   Baud speed         : 19200"
echo ""

# NodeMCU pins (fixed)
echo "📍 NodeMCU/OLED pins (fixed in firmware):"
echo "   RX (D6 = GPIO12)   : Expects ESP32 TX"
echo "   TX (D5 = GPIO14)   : Sends to ESP32 RX"
echo ""

# Check cable status
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "⚠️  CABLE VERIFICATION:"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""
echo "Check physical connections on ESP32:"
echo "  🔴 TX cable (GPIO ${TX_PIN}) → OLED pin D6 (GPIO12)"
echo "  🔵 RX cable (GPIO ${RX_PIN}) ← OLED pin D5 (GPIO14)"
echo ""
echo "If NOT connected:"
echo "  ❌ Screen will NOT update"
echo "  ❌ UI link will say 'not connected'"
echo ""
echo "Action:"
echo "  1️⃣  Check physical cables on those pins"
echo "  2️⃣  If moved from old pins 22/19 to 18/23: verify firmly connected"
echo "  3️⃣  If NOT moved yet: MOVE them from GPIO 22/19 to ${TX_PIN}/${RX_PIN}"
echo "  4️⃣  Run quick test after verification"
echo ""

# Test if we can read port status
if command -v ls &> /dev/null; then
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    echo "📡 Serial ports detected:"
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    echo ""
    ls -la /dev/cu.* 2>/dev/null | grep -E "(SLAB|usbmodem|ttyUSB)" || echo "   No USB devices found yet"
    echo ""
fi

echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "✅ Next step: physically verify cables, then run:"
echo "   python3 tools/dev/test_4scenarios_all.py --mode quick"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
