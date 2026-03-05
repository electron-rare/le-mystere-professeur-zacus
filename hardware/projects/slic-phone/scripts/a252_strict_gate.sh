#!/usr/bin/env bash
set -uo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${ROOT_DIR}"

A252_PORT="${A252_PORT:-/dev/cu.usbserial-0001}"
ZEROCLAW_ORCH="${ZEROCLAW_ORCH:-http://127.0.0.1:8788}"
ZEROCLAW_BIN="${ZEROCLAW_BIN:-zeroclaw}"
A252_HOOK_OBSERVE_SECONDS="${A252_HOOK_OBSERVE_SECONDS:-45}"
A252_REQUIRE_HOOK_TOGGLE="${A252_REQUIRE_HOOK_TOGGLE:-1}"
A252_IGNORE_ZEROCLAW="${A252_IGNORE_ZEROCLAW:-1}"

HW_REPORT_JSON="artifacts/hw_validation_a252_report.json"
HW_REPORT_MD="docs/hw_validation_a252_report.md"
ZEROCLAW_REPORT_JSON="artifacts/zeroclaw_orchestrator_health.json"
SUMMARY_MD="docs/a252_strict_gate_summary.md"

STATUS_PREFLIGHT="NOT_RUN"
STATUS_ZEROCLAW="NOT_RUN"
STATUS_BRANCH_GATE="NOT_RUN"
STATUS_HW_VALIDATION="NOT_RUN"
OVERALL="FAIL"

log() {
    echo "[a252-strict-gate] $*"
}

is_true() {
    case "${1,,}" in
        1|true|yes|on)
            return 0
            ;;
        *)
            return 1
            ;;
    esac
}

write_summary() {
    STATUS_PREFLIGHT="${STATUS_PREFLIGHT}" \
    STATUS_ZEROCLAW="${STATUS_ZEROCLAW}" \
    STATUS_BRANCH_GATE="${STATUS_BRANCH_GATE}" \
    STATUS_HW_VALIDATION="${STATUS_HW_VALIDATION}" \
    OVERALL="${OVERALL}" \
    ZEROCLAW_ORCH="${ZEROCLAW_ORCH}" \
    A252_PORT="${A252_PORT}" \
    HW_REPORT_JSON="${HW_REPORT_JSON}" \
    ZEROCLAW_REPORT_JSON="${ZEROCLAW_REPORT_JSON}" \
    SUMMARY_MD="${SUMMARY_MD}" \
    python3 - <<'PY'
import json
import os
from datetime import datetime, timezone
from pathlib import Path

summary_path = Path(os.environ["SUMMARY_MD"])
summary_path.parent.mkdir(parents=True, exist_ok=True)

def load_json(path: Path):
    if not path.exists():
        return None
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except Exception:
        return None

hw_payload = load_json(Path(os.environ["HW_REPORT_JSON"]))
zc_payload = load_json(Path(os.environ["ZEROCLAW_REPORT_JSON"]))

lines = [
    "# A252 Strict Gate Summary",
    "",
    f"- Date UTC: {datetime.now(timezone.utc).isoformat()}",
    f"- Verdict global: {os.environ['OVERALL']}",
    f"- Port A252: `{os.environ['A252_PORT']}`",
    f"- ZeroClaw: `{os.environ['ZEROCLAW_ORCH']}`",
    "",
    "## Étapes",
    "",
    "| Étape | État |",
    "|---|---|",
    f"| zeroclaw_hw_preflight | {os.environ['STATUS_PREFLIGHT']} |",
    f"| zeroclaw_orchestrator_health | {os.environ['STATUS_ZEROCLAW']} |",
    f"| branch_gate_profile_a252 | {os.environ['STATUS_BRANCH_GATE']} |",
    f"| hw_validation_a252 | {os.environ['STATUS_HW_VALIDATION']} |",
]

if hw_payload:
    lines.extend([
        "",
        "## Stacks A252 (hw_validation)",
        "",
        "| Stack/Scénario | État |",
        "|---|---|",
    ])
    for item in hw_payload.get("results", []):
        name = item.get("name", "")
        state = item.get("state", "")
        lines.append(f"| {name} | {state} |")

if zc_payload:
    lines.extend([
        "",
        "## ZeroClaw Docker",
        "",
        "| Check | État |",
        "|---|---|",
    ])
    for item in zc_payload.get("results", []):
        name = item.get("name", "")
        state = item.get("state", "")
        lines.append(f"| {name} | {state} |")

summary_path.write_text("\n".join(lines) + "\n", encoding="utf-8")
PY
}

run_step() {
    local step_var="$1"
    local description="$2"
    shift 2
    log "${description}"
    if "$@"; then
        printf -v "${step_var}" "PASS"
        return 0
    fi
    printf -v "${step_var}" "FAIL"
    OVERALL="FAIL"
    write_summary
    return 1
}

HOOK_TOGGLE_FLAG="--no-require-hook-toggle"
if is_true "${A252_REQUIRE_HOOK_TOGGLE}"; then
    HOOK_TOGGLE_FLAG="--require-hook-toggle"
fi

if [[ -z "${A252_WIFI_SSID:-}" || -z "${A252_WIFI_PASSWORD:-}" ]]; then
    log "erreur: A252_WIFI_SSID et A252_WIFI_PASSWORD sont requis"
    write_summary
    exit 2
fi

if is_true "${A252_IGNORE_ZEROCLAW}"; then
    STATUS_PREFLIGHT="MANUAL_SKIP"
    STATUS_ZEROCLAW="MANUAL_SKIP"
    log "ZeroClaw ignoré (A252_IGNORE_ZEROCLAW=${A252_IGNORE_ZEROCLAW})"
else
    run_step STATUS_PREFLIGHT "ZeroClaw hardware preflight" \
        python3 scripts/zeroclaw_hw_preflight.py \
        --zeroclaw-bin "${ZEROCLAW_BIN}" \
        --require-port \
        --port "${A252_PORT}" || exit 1

    run_step STATUS_ZEROCLAW "ZeroClaw orchestrator health + provider_scan" \
        python3 scripts/zeroclaw_orchestrator_health.py \
        --base-url "${ZEROCLAW_ORCH}" \
        --report-json "${ZEROCLAW_REPORT_JSON}" || exit 1
fi

run_step STATUS_BRANCH_GATE "Branch gate profile a252" \
    bash scripts/branch_gate.sh --profile a252 || exit 1

run_step STATUS_HW_VALIDATION "Hardware validation A252 strict" \
    pio run -e esp32dev -t upload_ffat --upload-port "${A252_PORT}" && \
    python3 scripts/hw_validation.py \
    --port-a252 "${A252_PORT}" \
    --flash \
    --wifi-ssid "${A252_WIFI_SSID}" \
    --wifi-password "${A252_WIFI_PASSWORD}" \
    --strict-serial-smoke \
    --allow-capture-fail-when-disabled \
    "${HOOK_TOGGLE_FLAG}" \
    --hook-observe-seconds "${A252_HOOK_OBSERVE_SECONDS}" \
    --report-json "${HW_REPORT_JSON}" \
    --report-md "${HW_REPORT_MD}" || exit 1

OVERALL="PASS"
write_summary
log "gate A252 strict terminé avec succès"
