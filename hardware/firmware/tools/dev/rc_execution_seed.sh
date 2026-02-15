#!/usr/bin/env bash
set -euo pipefail

REPO="${1:-electron-rare/le-mystere-professeur-zacus}"

ensure_label() {
  local name="$1"
  local color="$2"
  local description="$3"
  if gh label view "$name" --repo "$REPO" >/dev/null 2>&1; then
    gh label edit "$name" --repo "$REPO" --color "$color" --description "$description" >/dev/null
  else
    gh label create "$name" --repo "$REPO" --color "$color" --description "$description" >/dev/null
  fi
}

ensure_issue() {
  local card_id="$1"
  local title="$2"
  local body="$3"
  local labels="$4"

  local existing
  existing="$(gh issue list --repo "$REPO" --search "\"$card_id\" in:title state:open" --json number --jq '.[0].number // empty')"
  if [[ -n "$existing" ]]; then
    echo "[skip] issue already exists for $card_id (#$existing)"
    return
  fi

  gh issue create --repo "$REPO" --title "[$card_id] $title" --body "$body" --label "$labels" >/dev/null
  echo "[ok] created issue for $card_id"
}

echo "[1/3] Ensuring RC execution labels..."
ensure_label "rc:test" "0052cc" "RC test execution cycle"
ensure_label "gate:cheap" "5319e7" "Cheap/local gate required"
ensure_label "gate:live" "b60205" "Live hardware gate required"
ensure_label "risk:blocker" "d93f0b" "RC blocker"
ensure_label "sprint:s1" "0e8a16" "Sprint 1"
ensure_label "sprint:s2" "1d76db" "Sprint 2"
ensure_label "sprint:s3" "a371f7" "Sprint 3"
ensure_label "sprint:s4" "fbca04" "Sprint 4"
ensure_label "sprint:s5" "c5def5" "Sprint 5"

echo "[2/3] Seeding RCT card issues..."
ensure_issue "RCT-01" "Baseline freeze and test inventory" $'Sprint: S1\nPR branch: pr/rct-01-baseline-freeze\nBase branch: hardware/firmware\nGate: cheap + live\nDoD: tooling inventory frozen and baseline evidence linked.' "rc:test,sprint:s1,gate:cheap,gate:live"
ensure_issue "RCT-02" "Content gate CI/local" $'Sprint: S1\nPR branch: pr/rct-02-content-gate\nBase branch: hardware/firmware\nGate: cheap\nDoD: content checks stable and reproducible in CI/local.' "rc:test,sprint:s1,gate:cheap"
ensure_issue "RCT-03" "MP3 suites hardening" $'Sprint: S2\nPR branch: pr/rct-03-mp3-suites-hardening\nBase branch: hardware/firmware\nGate: cheap\nDoD: mp3_basic/mp3_fx suites robust in no-hardware and live contexts.' "rc:test,sprint:s2,gate:cheap"
ensure_issue "RCT-04" "MP3 live evidence" $'Sprint: S2\nPR branch: pr/rct-04-mp3-live-evidence\nBase branch: hardware/firmware\nGate: live\nDoD: live evidence published for SD present and degraded mode.' "rc:test,sprint:s2,gate:live"
ensure_issue "RCT-05" "Story V2 basic stabilization" $'Sprint: S3\nPR branch: pr/rct-05-story-v2-basic\nBase branch: hardware/firmware\nGate: cheap\nDoD: story_v2_basic suite stable with expected tolerances.' "rc:test,sprint:s3,gate:cheap"
ensure_issue "RCT-06" "Story V2 metrics live evidence" $'Sprint: S3\nPR branch: pr/rct-06-story-v2-metrics-live\nBase branch: hardware/firmware\nGate: live\nDoD: metrics and reset observed and logged in live run.' "rc:test,sprint:s3,gate:live"
ensure_issue "RCT-07" "UI Link simulator live E2E" $'Sprint: S4\nPR branch: pr/rct-07-ui-link-sim-live\nBase branch: hardware/firmware\nGate: cheap + live\nDoD: HELLO/ACK/KEYFRAME/PONG and scripted BTN sequence validated.' "rc:test,sprint:s4,gate:cheap,gate:live"
ensure_issue "RCT-08" "Menu and mini-REPL live validation" $'Sprint: S4\nPR branch: pr/rct-08-menu-live\nBase branch: hardware/firmware\nGate: cheap + live\nDoD: curses and fallback flows validated with operator sanity checks.' "rc:test,sprint:s4,gate:cheap,gate:live"
ensure_issue "RCT-09" "Final RC gates" $'Sprint: S5\nPR branch: pr/rct-09-final-gates\nBase branch: hardware/firmware\nGate: cheap + build + live\nDoD: final gates replayed with exact command evidence and KO diagnostics.' "rc:test,sprint:s5,gate:cheap,gate:live,risk:blocker"
ensure_issue "RCT-10" "RC closeout report" $'Sprint: S5\nPR branch: pr/rct-10-closeout-report\nBase branch: hardware/firmware\nGate: documentation\nDoD: final report table complete and rollback note recorded.' "rc:test,sprint:s5,gate:cheap"

echo "[3/3] Done."
cat <<'TXT'
Next:
  1) Move RCT issues into board columns:
     Backlog -> Sprint Ready -> In Progress -> PR Review -> Live Validation -> Done
  2) If project commands fail, refresh GH scopes:
     gh auth refresh -s read:project,project
TXT
