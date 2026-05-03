#!/usr/bin/env bash
# voice-bridge watchdog
#
# Idempotent: silent exit when service is up; restarts via the same nohup
# pattern as the @reboot crontab line when down. Designed to be invoked
# every 2 minutes by cron.
#
# Layout (on studio):
#   ~/voice-bridge/             venv + main.py + service_down.wav + this script
#   ~/voice-bridge.log          uvicorn stdout/stderr (also reads watchdog notes)
#   ~/voice-bridge-watchdog.log cron capture (this script's own stdout/stderr)
#
# Exit codes
#   0  service was already up, or restart issued
#   2  restart attempted but venv/script missing (operator action required)

set -u

VB_DIR="$HOME/voice-bridge"
VB_VENV="$VB_DIR/.venv/bin/python"
VB_LOG="$HOME/voice-bridge.log"
VB_PORT=8200
VB_PATTERN="uvicorn main:app.*--port ${VB_PORT}"

# Service is up: silent exit (no log spam every 2 min).
if pgrep -f "${VB_PATTERN}" >/dev/null 2>&1; then
    exit 0
fi

# Down: capture timestamp + log into uvicorn log (operator-visible).
TS="$(date -u +%Y-%m-%dT%H:%M:%SZ)"
REASON="no process matching '${VB_PATTERN}'"

if [[ ! -x "${VB_VENV}" ]] || [[ ! -f "${VB_DIR}/main.py" ]]; then
    echo "${TS} watchdog: cannot restart, missing venv or main.py (${VB_DIR})" >> "${VB_LOG}"
    exit 2
fi

echo "${TS} watchdog: restarted at ${TS} (reason: ${REASON})" >> "${VB_LOG}"

nohup "${VB_VENV}" -m uvicorn main:app \
    --host 0.0.0.0 --port ${VB_PORT} \
    --app-dir "${VB_DIR}" \
    >> "${VB_LOG}" 2>&1 < /dev/null &

# Detach so cron doesn't wait on the child.
disown 2>/dev/null || true
exit 0
