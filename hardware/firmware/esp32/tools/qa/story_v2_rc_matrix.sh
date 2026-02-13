#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT_DIR"

ESP32_PORT="${ESP32_PORT:-}"
SOAK_MINUTES="${SOAK_MINUTES:-20}"
POLL_SECONDS="${POLL_SECONDS:-15}"
TRACE_LEVEL="${TRACE_LEVEL:-INFO}"
SKIP_PREFLIGHT=0

usage() {
  cat <<'EOF'
Usage: story_v2_rc_matrix.sh [options]

Options:
  --esp32-port <port>      ESP32 serial port (required)
  --soak-minutes <n>       Soak duration in minutes (default: 20)
  --poll-seconds <n>       Poll period in seconds (default: 15)
  --trace-level <level>    Story trace level OFF|ERR|INFO|DEBUG (default: INFO)
  --skip-preflight         Skip story-validate/story-gen/qa-story-v2
  -h, --help               Show this help

Environment alternatives:
  ESP32_PORT=<port>
  SOAK_MINUTES=20
  POLL_SECONDS=15
  TRACE_LEVEL=INFO
EOF
}

while [ "$#" -gt 0 ]; do
  case "$1" in
    --esp32-port)
      ESP32_PORT="${2:-}"
      shift 2
      ;;
    --soak-minutes)
      SOAK_MINUTES="${2:-}"
      shift 2
      ;;
    --poll-seconds)
      POLL_SECONDS="${2:-}"
      shift 2
      ;;
    --trace-level)
      TRACE_LEVEL="${2:-}"
      shift 2
      ;;
    --skip-preflight)
      SKIP_PREFLIGHT=1
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

if [ -z "$ESP32_PORT" ]; then
  echo "ERROR: --esp32-port is required" >&2
  exit 1
fi

if ! python3 -c "import serial" >/dev/null 2>&1; then
  echo "ERROR: python3 module 'serial' missing (install pyserial)" >&2
  exit 1
fi

mkdir -p reports
RUN_ID="$(date '+%Y%m%d_%H%M%S')"
LOG_FILE="reports/story_v2_rc_matrix_${RUN_ID}.log"

log() {
  local line="[$(date '+%H:%M:%S')] $*"
  echo "$line"
  echo "$line" >>"$LOG_FILE"
}

run_logged() {
  log "RUN: $*"
  "$@" 2>&1 | tee -a "$LOG_FILE"
}

if [ "$SKIP_PREFLIGHT" -eq 0 ]; then
  run_logged make story-validate
  run_logged make story-gen
  run_logged make qa-story-v2
else
  log "Preflight skipped"
fi

log "Starting RC serial matrix on ${ESP32_PORT} (soak=${SOAK_MINUTES}m poll=${POLL_SECONDS}s)"
ESP32_PORT="$ESP32_PORT" SOAK_MINUTES="$SOAK_MINUTES" POLL_SECONDS="$POLL_SECONDS" TRACE_LEVEL="$TRACE_LEVEL" \
python3 - <<'PY' 2>&1 | tee -a "$LOG_FILE"
import os
import time
import serial

port = os.environ["ESP32_PORT"]
soak_minutes = max(1, int(os.environ["SOAK_MINUTES"]))
poll_seconds = max(3, int(os.environ["POLL_SECONDS"]))
trace_level = os.environ["TRACE_LEVEL"].upper()
baud = 115200

boot_commands = [
    "BOOT_STATUS",
    "STORY_V2_ENABLE ON",
    f"STORY_V2_TRACE_LEVEL {trace_level}",
    "STORY_V2_STATUS",
    "STORY_V2_VALIDATE",
    "STORY_V2_HEALTH",
    "STORY_TEST_ON",
    "STORY_TEST_DELAY 2500",
    "STORY_ARM",
    "STORY_V2_EVENT ETAPE2_DUE",
    "STORY_V2_METRICS",
]

poll_commands = [
    "STORY_V2_HEALTH",
    "STORY_V2_METRICS",
    "SYS_LOOP_BUDGET STATUS",
    "SCREEN_LINK_STATUS",
    "MP3_SCAN_PROGRESS",
    "MP3_BACKEND_STATUS",
]

end_commands = [
    "STORY_V2_STATUS",
    "STORY_TEST_OFF",
    "STORY_V2_TRACE_LEVEL OFF",
]

def send_and_drain(ser, cmd, quiet=False):
    if not quiet:
        print(f">>> {cmd}")
    ser.write((cmd + "\n").encode("utf-8"))
    ser.flush()
    deadline = time.time() + 1.8
    got = False
    while time.time() < deadline:
        raw = ser.readline()
        if not raw:
            continue
        got = True
        print(raw.decode("utf-8", errors="replace").rstrip())
    if not got and not quiet:
        print("(no immediate response)")

with serial.Serial(port, baud, timeout=0.25) as ser:
    time.sleep(1.0)
    for cmd in boot_commands:
        send_and_drain(ser, cmd)
        time.sleep(0.15)

    soak_end = time.time() + soak_minutes * 60
    loop_count = 0
    while time.time() < soak_end:
        loop_count += 1
        print(f"--- soak poll #{loop_count} ---")
        for cmd in poll_commands:
            send_and_drain(ser, cmd, quiet=True)
            time.sleep(0.05)
        time.sleep(poll_seconds)

    for cmd in end_commands:
        send_and_drain(ser, cmd)
PY

log "RC matrix completed. Log file: ${LOG_FILE}"
log "Run manual reset-cross checks via tools/qa/live_story_v2_rc_runbook.md"
