#!/usr/bin/env bash
set -euo pipefail

REPO="electron-rare/le-mystere-professeur-zacus"
PROJECT_OWNER=""
PROJECT_NUMBER=""
DRY_RUN=0
ASSUME_YES=0
VERBOSE=0

DEFAULT_COLUMNS=(
  "Backlog"
  "Sprint Ready"
  "In Progress"
  "PR Review"
  "Live Validation"
  "Done"
)

LABEL_ROWS=(
  "rc:test|0052cc|RC test execution cycle"
  "gate:cheap|5319e7|Cheap/local gate required"
  "gate:live|b60205|Live hardware gate required"
  "risk:blocker|d93f0b|RC blocker"
  "sprint:s1|0e8a16|Sprint 1"
  "sprint:s2|1d76db|Sprint 2"
  "sprint:s3|a371f7|Sprint 3"
  "sprint:s4|fbca04|Sprint 4"
  "sprint:s5|c5def5|Sprint 5"
)

ISSUE_ROWS=(
  "RCT-01|Baseline freeze and test inventory|Sprint: S1\nPR branch: pr/rct-01-baseline-freeze\nBase branch: hardware/firmware\nGate: cheap + live\nDoD: tooling inventory frozen and baseline evidence linked.|rc:test,sprint:s1,gate:cheap,gate:live"
  "RCT-02|Content gate CI/local|Sprint: S1\nPR branch: pr/rct-02-content-gate\nBase branch: hardware/firmware\nGate: cheap\nDoD: content checks stable and reproducible in CI/local.|rc:test,sprint:s1,gate:cheap"
  "RCT-03|MP3 suites hardening|Sprint: S2\nPR branch: pr/rct-03-mp3-suites-hardening\nBase branch: hardware/firmware\nGate: cheap\nDoD: mp3_basic/mp3_fx suites robust in no-hardware and live contexts.|rc:test,sprint:s2,gate:cheap"
  "RCT-04|MP3 live evidence|Sprint: S2\nPR branch: pr/rct-04-mp3-live-evidence\nBase branch: hardware/firmware\nGate: live\nDoD: live evidence published for SD present and degraded mode.|rc:test,sprint:s2,gate:live"
  "RCT-05|Story V2 basic stabilization|Sprint: S3\nPR branch: pr/rct-05-story-v2-basic\nBase branch: hardware/firmware\nGate: cheap\nDoD: story_v2_basic suite stable with expected tolerances.|rc:test,sprint:s3,gate:cheap"
  "RCT-06|Story V2 metrics live evidence|Sprint: S3\nPR branch: pr/rct-06-story-v2-metrics-live\nBase branch: hardware/firmware\nGate: live\nDoD: metrics and reset observed and logged in live run.|rc:test,sprint:s3,gate:live"
  "RCT-07|UI Link simulator live E2E|Sprint: S4\nPR branch: pr/rct-07-ui-link-sim-live\nBase branch: hardware/firmware\nGate: cheap + live\nDoD: HELLO/ACK/KEYFRAME/PONG and scripted BTN sequence validated.|rc:test,sprint:s4,gate:cheap,gate:live"
  "RCT-08|Menu and mini-REPL live validation|Sprint: S4\nPR branch: pr/rct-08-menu-live\nBase branch: hardware/firmware\nGate: cheap + live\nDoD: curses and fallback flows validated with operator sanity checks.|rc:test,sprint:s4,gate:cheap,gate:live"
  "RCT-09|Final RC gates|Sprint: S5\nPR branch: pr/rct-09-final-gates\nBase branch: hardware/firmware\nGate: cheap + build + live\nDoD: final gates replayed with exact command evidence and KO diagnostics.|rc:test,sprint:s5,gate:cheap,gate:live,risk:blocker"
  "RCT-10|RC closeout report|Sprint: S5\nPR branch: pr/rct-10-closeout-report\nBase branch: hardware/firmware\nGate: documentation\nDoD: final report table complete and rollback note recorded.|rc:test,sprint:s5,gate:cheap"
)

declare -a LABEL_CREATED=()
declare -a LABEL_UPDATED=()
declare -a LABEL_UNCHANGED=()
declare -a ISSUE_CREATED=()
declare -a ISSUE_SKIPPED=()
declare -a BOARD_COLUMNS=()

BOARD_URL="n/a"
BOARD_NOTE=""
BOARD_STATUS_COUNTS=""
BOARD_RESOLVED_TITLE=""

usage() {
  cat <<'EOF'
Usage:
  tools/dev/rc_execution_seed.sh [options]

Options:
  --repo <owner/repo>        Target repository (default: electron-rare/le-mystere-professeur-zacus)
  --project-owner <owner>    GitHub owner for Projects (default: inferred from --repo)
  --project-number <n>       Explicit project number to inspect
  --dry-run                  Preview only, no remote mutations
  --yes                      Apply without confirmation prompt
  --verbose                  Print gh commands before running
  -h, --help                 Show this message


# Centralise les helpers via agent_utils.sh
source "$(dirname "$0")/../agent_utils.sh"

run_cmd() {
  if (( VERBOSE )); then
    printf '+'
    for token in "$@"; do
      printf ' %q' "$token"
    done
    printf '\n'
  fi
  "$@"
}

require_tools() {
  command -v gh >/dev/null 2>&1 || die "gh CLI not found"
  command -v python3 >/dev/null 2>&1 || die "python3 not found"
}

parse_args() {
  while [[ $# -gt 0 ]]; do
    case "$1" in
      --repo)
        REPO="${2:-}"
        shift 2
        ;;
      --project-owner)
        PROJECT_OWNER="${2:-}"
        shift 2
        ;;
      --project-number)
        PROJECT_NUMBER="${2:-}"
        shift 2
        ;;
      --dry-run)
        DRY_RUN=1
        shift
        ;;
      --yes)
        ASSUME_YES=1
        shift
        ;;
      --verbose)
        VERBOSE=1
        shift
        ;;
      -h|--help)
        usage
        exit 0
        ;;
      *)
        die "Unknown option: $1"
        ;;
    esac
  done

  [[ "$REPO" == */* ]] || die "--repo must be in owner/repo format"
  if [[ -z "$PROJECT_OWNER" ]]; then
    PROJECT_OWNER="${REPO%%/*}"
  fi
  if [[ -n "$PROJECT_NUMBER" && ! "$PROJECT_NUMBER" =~ ^[0-9]+$ ]]; then
    die "--project-number must be numeric"
  fi
}

confirm_apply() {
  if (( DRY_RUN || ASSUME_YES )); then
    return 0
  fi
  if [[ ! -t 0 ]]; then
    log "[confirm] non-interactive session detected; use --yes or --dry-run. No changes applied."
    exit 0
  fi
  printf 'Proceed with label/issue updates on %s? [y/N] ' "$REPO"
  read -r answer
  case "${answer,,}" in
    y|yes) return 0 ;;
    *)
      log "[confirm] aborted by user; no changes applied."
      exit 0
      ;;
  esac
}

load_labels_json() {
  run_cmd gh label list --repo "$REPO" --limit 200 --json name,color,description
}

label_action_for() {
  local labels_json="$1"
  local name="$2"
  local color="$3"
  local description="$4"

  LABELS_JSON="$labels_json" python3 - "$name" "$color" "$description" <<'PY'
import json
import os
import sys

name, wanted_color, wanted_desc = sys.argv[1], sys.argv[2].lower(), sys.argv[3]
rows = json.loads(os.environ["LABELS_JSON"])
for row in rows:
    if row.get("name") == name:
        have_color = (row.get("color") or "").lower()
        have_desc = row.get("description") or ""
        if have_color == wanted_color and have_desc == wanted_desc:
            print("unchanged")
        else:
            print("update")
        break
else:
    print("create")
PY
}

ensure_labels() {
  local labels_json="$1"
  local row
  for row in "${LABEL_ROWS[@]}"; do
    IFS='|' read -r name color description <<<"$row"
    local action
    action="$(label_action_for "$labels_json" "$name" "$color" "$description")"
    case "$action" in
      unchanged)
        LABEL_UNCHANGED+=("$name")
        log "[label] unchanged: $name"
        ;;
      update)
        LABEL_UPDATED+=("$name")
        if (( DRY_RUN )); then
          log "[label] dry-run update: $name"
        else
          run_cmd gh label edit "$name" --repo "$REPO" --color "$color" --description "$description" >/dev/null
          log "[label] updated: $name"
        fi
        ;;
      create)
        LABEL_CREATED+=("$name")
        if (( DRY_RUN )); then
          log "[label] dry-run create: $name"
        else
          run_cmd gh label create "$name" --repo "$REPO" --color "$color" --description "$description" >/dev/null
          log "[label] created: $name"
        fi
        ;;
      *)
        die "unexpected label action '$action' for $name"
        ;;
    esac
  done
}

load_issue_index() {
  run_cmd gh issue list --repo "$REPO" --state all --limit 300 --json number,title,state,url
}

issue_lookup() {
  local issues_json="$1"
  local card_id="$2"
  ISSUES_JSON="$issues_json" python3 - "$card_id" <<'PY'
import json
import os
import re
import sys

card = sys.argv[1]
pattern = re.compile(rf"^\[{re.escape(card)}\]\s")
rows = json.loads(os.environ["ISSUES_JSON"])
for row in rows:
    title = row.get("title") or ""
    if pattern.match(title):
        print(f"{row.get('number')}|{row.get('state')}|{row.get('url')}")
        break
PY
}

ensure_issues() {
  local issues_json="$1"
  local row
  for row in "${ISSUE_ROWS[@]}"; do
    IFS='|' read -r card_id title body labels <<<"$row"
    body="${body//\\n/$'\n'}"
    local found
    found="$(issue_lookup "$issues_json" "$card_id")"
    if [[ -n "$found" ]]; then
      ISSUE_SKIPPED+=("$card_id|$found")
      log "[issue] skip existing: $card_id -> $found"
      continue
    fi

    if (( DRY_RUN )); then
      ISSUE_CREATED+=("$card_id|dry-run|pending")
      log "[issue] dry-run create: $card_id"
      continue
    fi

    local created_raw created_url parsed number state
    created_raw="$(run_cmd gh issue create --repo "$REPO" --title "[$card_id] $title" --body "$body" --label "$labels")"
    created_url="$(CREATED_RAW="$created_raw" python3 - <<'PY'
import os
import re

raw = os.environ.get("CREATED_RAW", "")
for line in raw.splitlines()[::-1]:
    line = line.strip()
    if line.startswith("http://") or line.startswith("https://"):
        print(line)
        raise SystemExit(0)
match = re.search(r"https?://github\.com/\S+/issues/\d+", raw)
if match:
    print(match.group(0))
PY
)"
    if [[ -z "$created_url" ]]; then
      die "issue creation succeeded but URL could not be parsed for $card_id"
    fi
    number="${created_url##*/}"
    state="OPEN"
    parsed="${number}|${state}|${created_url}"
    ISSUE_CREATED+=("$card_id|$parsed")
    log "[issue] created: $card_id -> $parsed"
  done
}

has_read_project_scope() {
  local status_out
  if ! status_out="$(gh auth status -h github.com 2>&1)"; then
    return 1
  fi
  [[ "$status_out" == *"read:project"* ]]
}

resolve_board() {
  if ! has_read_project_scope; then
    BOARD_NOTE="SKIP: missing scope read:project; run gh auth refresh -s read:project,project"
    return 0
  fi

  local view_json=""
  if [[ -n "$PROJECT_NUMBER" ]]; then
    if ! view_json="$(run_cmd gh project view "$PROJECT_NUMBER" --owner "$PROJECT_OWNER" --format json 2>/dev/null)"; then
      BOARD_NOTE="SKIP: project #$PROJECT_NUMBER not accessible for owner $PROJECT_OWNER"
      return 0
    fi
  else
    local list_json
    if ! list_json="$(run_cmd gh project list --owner "$PROJECT_OWNER" --format json 2>/dev/null)"; then
      BOARD_NOTE="SKIP: unable to list projects for owner $PROJECT_OWNER"
      return 0
    fi
    local picked
    picked="$(PROJECT_LIST_JSON="$list_json" python3 - <<'PY'
import json
import os

rows = json.loads(os.environ["PROJECT_LIST_JSON"]).get("projects", [])
def find(sub):
    sub = sub.lower()
    for row in rows:
        title = (row.get("title") or "").lower()
        if sub in title:
            return row
    return None

row = find("rc execution board") or find("hardware/firmware")
if row:
    print(row.get("number", ""))
PY
)"
    if [[ -z "$picked" ]]; then
      BOARD_NOTE="SKIP: no matching project found; use --project-number <n>"
      return 0
    fi
    PROJECT_NUMBER="$picked"
    if ! view_json="$(run_cmd gh project view "$PROJECT_NUMBER" --owner "$PROJECT_OWNER" --format json 2>/dev/null)"; then
      BOARD_NOTE="SKIP: matched project #$PROJECT_NUMBER not readable"
      return 0
    fi
  fi

  BOARD_RESOLVED_TITLE="$(PROJECT_VIEW_JSON="$view_json" python3 - <<'PY'
import json
import os
data = json.loads(os.environ["PROJECT_VIEW_JSON"])
print(data.get("title", ""))
PY
)"

  BOARD_URL="$(PROJECT_VIEW_JSON="$view_json" python3 - <<'PY'
import json
import os
data = json.loads(os.environ["PROJECT_VIEW_JSON"])
print(data.get("url", "n/a"))
PY
)"
}

resolve_board_columns() {
  BOARD_COLUMNS=()
  if [[ "$BOARD_URL" == "n/a" || -z "$PROJECT_NUMBER" ]]; then
    BOARD_COLUMNS=("${DEFAULT_COLUMNS[@]}")
    return 0
  fi
  local fields_json
  if ! fields_json="$(run_cmd gh project field-list "$PROJECT_NUMBER" --owner "$PROJECT_OWNER" --format json 2>/dev/null)"; then
    BOARD_COLUMNS=("${DEFAULT_COLUMNS[@]}")
    return 0
  fi
  local cols
  cols="$(FIELDS_JSON="$fields_json" python3 - <<'PY'
import json
import os

payload = json.loads(os.environ["FIELDS_JSON"])
fields = payload.get("fields", [])
for field in fields:
    if (field.get("name") or "").lower() == "status":
        options = field.get("options") or []
        print("\n".join(opt.get("name", "") for opt in options if opt.get("name")))
        break
PY
)"
  if [[ -z "$cols" ]]; then
    BOARD_COLUMNS=("${DEFAULT_COLUMNS[@]}")
  else
    while IFS= read -r c; do
      [[ -n "$c" ]] && BOARD_COLUMNS+=("$c")
    done <<<"$cols"
  fi
}

resolve_board_status_counts() {
  BOARD_STATUS_COUNTS=""
  if [[ "$BOARD_URL" == "n/a" || -z "$PROJECT_NUMBER" ]]; then
    return 0
  fi
  local items_json
  if ! items_json="$(run_cmd gh project item-list "$PROJECT_NUMBER" --owner "$PROJECT_OWNER" --limit 200 --format json 2>/dev/null)"; then
    return 0
  fi
  BOARD_STATUS_COUNTS="$(ITEMS_JSON="$items_json" python3 - <<'PY'
import json
import os
import re
from collections import OrderedDict

payload = json.loads(os.environ["ITEMS_JSON"])
items = payload.get("items", [])
counts = OrderedDict()
pat = re.compile(r"^\[RCT-\d{2}\]\s")

def status_from_field_values(vals):
    for val in vals:
        field = val.get("field") or {}
        name = field.get("name") or ""
        if name.lower() != "status":
            continue
        if val.get("name"):
            return val["name"]
        option = val.get("option") or {}
        if option.get("name"):
            return option["name"]
    return "Unspecified"

for item in items:
    content = item.get("content") or {}
    title = content.get("title") or ""
    if not pat.match(title):
        continue
    status = status_from_field_values(item.get("fieldValues", []))
    counts[status] = counts.get(status, 0) + 1

if counts:
    print(", ".join(f"{k}={v}" for k, v in counts.items()))
PY
)"
}

print_plan_preview() {
  log "== Plan preview =="
  log "repo: $REPO"
  log "project owner: $PROJECT_OWNER"
  if [[ -n "$PROJECT_NUMBER" ]]; then
    log "project number: $PROJECT_NUMBER"
  else
    log "project number: auto-discovery"
  fi
  if (( DRY_RUN )); then
    log "mode: dry-run (no mutations)"
  else
    log "mode: apply (confirmation required unless --yes)"
  fi
  log "labels target: ${#LABEL_ROWS[@]}"
  log "issues target: ${#ISSUE_ROWS[@]} (RCT-01..RCT-10)"
}

print_board_summary() {
  log "== Board summary =="
  if [[ -n "$BOARD_NOTE" ]]; then
    log "$BOARD_NOTE"
  fi
  log "board url: $BOARD_URL"
  if [[ -n "$BOARD_RESOLVED_TITLE" ]]; then
    log "board title: $BOARD_RESOLVED_TITLE"
  fi
  if [[ ${#BOARD_COLUMNS[@]} -gt 0 ]]; then
    log "columns: ${BOARD_COLUMNS[*]}"
  fi
  if [[ -n "$BOARD_STATUS_COUNTS" ]]; then
    log "rct status counts: $BOARD_STATUS_COUNTS"
  fi
}

print_final_summary() {
  local issue_created_count="${#ISSUE_CREATED[@]}"
  local issue_skipped_count="${#ISSUE_SKIPPED[@]}"
  log "== Final summary =="
  log "repo: $REPO"
  log "board url: $BOARD_URL"
  log "labels created: ${#LABEL_CREATED[@]}"
  log "labels updated: ${#LABEL_UPDATED[@]}"
  log "labels unchanged: ${#LABEL_UNCHANGED[@]}"
  log "issues created: $issue_created_count"
  log "issues skipped existing: $issue_skipped_count"

  if (( issue_created_count > 0 )); then
    log "created issue refs:"
    printf '  - %s\n' "${ISSUE_CREATED[@]}"
  fi
  if (( issue_skipped_count > 0 )); then
    log "skipped issue refs:"
    printf '  - %s\n' "${ISSUE_SKIPPED[@]}"
  fi
}

main() {
  require_tools
  parse_args "$@"
  print_plan_preview
  confirm_apply

  log "== Execution: labels =="
  local labels_json
  labels_json="$(load_labels_json)"
  ensure_labels "$labels_json"

  log "== Execution: issues =="
  local issues_json
  issues_json="$(load_issue_index)"
  ensure_issues "$issues_json"

  resolve_board
  resolve_board_columns
  resolve_board_status_counts
  print_board_summary
  print_final_summary
}

main "$@"
