#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT_DIR"

echo "[mp3-client-demo] baseline smoke"
bash tools/qa/mp3_rc_smoke.sh

echo "[mp3-client-demo] verify MP3 command surface"
rg -q "MP3_STATUS" src/services/serial/serial_commands_mp3.cpp
rg -q "MP3_UI_STATUS" src/services/serial/serial_commands_mp3.cpp
rg -q "MP3_SCAN_PROGRESS" src/services/serial/serial_commands_mp3.cpp
rg -q "MP3_BACKEND_STATUS" src/services/serial/serial_commands_mp3.cpp
rg -q "MP3_CAPS" src/services/serial/serial_commands_mp3.cpp
rg -q "MP3_QUEUE_PREVIEW" src/services/serial/serial_commands_mp3.cpp

echo "[mp3-client-demo] verify backend observability fields"
rg -q "last_fallback_reason" src/controllers/mp3/mp3_controller.cpp
rg -q "tools_attempt=" src/controllers/mp3/mp3_controller.cpp
rg -q "legacy_attempt=" src/controllers/mp3/mp3_controller.cpp

echo "[mp3-client-demo] verify OLED MP3 structured fields"
rg -q "uiCursor" screen_esp8266_hw630/src/core/telemetry_state.h
rg -q "uiOffset" screen_esp8266_hw630/src/core/telemetry_state.h
rg -q "uiCount" screen_esp8266_hw630/src/core/telemetry_state.h
rg -q "queueCount" screen_esp8266_hw630/src/core/telemetry_state.h

echo "[mp3-client-demo] verify screen link robustness fields"
rg -q "frameSeq" screen_esp8266_hw630/src/core/telemetry_state.h
rg -q "crc" screen_esp8266_hw630/src/core/stat_parser.cpp

echo "[mp3-client-demo] PASS"
