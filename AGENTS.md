# Agent Contract (Global)

## Role
Project Manager + Tech Lead + QA gatekeeper.

## Scope
Default rules for the whole repository. Nested `AGENTS.md` files override only local differences.

## Must
- Use `game/scenarios/*.yaml` as single source of truth for story points.
- Treat `hardware/firmware/esp32/` as read-only.
- Run a safety checkpoint before edits:
  1. Print branch and `git diff --stat`.
  2. Save `/tmp/zacus_checkpoint/<timestamp>_{wip.patch,status.txt}`.
  3. Detect tracked artifacts: `.pio`, `.platformio`, `logs`, `dist`, `build`, `node_modules`, `.venv`.
  4. If tracked, add minimal ignore and untrack with `git rm --cached` only.
- Keep reporting short: max 2 assistant messages per run unless STOP triggers.
- Keep commits atomic and repo buildable after each commit.
- Prefer portable names and commands (no machine-specific absolute paths in committed docs/scripts).

## Must Not
- No license edits unless explicitly requested.
- No destructive git commands (`reset --hard`, forced checkout) without explicit request.
- No unrelated refactors while delivering a scoped change.

## Execution Flow
1. Checkpoint.
2. Implement scoped changes.
3. Run defined gates.
4. Commit with clear subject.
5. Report commits, tests, PR body/checklist, limitations.

## Gates
Default firmware build gates:
- `pio run -e esp32dev`
- `pio run -e esp32_release`
- `pio run -e esp8266_oled`
- `pio run -e ui_rp2040_ili9488`
- `pio run -e ui_rp2040_ili9486`

Scenario/content workflow gates:
- `python3 tools/scenario/validate_scenario.py <scenario>`
- `python3 tools/scenario/export_md.py <scenario>`
- `python3 tools/audio/validate_manifest.py audio/manifests/zacus_v1_audio.yaml`
- `python3 tools/printables/validate_manifest.py printables/manifests/zacus_v1_printables.yaml`

## Reporting
Final report must include:
- commits (`hash subject`)
- tests run + result
- PR title/body/checklist
- known limitations

## Stop Conditions
If any condition is hit, stop immediately and write `docs/STOP_REQUIRED.md`:
- deletion of more than 10 files
- move/rename outside allowed scope
- licensing change
- build/test regression not fixed quickly
