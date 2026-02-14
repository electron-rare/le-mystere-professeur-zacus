#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT_DIR"

ESP32_PORT="${ESP32_PORT:-}"
SCREEN_PORT="${SCREEN_PORT:-}"
SKIP_FLASH=0
SKIP_SERIAL=0

usage() {
  cat <<'EOF'
Usage: live_story_v2_smoke.sh [options]

Options:
  --esp32-port <port>    ESP32 serial port (e.g. /dev/cu.SLAB_USBtoUART)
  --screen-port <port>   ESP8266 serial port
  --skip-flash           Skip upload steps
  --skip-serial          Skip serial command smoke
  -h, --help             Show this help

Environment alternatives:
  ESP32_PORT=<port>
  SCREEN_PORT=<port>
EOF
}

while [ "$#" -gt 0 ]; do
  case "$1" in
    --esp32-port)
      ESP32_PORT="${2:-}"
      shift 2
      ;;
    --screen-port)
      SCREEN_PORT="${2:-}"
      shift 2
      ;;
    --skip-flash)
      SKIP_FLASH=1
      shift
      ;;
    --skip-serial)
      SKIP_SERIAL=1
      shift
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "Unknown option: $1" >&2
      usage
      exit 1
      ;;
  esac
done

mkdir -p reports
RUN_ID="$(date '+%Y%m%d_%H%M%S')"
LOG_FILE="reports/live_story_v2_smoke_${RUN_ID}.log"

log() {
  local line="[$(date '+%H:%M:%S')] $*"
  echo "$line"
  echo "$line" >>"$LOG_FILE"
}

run_logged() {
  log "RUN: $*"
  "$@" 2>&1 | tee -a "$LOG_FILE"
}

log "Story V2 smoke start (run_id=${RUN_ID})"
run_logged pio device list
run_logged make story-validate
run_logged make story-gen

if [ "$SKIP_FLASH" -eq 0 ]; then
  if [ -z "$ESP32_PORT" ] || [ -z "$SCREEN_PORT" ]; then
    log "ERROR: flash requires --esp32-port and --screen-port"
    exit 1
  fi
  run_logged make upload-esp32 "ESP32_PORT=${ESP32_PORT}"
  run_logged make uploadfs-esp32 "ESP32_PORT=${ESP32_PORT}"
  run_logged make upload-screen "SCREEN_PORT=${SCREEN_PORT}"
else
  log "Flash step skipped"
fi

if [ "$SKIP_SERIAL" -eq 0 ]; then
  if [ -z "$ESP32_PORT" ]; then
    log "ERROR: serial smoke requires --esp32-port"
    exit 1
  fi
  if python3 -c "import serial" >/dev/null 2>&1; then
    log "Running serial smoke command set on ${ESP32_PORT}"
    ESP32_PORT="$ESP32_PORT" python3 - <<'PY' 2>&1 | tee -a "$LOG_FILE"
import os
import time
import serial

port = os.environ["ESP32_PORT"]
baud = 115200
commands = [
    "BOOT_STATUS",
    "STORY_V2_ENABLE ON",
    "STORY_V2_TRACE_LEVEL INFO",
    "STORY_V2_STATUS",
    "STORY_V2_HEALTH",
    "STORY_TEST_ON",
    "STORY_TEST_DELAY 2500",
    "STORY_ARM",
    "STORY_V2_EVENT ETAPE2_DUE",
    "STORY_V2_METRICS",
    "SYS_LOOP_BUDGET STATUS",
    "SCREEN_LINK_STATUS",
    "MP3_SCAN_PROGRESS",
    "MP3_BACKEND_STATUS",
    "STORY_TEST_OFF",
    "STORY_V2_TRACE_LEVEL OFF",
]

with serial.Serial(port, baud, timeout=0.2) as ser:
    time.sleep(1.0)
    for cmd in commands:
        print(f">>> {cmd}")
        ser.write((cmd + "\n").encode("utf-8"))
        ser.flush()
        deadline = time.time() + 1.8
        seen = False
        while time.time() < deadline:
            raw = ser.readline()
            if not raw:
                continue
            seen = True
            print(raw.decode("utf-8", errors="replace").rstrip())
        if not seen:
            print("(no immediate response)")
PY
  else
    log "pyserial unavailable in current python; serial smoke skipped"
  fi
else
  log "Serial smoke skipped"
fi

log "Smoke complete. Log file: ${LOG_FILE}"
log "Run full end-of-sprint test with tools/qa/live_story_v2_runbook.md"
