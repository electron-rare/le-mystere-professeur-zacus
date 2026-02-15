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

echo "[1/3] Ensuring labels..."
ensure_label "squad:a-core" "0e8a16" "Firmware Core squad"
ensure_label "squad:b-ui" "1d76db" "UI/Protocol squad"
ensure_label "squad:c-qa" "a371f7" "QA Release squad"
ensure_label "squad:d-ops" "fbca04" "Ops CI/Docs squad"
ensure_label "gate:ci" "5319e7" "CI gate required"
ensure_label "gate:live" "b60205" "Live hardware gate required"
ensure_label "risk:blocker" "d93f0b" "Release blocker"
ensure_label "rc:final" "0052cc" "RC final cycle"

echo "[2/3] Seeding RC card issues..."
ensure_issue "RCF-01" "CI path alignment" $'Sprint: S1\nSquad: D\nPR branch: pr/rcf-01-ci-path-alignment\nBase: hardware/firmware\nDoD: workflow paths and working directories aligned with active layout.' "rc:final,squad:d-ops,gate:ci"
ensure_issue "RCF-02" "PR template alignment" $'Sprint: S1\nSquad: D\nPR branch: pr/rcf-02-pr-template-alignment\nBase: hardware/firmware\nDoD: PR checklist aligned with current build matrix and smoke workflow.' "rc:final,squad:d-ops,gate:ci"
ensure_issue "RCF-03" "QA script path migration" $'Sprint: S2\nSquad: C\nPR branch: pr/rcf-03-qa-script-path-migration\nBase: hardware/firmware\nDoD: no executable QA script uses legacy screen_esp8266_hw630 path.' "rc:final,squad:c-qa,gate:ci"
ensure_issue "RCF-04" "Matrix/smoke venv hardening" $'Sprint: S3\nSquad: A\nPR branch: pr/rcf-04-run-matrix-venv-hardening\nBase: hardware/firmware\nDoD: run_matrix_and_smoke supports skip/env controls and stable exit codes.' "rc:final,squad:a-core,gate:ci"
ensure_issue "RCF-05" "Serial smoke contract freeze" $'Sprint: S3\nSquad: A\nPR branch: pr/rcf-05-serial-smoke-contract-freeze\nBase: hardware/firmware\nDoD: serial_smoke options and role detection contract documented and stable.' "rc:final,squad:a-core,gate:ci"
ensure_issue "RCF-06" "VS Code cockpit RC alignment" $'Sprint: S4\nSquad: B\nPR branch: pr/rcf-06-vscode-cockpit-rc\nBase: hardware/firmware\nDoD: tasks align with final RC scripts and commands.' "rc:final,squad:b-ui,gate:ci"
ensure_issue "RCF-07" "Ports map single source" $'Sprint: S3\nSquad: A\nPR branch: pr/rcf-07-ports-map-single-source\nBase: hardware/firmware\nDoD: one ports_map source under tools/dev and duplicate removed.' "rc:final,squad:a-core,gate:ci"
ensure_issue "RCF-08" "Runbook/doc sync" $'Sprint: S4\nSquad: C\nPR branch: pr/rcf-08-qa-runbook-doc-sync\nBase: hardware/firmware\nDoD: RC docs contain no obsolete command paths.' "rc:final,squad:c-qa,gate:ci"
ensure_issue "RCF-09" "Final CI gate replay" $'Sprint: S5\nSquad: D\nPR branch: pr/rcf-09-final-ci-gate\nBase: hardware/firmware\nDoD: final CI gate evidence attached.' "rc:final,squad:d-ops,gate:ci"
ensure_issue "RCF-10" "Live RC evidence" $'Sprint: S5\nSquad: C\nPR branch: pr/rcf-10-live-rc-evidence\nBase: hardware/firmware\nDoD: live runbook replayed, signed RC report attached.' "rc:final,squad:c-qa,gate:live"
ensure_issue "RCF-11" "RC closeout" $'Sprint: S5\nSquad: D\nPR branch: pr/rcf-11-rc-closeout\nBase: hardware/firmware\nDoD: board closure and rollback notes completed.' "rc:final,squad:d-ops,gate:ci"

cat <<'EOF'
[3/3] Done.
Next:
  1) Open GitHub Issues and move RCF cards into your project board columns.
  2) If gh project fails with scope errors, refresh auth scopes:
       gh auth refresh -s read:project,project
EOF
