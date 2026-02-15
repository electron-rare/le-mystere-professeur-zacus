# RC Final Board - hardware/firmware

This document is the execution board source of truth for the `hardware/firmware` release candidate.

## Scope

- Target branch: `hardware/firmware`
- Cycle: 5 sprints, 2 days each
- Validation model: CI/script gate on every PR, live hardware gate only on sprint 5
- Firmware-only scope for this RC

## Squads

- `Squad A - Firmware Core`: `tools/dev/*`, runtime scripts, CLI contracts.
- `Squad B - UI/Protocol`: UI path consistency (`ui/esp8266_oled`, `ui/rp2040_tft`) and protocol docs alignment.
- `Squad C - QA Release`: smoke scripts, runbooks, RC evidence.
- `Squad D - Ops CI/Docs`: workflows, PR template, release governance docs.

## GitHub Project schema

### Columns

`Backlog` -> `Sprint Ready` -> `In Progress` -> `PR Review` -> `Validation` -> `Done`

### Labels

- `squad:a-core`
- `squad:b-ui`
- `squad:c-qa`
- `squad:d-ops`
- `gate:ci`
- `gate:live`
- `risk:blocker`
- `rc:final`

### Card fields

- `Owner`
- `Sprint`
- `PR #`
- `Base branch`
- `Commands run`
- `Evidence link`
- `Rollback note`

Rule: one card = one PR.

### Optional seeding script

Use `tools/dev/rc_board_seed.sh` to create labels and card issues from this plan.

## Sprint cards

| Card | Sprint | Squad | PR branch | Definition of done |
| --- | --- | --- | --- | --- |
| `RCF-01` | S1 | D | `pr/rcf-01-ci-path-alignment` | Workflow paths/working directories aligned with active firmware layout. |
| `RCF-02` | S1 | D | `pr/rcf-02-pr-template-alignment` | PR checklist aligned with current build matrix and smoke commands. |
| `RCF-03` | S2 | C | `pr/rcf-03-qa-script-path-migration` | Executable QA scripts contain no `screen_esp8266_hw630` references. |
| `RCF-04` | S3 | A | `pr/rcf-04-run-matrix-venv-hardening` | `run_matrix_and_smoke.sh` handles venv/skip/build/env controls reliably. |
| `RCF-05` | S3 | A | `pr/rcf-05-serial-smoke-contract-freeze` | `serial_smoke.py` options and role detection contract frozen/documented. |
| `RCF-07` | S3 | A | `pr/rcf-07-ports-map-single-source` | One `ports_map.json` source in `tools/dev`, duplicate removed. |
| `RCF-06` | S4 | B | `pr/rcf-06-vscode-cockpit-rc` | VS Code firmware tasks aligned with RC scripts. |
| `RCF-08` | S4 | C | `pr/rcf-08-qa-runbook-doc-sync` | RC docs/runbooks contain no obsolete command paths. |
| `RCF-09` | S5 | D | `pr/rcf-09-final-ci-gate` | Full CI gate replayed and evidence attached. |
| `RCF-10` | S5 | C | `pr/rcf-10-live-rc-evidence` | Live hardware gate executed and signed report produced. |
| `RCF-11` | S5 | D | `pr/rcf-11-rc-closeout` | Board/cards closed with final status and rollback note. |

## Mandatory CI gate commands

```bash
bash -n hardware/firmware/tools/dev/*.sh
python3 -m compileall hardware/firmware/tools/dev
source hardware/firmware/.venv/bin/activate && python3 hardware/firmware/tools/dev/serial_smoke.py --help
rg -n "screen_esp8266_hw630" hardware/firmware/esp32_audio/tools/qa
```

## RC final gates

```bash
cd hardware/firmware && ./build_all.sh
cd hardware/firmware && ZACUS_SKIP_PIO=1 ./tools/dev/run_matrix_and_smoke.sh
cd hardware/firmware && ZACUS_REQUIRE_HW=1 ./tools/dev/run_matrix_and_smoke.sh
```

## Board operation note

If `gh project` commands fail due missing `read:project` or `project` scope, keep this file as the authoritative board and map each row to a PR until token scopes are refreshed.
