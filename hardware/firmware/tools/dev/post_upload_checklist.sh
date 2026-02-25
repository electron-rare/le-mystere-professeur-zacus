#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
FW_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

source "${SCRIPT_DIR}/agent_utils.sh"

ENV="freenove_esp32s3"
BAUD=115200
WAIT_PORT=12
MONITOR_SECONDS=18
LOG_LINES=20
UPLOAD=0
PORT=""
SCENE=""
SCENE_CHECK=0
REQUIRED_REGEX=""
VISUAL_CONFIRM=1
OUTDIR=""

usage() {
  cat <<'EOF'
Usage:
  post_upload_checklist.sh [options]

Options:
  --env <env>                PlatformIO env for upload (default: freenove_esp32s3)
  --baud <baud>              Serial baud (default: 115200)
  --port <tty>               Force serial port (auto-detect by default)
  --wait-port <sec>          Port detection timeout (default: 12)
  --monitor-seconds <sec>    Serial capture duration (default: 18)
  --lines <n>                Minimum serial lines to capture (default: 20)
  --upload                   Run pio upload before checklist
  --scene <SCENE_ID>         Send SCENE_GOTO and require ACK (optional)
  --required-regex <regex>    Require this regex in first N captured lines (optional)
  --no-visual                Skip manual visual confirmation prompt
  --outdir <path>            Evidence directory (default: artifacts/post_upload/<stamp>)
  --help                     Show this help

Example:
  ./tools/dev/post_upload_checklist.sh --env freenove_esp32s3 --upload --scene SCENE_WIN_ETAPE
EOF
}

log_usage_and_exit() {
  usage
  exit 1
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --env)
      ENV="${2:?}"
      shift 2
      ;;
    --baud)
      BAUD="${2:?}"
      shift 2
      ;;
    --port)
      PORT="${2:?}"
      shift 2
      ;;
    --wait-port)
      WAIT_PORT="${2:?}"
      shift 2
      ;;
    --monitor-seconds)
      MONITOR_SECONDS="${2:?}"
      shift 2
      ;;
    --lines)
      LOG_LINES="${2:?}"
      shift 2
      ;;
    --upload)
      UPLOAD=1
      shift
      ;;
    --scene)
      SCENE="${2:?}"
      SCENE_CHECK=1
      shift 2
      ;;
    --required-regex)
      REQUIRED_REGEX="${2:?}"
      shift 2
      ;;
    --no-visual)
      VISUAL_CONFIRM=0
      shift
      ;;
    --outdir)
      OUTDIR="${2:?}"
      shift 2
      ;;
    --help|-h)
      usage
      exit 0
      ;;
    *)
      echo "Unknown argument: $1"
      log_usage_and_exit
      ;;
  esac
done

if [[ "${LOG_LINES}" -le 0 ]]; then
  echo "[error] --lines must be >= 1"
  exit 1
fi

if [[ "${MONITOR_SECONDS}" -le 0 ]]; then
  echo "[error] --monitor-seconds must be > 0"
  exit 1
fi

if [[ -n "${OUTDIR}" ]]; then
  if [[ "${OUTDIR}" = /* ]]; then
    evidence_init "post_upload_checklist" "${OUTDIR}"
  else
    evidence_init "post_upload_checklist" "${FW_ROOT}/${OUTDIR}"
  fi
else
  evidence_init "post_upload_checklist"
fi
evidence_record_command "$0 $*"

PYTHON_EXEC="${FW_ROOT}/.venv/bin/python3"
if [[ ! -x "$PYTHON_EXEC" ]]; then
  PYTHON_EXEC="python3"
fi

resolve_port() {
  if [[ -n "${PORT}" ]]; then
    echo "[info] Serial port forced: ${PORT}"
    return 0
  fi

  local ports_json="${EVIDENCE_DIR}/ports.json"
  local port_result
  local status

  if [[ "${WAIT_PORT}" -lt 1 ]]; then
    WAIT_PORT=1
  fi

  "${PYTHON_EXEC}" "${FW_ROOT}/tools/test/resolve_ports.py" \
    --auto-ports --need-esp32 --wait-port "${WAIT_PORT}" --ports-resolve-json "${ports_json}" \
    >/dev/null
  status=$?
  if [[ "${status}" -ne 0 ]]; then
    echo "[fail] Aucun port ESP32 détecté (status=${status})"
    return 1
  fi

  port_result=$("${PYTHON_EXEC}" - "${ports_json}" <<'PY'
import json
import sys

data = json.load(open(sys.argv[1], "r", encoding="utf-8"))
port = data.get("ports", {}).get("esp32", "")
sys.stdout.write(port)
PY
)
  if [[ -z "${port_result}" ]]; then
    echo "[fail] Résolution de port OK, aucun ESP32 dans le mapping."
    return 1
  fi
  PORT="${port_result}"
  echo "[info] Port auto-détecté: ${PORT}"
  return 0
}

run_upload() {
  if [[ "${UPLOAD}" -ne 1 ]]; then
    return 0
  fi
  log "[step] pio run -e ${ENV} -t upload --upload-port \"${PORT}\""
  if ! (cd "${FW_ROOT}" && pio run -e "${ENV}" -t upload --upload-port "${PORT}") | tee "${EVIDENCE_DIR}/upload.log"; then
    echo "[fail] Upload failed"
    return 1
  fi
  return 0
}

collect_logs() {
  local log_path="${EVIDENCE_DIR}/post_upload_serial.log"
  echo "[step] Capture série: ${LOG_LINES} lignes min sur ${MONITOR_SECONDS}s (port=${PORT}, baud=${BAUD})"
  "${PYTHON_EXEC}" - "${PORT}" "${BAUD}" "${MONITOR_SECONDS}" "${LOG_LINES}" "${SCENE}" "${SCENE_CHECK}" "${REQUIRED_REGEX}" "${log_path}" <<'PY'
import re
import sys
import time

try:
    from serial import Serial
except Exception as exc:
    raise SystemExit(f"pyserial missing: {exc}")

port = sys.argv[1]
baud = int(sys.argv[2])
monitor_seconds = float(sys.argv[3])
line_target = int(sys.argv[4])
scene = sys.argv[5]
scene_check = sys.argv[6] == "1"
required_regex = sys.argv[7]
log_path = sys.argv[8]

fatal_re = re.compile(
    r"(User exception|panic|abort|assert|Guru Meditation|rst cause|Stack|Fatal)",
    re.IGNORECASE,
)
boot_re = re.compile(r"(boot mode|load:0x|entry 0x|ets Jan)", re.IGNORECASE)
required_re = re.compile(required_regex, re.IGNORECASE) if required_regex else None

scene_ack_ok = False
required_ok = False

start = time.time()
deadline = start + monitor_seconds
lines = []

with Serial(port, baud, timeout=0.2) as ser:
    time.sleep(0.2)
    ser.reset_input_buffer()
    ser.write(b"PING\n")
    if scene:
        ser.write((f"SCENE_GOTO {scene}\n").encode("utf-8"))

    while len(lines) < line_target and time.time() < deadline:
        raw = ser.readline()
        if not raw:
            continue
        text = raw.decode("utf-8", errors="ignore").strip()
        if not text:
            text = raw.hex()
            prefix = "[rx-bin]"
        else:
            prefix = "[rx]"
        entry = f"{prefix} {text}"
        lines.append(entry)
        print(f"{len(lines):02d}: {entry}")

        if fatal_re.search(text):
            with open(log_path, "w", encoding="utf-8") as fp:
                fp.write("\n".join(lines) + "\n")
            print(f"[fail] fatal marker: {text}")
            raise SystemExit(2)
        if boot_re.search(text):
            # boot marker is valid but not terminal
            pass
        if required_re and required_re.search(text):
            required_ok = True
        if scene and "ACK SCENE_GOTO ok=1" in text:
            scene_ack_ok = True

if len(lines) < line_target:
    with open(log_path, "w", encoding="utf-8") as fp:
        fp.write("\n".join(lines) + "\n")
    print(f"[fail] only {len(lines)} lignes capturées (<{line_target})")
    raise SystemExit(3)

if required_re and not required_ok:
    with open(log_path, "w", encoding="utf-8") as fp:
        fp.write("\n".join(lines) + "\n")
    print("[fail] required pattern not found in captured lines")
    raise SystemExit(4)

if scene_check and not scene_ack_ok:
    with open(log_path, "w", encoding="utf-8") as fp:
        fp.write("\n".join(lines) + "\n")
    print(f"[fail] SCENE_GOTO {scene} ack absent")
    raise SystemExit(5)

with open(log_path, "w", encoding="utf-8") as fp:
    fp.write("\n".join(lines) + "\n")
PY
  local rc=$?
  if [[ "${rc}" -ne 0 ]]; then
    return "${rc}"
  fi
  return 0
}

if ! resolve_port; then
  echo "[fail] port resolution failed"
  exit 1
fi

if ! run_upload; then
  echo "[fail] upload failed"
  exit 1
fi

if ! collect_logs; then
  echo "[fail] serial checklist failed (see ${EVIDENCE_DIR}/post_upload_serial.log)"
  exit 1
fi

echo "[ok] Logs capturés (${LOG_LINES} lignes, ${MONITOR_SECONDS}s) => ${EVIDENCE_DIR}/post_upload_serial.log"
tail -n 20 "${EVIDENCE_DIR}/post_upload_serial.log"

if [[ "${VISUAL_CONFIRM}" -eq 1 ]]; then
  echo
  read -r -p "Confirmation visuelle requise: texte et FX visibles ? [y/N] " ans
  if [[ ! "${ans}" =~ ^[Yy] ]]; then
    echo "[fail] confirmation visuelle refusée"
    echo "Rejouer le test après vérification manuelle de l'écran."
    exit 1
  fi
fi

echo "[ok] checklist post-upload terminée"
