#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/agent_utils.sh"

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
REPO_ROOT="$(cd "$ROOT/../.." && pwd)"
cd "$ROOT"

PHASE="smoke_tests"
OUTDIR="${ZACUS_OUTDIR:-}"
ORIG_ARGS=("$@")

usage() {
    cat <<'USAGE'
Usage: ./tools/dev/run_smoke_tests.sh [--outdir <path>] [--combined-board]

Options:
    --outdir <path>   Evidence output directory (default: artifacts/smoke_tests/<timestamp>)
    --combined-board  Run ESP32-only smoke (Freenove combined board mode)
    -h, --help        Show this help
USAGE
}

COMBINED_BOARD="${ZACUS_COMBINED_BOARD:-0}"
while [[ $# -gt 0 ]]; do
    case "$1" in
        --outdir)
            OUTDIR="${2:-}"
            shift 2
            ;;
        --combined-board)
            COMBINED_BOARD="1"
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "[error] unknown option: $1" >&2
            usage >&2
            exit 2
            ;;
    esac
done

EVIDENCE_CMDLINE="$0 ${ORIG_ARGS[*]:-}"
export EVIDENCE_CMDLINE
evidence_init "$PHASE" "$OUTDIR"

RESULT_OVERRIDE=""

FINALIZED=0
finalize() {
    local rc="$1"
    if [[ "$FINALIZED" == "1" ]]; then
        return
    fi
    FINALIZED=1
    trap - EXIT
    local result="PASS"
    if [[ "$rc" != "0" ]]; then
        result="FAIL"
    fi
    if [[ -n "$RESULT_OVERRIDE" ]]; then
        result="$RESULT_OVERRIDE"
    fi
    cat > "$EVIDENCE_SUMMARY" <<EOF
# Smoke tests summary

- Result: **${result}**
- Log: $(basename "$LOG_PATH")
- Ports: $(basename "$PORTS_JSON")
- Mode: $( [[ "$COMBINED_BOARD" == "1" ]] && echo "combined-board" || echo "dual-board" )
- Scenarios: ${SCENARIOS}
- Baud: ${BAUD}
- Duration per scenario: ${SCENARIO_DURATION_S}s
EOF
    echo "RESULT=${result}"
    exit "$rc"
}

trap 'finalize "$?"' EXIT

TIMESTAMP="$EVIDENCE_TIMESTAMP"
ARTIFACT_DIR="$EVIDENCE_DIR"
LOG_PATH="$ARTIFACT_DIR/smoke_tests.log"
PORTS_JSON="$ARTIFACT_DIR/ports_resolve.json"
RESOLVER="$REPO_ROOT/tools/test/resolve_ports.py"

SCENARIOS_DEFAULT="DEFAULT,EXPRESS,EXPRESS_DONE,SPECTRE"
SCENARIOS="${ZACUS_SMOKE_SCENARIOS:-$SCENARIOS_DEFAULT}"
SCENARIO_DURATION_S="${ZACUS_SCENARIO_DURATION_S:-8}"
BAUD="${ZACUS_SMOKE_BAUD:-115200}"

if [[ "${ZACUS_ENV:-}" == *"freenove_esp32s3"* ]]; then
  COMBINED_BOARD="1"
fi

mkdir -p "$ARTIFACT_DIR"
: >"$LOG_PATH"

if ! python3 -c "import serial" >/dev/null 2>&1; then
  fail "pyserial missing. Run ./tools/dev/bootstrap_local.sh first."
fi

if [[ "$COMBINED_BOARD" == "1" ]]; then
  log "Resolving ESP32 port (combined board mode)"
else
  log "Resolving ESP32/ESP8266 ports..."
fi
wait_port="${ZACUS_WAIT_PORT:-3}"
require_hw="${ZACUS_REQUIRE_HW:-0}"
if ! [[ "$wait_port" =~ ^[0-9]+$ ]]; then
  wait_port=3
fi

resolver_cmd=(
  python3
  "$RESOLVER"
  --wait-port "$wait_port"
  --auto-ports
  --need-esp32
  --ports-resolve-json "$PORTS_JSON"
)
if [[ "$COMBINED_BOARD" != "1" ]]; then
  resolver_cmd+=(--need-esp8266)
fi

if [[ "$(uname -s)" == "Darwin" ]]; then
  resolver_cmd+=(--prefer-cu)
fi
if [[ -n "${ZACUS_PORT_ESP32:-}" ]]; then
  resolver_cmd+=(--port-esp32 "${ZACUS_PORT_ESP32}")
fi
if [[ -n "${ZACUS_PORT_ESP8266:-}" ]]; then
  resolver_cmd+=(--port-esp8266 "${ZACUS_PORT_ESP8266}")
fi
if [[ "$require_hw" != "1" ]]; then
  resolver_cmd+=(--allow-no-hardware)
fi

set +e
evidence_record_command "${resolver_cmd[*]}"
"${resolver_cmd[@]}" >/dev/null 2>&1
resolve_rc=$?
set -e

if [[ "$resolve_rc" != "0" ]]; then
  if [[ "$require_hw" == "1" ]]; then
    fail "port resolution failed"
  fi
  log "Port resolution failed but ZACUS_REQUIRE_HW=0; skipping"
    RESULT_OVERRIDE="SKIP"
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
ports = data.get("ports", {})
print(f"esp32={ports.get('esp32','')}")
print(f"esp8266={ports.get('esp8266', ports.get('esp8266_usb',''))}")
print(f"reason_esp32={data.get('reasons',{}).get('esp32','')}")
print(f"reason_esp8266={data.get('reasons',{}).get('esp8266','')}")
PY
}

status_lines=$(read_status "$PORTS_JSON")
status=$(printf '%s\n' "$status_lines" | grep '^status=' | cut -d= -f2-)
port_esp32=$(printf '%s\n' "$status_lines" | grep '^esp32=' | cut -d= -f2-)
port_esp8266=$(printf '%s\n' "$status_lines" | grep '^esp8266=' | cut -d= -f2-)
reason_esp32=$(printf '%s\n' "$status_lines" | grep '^reason_esp32=' | cut -d= -f2-)
reason_esp8266=$(printf '%s\n' "$status_lines" | grep '^reason_esp8266=' | cut -d= -f2-)

if [[ "$status" != "pass" || -z "$port_esp32" || ("$COMBINED_BOARD" != "1" && -z "$port_esp8266") ]]; then
  if [[ "$require_hw" == "1" ]]; then
    if [[ "$COMBINED_BOARD" == "1" ]]; then
      fail "ESP32 port missing in combined mode (status=$status esp32=${port_esp32:-n/a})"
    fi
    fail "ESP32/ESP8266 ports missing (status=$status esp32=${port_esp32:-n/a} esp8266=${port_esp8266:-n/a})"
  fi
  if [[ "$COMBINED_BOARD" == "1" ]]; then
    log "No strict ESP32 mapping found in combined mode (status=$status). Skipping smoke."
  else
    log "No strict ESP32/ESP8266 mapping found (status=$status). Skipping smoke."
  fi
  RESULT_OVERRIDE="SKIP"
  exit 0
fi

if [[ "$COMBINED_BOARD" == "1" ]]; then
  log "Running combined-board smoke on ESP32=$port_esp32 (reason=$reason_esp32) (baud=$BAUD)"
else
  log "Running smoke on ESP32=$port_esp32 (reason=$reason_esp32) with ESP8266=$port_esp8266 (reason=$reason_esp8266) (baud=$BAUD)"
fi
evidence_record_command "python3 - <inline> esp32=$port_esp32 esp8266=$port_esp8266 combined=$COMBINED_BOARD baud=$BAUD scenarios=$SCENARIOS duration_s=$SCENARIO_DURATION_S"
set +e
python3 - "$port_esp32" "$port_esp8266" "$BAUD" "$LOG_PATH" "$SCENARIOS" "$SCENARIO_DURATION_S" "$COMBINED_BOARD" <<'PY'
import json
import re
import sys
import time
from datetime import datetime

try:
    import serial
except ImportError:
    print("pyserial missing", file=sys.stderr)
    sys.exit(2)

port_esp32 = sys.argv[1]
port_esp8266 = sys.argv[2]
baud = int(sys.argv[3])
log_path = sys.argv[4]
scenarios = [s.strip() for s in sys.argv[5].split(',') if s.strip()]
scenario_duration_s = float(sys.argv[6])
combined_board = (sys.argv[7] == "1")

fatal_re = re.compile(r"(PANIC|Guru Meditation|ASSERT|ABORT|REBOOT|rst:|watchdog)", re.IGNORECASE)
ui_down_re = re.compile(r"UI_LINK_STATUS.*connected=0", re.IGNORECASE)
load_ok_re = re.compile(r'"ok"\s*:\s*true.*"code"\s*:\s*"ok".*"detail"\s*:\s*"[^"]+"', re.IGNORECASE)
arm_ok_re = re.compile(r'"running"\s*:\s*true', re.IGNORECASE)
done_re = re.compile(r'STORY_ENGINE_DONE|step=STEP_DONE|step: done|-> STEP_DONE|"step"\s*:\s*"STEP_DONE"', re.IGNORECASE)
legacy_load_ok_re = re.compile(r"ACK SC_LOAD id=[A-Za-z0-9_\-]+ ok=1", re.IGNORECASE)
legacy_status_done_re = re.compile(r"step=STEP_DONE|STEP_DONE", re.IGNORECASE)


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
        if (not combined_board) and ui_down_re.search(line):
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


def send_json_cmd(ser, cmd: str, data=None):
    payload = {"cmd": cmd}
    if data:
        payload["data"] = data
    send_cmd(ser, json.dumps(payload, separators=(",", ":")))


def check_ui_link(ser) -> bool:
    if combined_board:
        log("SKIP ui link check: not needed for combined board")
        return True
    send_cmd(ser, "UI_LINK_STATUS")
    ok, info = scan_for(ser, [re.compile(r"UI_LINK_STATUS.*connected=1", re.IGNORECASE)], 2.0)
    if not ok:
        log(f"FAIL ui link: {info}")
        return False
    log("OK ui link connected")
    return True


def detect_story_protocol(ser) -> str:
    send_json_cmd(ser, "story.status")
    saw_unknown = False
    saw_json = False
    for line in read_lines(ser, 1.5):
        log(f"rx {line}")
        if fatal_re.search(line):
            return "legacy_sc"
        if 'UNKNOWN {"cmd":"story.status"}' in line:
            saw_unknown = True
        if '"running"' in line or '"step"' in line or '"ok"' in line:
            saw_json = True
    if saw_json and not saw_unknown:
        return "json_v3"

    send_cmd(ser, "HELP")
    saw_sc = False
    for line in read_lines(ser, 1.5):
        log(f"rx {line}")
        if fatal_re.search(line):
            return "legacy_sc"
        if "SC_LOAD" in line or "SC_LIST" in line:
            saw_sc = True
    if saw_unknown or saw_sc:
        return "legacy_sc"
    return "json_v3"


def run_scenario_json_v3(ser, scenario: str) -> bool:
    log(f"=== Scenario {scenario} ===")
    send_json_cmd(ser, "story.load", {"scenario": scenario})
    ok, info = scan_for(ser, [load_ok_re], 2.5)
    if not ok:
        log(f"FAIL load {scenario}: {info}")
        return False

    send_cmd(ser, "STORY_ARM")
    send_json_cmd(ser, "story.status")
    ok, info = scan_for(ser, [arm_ok_re], 2.5)
    if not ok:
        log(f"FAIL arm {scenario}: {info}")
        return False

    if not check_ui_link(ser):
        return False

    ok, info = scan_for(ser, [done_re], scenario_duration_s)
    if not ok and info == "timeout":
        log(f"WARN completion timeout for {scenario}, forcing ETAPE2 once")
        send_cmd(ser, "STORY_FORCE_ETAPE2")
        send_json_cmd(ser, "story.status")
        ok, info = scan_for(ser, [done_re], min(6.0, scenario_duration_s))
    if not ok:
        log(f"FAIL completion {scenario}: {info}")
        return False

    log(f"OK {scenario}")
    return True


def run_scenario_legacy_sc(ser, scenario: str) -> bool:
    log(f"=== Scenario {scenario} (legacy) ===")
    send_cmd(ser, f"SC_LOAD {scenario}")
    ok, info = scan_for(ser, [legacy_load_ok_re], 3.0)
    if not ok:
        log(f"FAIL legacy load {scenario}: {info}")
        return False

    if not check_ui_link(ser):
        return False

    for cmd in ("UNLOCK", "NEXT", "NEXT", "NEXT"):
        send_cmd(ser, cmd)
        ok, info = scan_for(
            ser,
            [re.compile(rf"ACK {re.escape(cmd)}$", re.IGNORECASE), done_re, legacy_status_done_re],
            2.2,
        )
        if ok and (done_re.search(info) or legacy_status_done_re.search(info)):
            log(f"OK {scenario}")
            return True

    log(f"WARN completion timeout for {scenario}, forcing ETAPE2 once")
    send_cmd(ser, "STORY_FORCE_ETAPE2")
    ok, info = scan_for(ser, [done_re, legacy_status_done_re], min(6.0, scenario_duration_s))
    if not ok:
        log(f"FAIL completion {scenario}: {info}")
        return False

    log(f"OK {scenario}")
    return True


try:
    with serial.Serial(port_esp32, baud, timeout=0.4) as ser:
        time.sleep(0.6)
        ser.reset_input_buffer()
        if combined_board:
            log(f"Using ESP32={port_esp32} (combined board mode)")
        else:
            log(f"Using ESP32={port_esp32} ESP8266={port_esp8266}")
        protocol = detect_story_protocol(ser)
        log(f"Story protocol: {protocol}")
        send_cmd(ser, "BOOT_NEXT")
        if protocol == "json_v3":
            send_cmd(ser, "STORY_TEST_ON")
            send_cmd(ser, "STORY_TEST_DELAY 1000")
        failures = 0
        for scenario in scenarios:
            if protocol == "legacy_sc":
                ok = run_scenario_legacy_sc(ser, scenario)
            else:
                ok = run_scenario_json_v3(ser, scenario)
            if not ok:
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
exit_code=$?
set -e
exit "$exit_code"
