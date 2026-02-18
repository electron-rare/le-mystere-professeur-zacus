#!/usr/bin/env bash
# Validation script for baudrate migration 19200 -> 57600
# Usage: ./tools/dev/validate_baudrate_57600.sh

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
FIRMWARE_ROOT="${SCRIPT_DIR}/../.."
cd "$FIRMWARE_ROOT"
source tools/dev/layout_paths.sh

ESP32_SRC_ROOT="$(fw_esp32_src_root)"
UI_OLED_SRC_ROOT="$(fw_ui_oled_src)"
UI_TFT_SRC_ROOT="$(fw_ui_tft_src)"
UI_TFT_INCLUDE_FILE="$(cd "$UI_TFT_SRC_ROOT/.." && pwd)/include/ui_config.h"
UI_OLED_MAIN_FILE="$UI_OLED_SRC_ROOT/main.cpp"
ESP32_CONFIG_FILE="$ESP32_SRC_ROOT/config.h"

echo "=========================================="
echo "Baudrate 57600 Validation Script"
echo "=========================================="
echo ""

# Colors
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

PASS_COUNT=0
FAIL_COUNT=0

function check_pass() {
  echo -e "${GREEN}✅ PASS${NC}: $1"
  PASS_COUNT=$((PASS_COUNT + 1))
}

function check_fail() {
  echo -e "${RED}❌ FAIL${NC}: $1"
  FAIL_COUNT=$((FAIL_COUNT + 1))
}

function check_warn() {
  echo -e "${YELLOW}⚠️  WARN${NC}: $1"
}

echo "1. Checking ESP32 config..."
if grep -q "constexpr uint32_t kUiUartBaud = 57600" "$ESP32_CONFIG_FILE"; then
  check_pass "ESP32 kUiUartBaud = 57600"
else
  check_fail "ESP32 kUiUartBaud != 57600"
fi

echo ""
echo "2. Checking ESP8266 OLED config..."
if grep -q "constexpr uint32_t kLinkBaud = 57600" "$UI_OLED_MAIN_FILE"; then
  check_pass "ESP8266 OLED kLinkBaud = 57600"
else
  check_fail "ESP8266 OLED kLinkBaud != 57600"
fi

echo ""
echo "3. Checking RP2040 TFT config..."
if grep -q "constexpr uint32_t kSerialBaud = 57600U" "$UI_TFT_INCLUDE_FILE"; then
  check_pass "RP2040 TFT kSerialBaud default = 57600"
else
  check_fail "RP2040 TFT kSerialBaud default != 57600"
fi

if grep -q "\-DUI_SERIAL_BAUD=57600" platformio.ini; then
  check_pass "PlatformIO.ini UI_SERIAL_BAUD = 57600"
else
  check_fail "PlatformIO.ini UI_SERIAL_BAUD != 57600"
fi

echo ""
echo "4. Checking ESP32 pins (GPIO22/19)..."
if grep -q "constexpr uint8_t kUiUartTxPin = 22" "$ESP32_CONFIG_FILE"; then
  check_pass "ESP32 TX pin = GPIO22"
else
  check_fail "ESP32 TX pin != GPIO22"
fi

if grep -q "constexpr uint8_t kUiUartRxPin = 19" "$ESP32_CONFIG_FILE"; then
  check_pass "ESP32 RX pin = GPIO19"
else
  check_fail "ESP32 RX pin != GPIO19"
fi

echo ""
echo "5. Checking protocol docs..."
if grep -q "Default baud: 57600" protocol/ui_link_v2.md; then
  check_pass "protocol/ui_link_v2.md mentions 57600"
else
  check_fail "protocol/ui_link_v2.md does not mention 57600"
fi

echo ""
echo "6. Checking AGENTS.md..."
if grep -q "internal ESP8266 SoftwareSerial UI link is \`57600\`" AGENTS.md; then
  check_pass "AGENTS.md baudrate updated to 57600"
else
  check_fail "AGENTS.md still references old baudrate"
fi

echo ""
echo "7. Checking for leftover 19200 references (should be minimal)..."
OLD_BAUD_COUNT=$(grep -r "19200" --include="*.cpp" --include="*.h" --include="*.ini" . 2>/dev/null | grep -v ".pio/" | grep -v "artifacts/" | grep -v "logs/" | wc -l | xargs)
if [[ "$OLD_BAUD_COUNT" -eq 0 ]]; then
  check_pass "No leftover 19200 in code"
else
  check_warn "Found $OLD_BAUD_COUNT references to 19200 (check manually)"
fi

echo ""
echo "8. Checking GPIO18/GPIO23 old pins removed..."
OLD_PIN_TX=$(grep -r "GPIO18" --include="*.cpp" --include="*.h" "$ESP32_SRC_ROOT" "$UI_OLED_SRC_ROOT" "$UI_TFT_SRC_ROOT" 2>/dev/null | wc -l | xargs)
OLD_PIN_RX=$(grep -r "GPIO23" --include="*.cpp" --include="*.h" "$ESP32_SRC_ROOT" "$UI_OLED_SRC_ROOT" "$UI_TFT_SRC_ROOT" 2>/dev/null | wc -l | xargs)
if [[ "$OLD_PIN_TX" -eq 0 && "$OLD_PIN_RX" -eq 0 ]]; then
  check_pass "No leftover GPIO18/23 in firmware"
else
  check_warn "Found GPIO18/23 references (TX: $OLD_PIN_TX, RX: $OLD_PIN_RX)"
fi

echo ""
echo "=========================================="
echo "Summary: ${GREEN}${PASS_COUNT} passed${NC}, ${RED}${FAIL_COUNT} failed${NC}"
echo "=========================================="

if [[ "$FAIL_COUNT" -gt 0 ]]; then
  echo ""
  echo -e "${RED}⚠️  VALIDATION FAILED${NC}"
  echo "Please check the errors above and fix them."
  exit 1
fi

echo ""
echo -e "${GREEN}✅ ALL CHECKS PASSED${NC}"
echo ""
echo "Next steps:"
echo "  1. Flash all 3 firmwares: ./build_all.sh && ./tools/dev/cockpit.sh flash"
echo "  2. Run smoke tests: ./tools/dev/run_matrix_and_smoke.sh"
echo "  3. Verify UI Link handshake: monitor logs for [UI_LINK_STATUS connected=1]"
echo ""
exit 0
