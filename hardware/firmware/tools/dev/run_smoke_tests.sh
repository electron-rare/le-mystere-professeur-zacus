#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/agent_utils.sh"

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

TIMESTAMP="$(date -u +"%Y%m%d-%H%M%S")"
ARTIFACT_DIR="$ROOT/artifacts/rc_live"
LOG_PATH="$ARTIFACT_DIR/smoke_${TIMESTAMP}.log"
PORTS_JSON="$ARTIFACT_DIR/smoke_ports_${TIMESTAMP}.json"

SCENARIOS_DEFAULT="DEFAULT,EXPRESS,EXPRESS_DONE,SPECTRE"
SCENARIOS="${ZACUS_SMOKE_SCENARIOS:-$SCENARIOS_DEFAULT}"
SCENARIO_DURATION_S="${ZACUS_SCENARIO_DURATION_S:-8}"
BAUD="${ZACUS_SMOKE_BAUD:-115200}"

mkdir -p "$ARTIFACT_DIR"
: >"$LOG_PATH"

if ! python3 -c "import serial" >/dev/null 2>&1; then
  fail "pyserial missing. Run ./tools/dev/bootstrap_local.sh first."
fi

log "Resolving ESP32 port..."
set +e
python3 tools/test/resolve_ports.py \
    --auto-ports --need-esp32 --allow-no-hardware \
    --ports-resolve-json "$PORTS_JSON" >/dev/null 2>&1
resolve_rc=$?
set -e

if [[ "$resolve_rc" != "0" ]]; then
  if [[ "${ZACUS_REQUIRE_HW:-0}" == "1" ]]; then
    fail "port resolution failed"
  fi
  log "Port resolution failed but ZACUS_REQUIRE_HW=0; skipping"
  exit 0
fi

read_status() {
    python3 - "$1" <<'PY'
import json
import sys
from pathlib import Path

path = Path(sys.argv[1])
if not path.exists():
    print("status=fail")
    sys.exit(0)

data = json.loads(path.read_text(encoding="utf-8"))
print(f"status={data.get('status','fail')}")
print(f"port={data.get('ports',{}).get('esp32','')}")
PY
}

status_lines=$(read_status "$PORTS_JSON")
status=$(printf '%s\n' "$status_lines" | grep '^status=' | cut -d= -f2-)
port=$(printf '%s\n' "$status_lines" | grep '^port=' | cut -d= -f2-)

if [[ "$status" != "pass" || -z "$port" ]]; then
  if [[ "${ZACUS_REQUIRE_HW:-0}" == "1" ]]; then
    fail "ESP32 port missing (status=$status)"
  fi
  log "No ESP32 port found (status=$status). Skipping smoke."
  exit 0
fi

log "Running smoke on $port (baud=$BAUD)"
python3 - "$port" "$BAUD" "$LOG_PATH" "$SCENARIOS" "$SCENARIO_DURATION_S" <<'PY'
import re
import sys
import time
from datetime import datetime

try:
    import serial
except ImportError:
    print("pyserial missing", file=sys.stderr)
    sys.exit(2)

port = sys.argv[1]
baud = int(sys.argv[2])
log_path = sys.argv[3]
scenarios = [s.strip() for s in sys.argv[4].split(',') if s.strip()]
scenario_duration_s = float(sys.argv[5])

fatal_re = re.compile(r"(PANIC|Guru Meditation|ASSERT|ABORT|REBOOT|rst:|watchdog)", re.IGNORECASE)
ui_down_re = re.compile(r"UI_LINK_STATUS.*connected=0", re.IGNORECASE)
load_ok_re = re.compile(r"STORY_LOAD_SCENARIO_OK", re.IGNORECASE)
arm_ok_re = re.compile(r"STORY_ARM_OK|armed=1", re.IGNORECASE)
done_re = re.compile(r"STORY_ENGINE_DONE|step=STEP_DONE|step: done|\bdone\b", re.IGNORECASE)


def ts(msg: str) -> str:
    stamp = datetime.now().strftime("%H:%M:%S")
    return f"[{stamp}] {msg}"


def log(msg: str) -> None:
    line = ts(msg)
    print(line, flush=True)
    with open(log_path, "a", encoding="utf-8") as fp:
        fp.write(line + "\n")


def read_lines(ser, duration_s: float):
    deadline = time.time() + duration_s
    while time.time() < deadline:
        raw = ser.readline()
        if not raw:
            continue
        line = raw.decode("utf-8", errors="ignore").strip()
        if not line:
            continue
        yield line


def scan_for(ser, patterns, duration_s: float):
    for line in read_lines(ser, duration_s):
        log(f"rx {line}")
        if fatal_re.search(line):
            return False, f"fatal marker: {line}"
        if ui_down_re.search(line):
            return False, f"ui link down: {line}"
        for pat in patterns:
            if pat.search(line):
                return True, line
    return False, "timeout"


def send_cmd(ser, cmd: str):
    ser.write((cmd + "\n").encode("utf-8"))
    ser.flush()
    log(f"tx {cmd}")
    time.sleep(0.2)


def check_ui_link(ser) -> bool:
    send_cmd(ser, "UI_LINK_STATUS")
    ok, info = scan_for(ser, [re.compile(r"UI_LINK_STATUS.*connected=1", re.IGNORECASE)], 2.0)
    if not ok:
        log(f"FAIL ui link: {info}")
        return False
    log("OK ui link connected")
    return True


def run_scenario(ser, scenario: str) -> bool:
    log(f"=== Scenario {scenario} ===")
    send_cmd(ser, f"STORY_LOAD_SCENARIO {scenario}")
    ok, info = scan_for(ser, [load_ok_re], 2.5)
    if not ok:
        log(f"FAIL load {scenario}: {info}")
        return False

    send_cmd(ser, "STORY_ARM")
    send_cmd(ser, "STORY_STATUS")
    ok, info = scan_for(ser, [arm_ok_re], 2.5)
    if not ok:
        log(f"FAIL arm {scenario}: {info}")
        return False

    if not check_ui_link(ser):
        return False

    ok, info = scan_for(ser, [done_re], scenario_duration_s)
    if not ok:
        log(f"FAIL completion {scenario}: {info}")
        return False

    log(f"OK {scenario}")
    return True


try:
    with serial.Serial(port, baud, timeout=0.4) as ser:
        time.sleep(0.6)
        ser.reset_input_buffer()
        failures = 0
        for scenario in scenarios:
            if not run_scenario(ser, scenario):
                failures += 1
                break

    if failures:
        log("Smoke test failed")
        sys.exit(1)

    log("All smoke tests passed")
    sys.exit(0)
except Exception as exc:
    log(f"ERROR serial failure: {exc}")
    sys.exit(2)
PY
