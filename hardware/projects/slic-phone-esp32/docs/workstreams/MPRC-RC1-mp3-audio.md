# MPRC-RC1 — MP3/Audio Workstream

## Base
- Branch: `feature/MPRC-RC1-mp3-audio`
- Base branch: `codex/esp32-audio-mozzi-20260213`
- Owner scope:
  - `src/controllers/mp3/*`
  - `src/audio/mp3_player.*`
  - `src/audio/player/*`
  - `src/audio/catalog/*`
  - `src/services/serial/serial_commands_mp3.*`
  - ESP8266 MP3 only:
    - `../ui/esp8266_oled/src/apps/mp3_app.*`

## Shared File Rule
- `src/app/app_orchestrator.cpp` is shared with Story branch.
- Allowed changes here for this branch: MP3/Audio wiring only.
- Forbidden here for this branch: Story engine/state behavior.

## Delivery Sequence
1. **M1 (MPRC-301/302)**
- Complete ESP32 UI/UX NOW/BROWSE/QUEUE/SET via controller.
- Ensure keyboard/serial parity and feedback under 250ms.

2. **M2 (MPRC-303)**
- Move remaining MP3 OLED rendering from `../ui/esp8266_oled/src/main.cpp` into `mp3_app.cpp`.
- Keep `main.cpp` as orchestration only.

3. **M3 (MPRC-304/305)**
- Harden scan/catalog behavior under concurrent navigation and FX.
- Extend backend observability (`MP3_BACKEND_STATUS`, `MP3_CAPS`) with stable counters/reasons.

4. **M4 (MPRC-306)**
- Finalize QA/docs:
  - `tools/qa/mp3_rc_runbook.md`
  - `tools/qa/mp3_review_checklist.md`
  - `README.md`
  - `TESTING.md`
  - `../ui/esp8266_oled/README.md`

## PR Plan
- PR M1: UI/parity
- PR M2: ESP8266 mp3_app extraction
- PR M3: runtime/backend hardening
- PR M4: QA/docs convergence

## Action Board (Execution-Ready)
1. **Start M1 immediately**
- Create subtasks:
  - `MPRC-301A` now page data
  - `MPRC-301B` browse cursor/offset/count
  - `MPRC-301C` queue preview computation
  - `MPRC-302A` serial parity map
  - `MPRC-302B` keyboard parity map
- Commit pattern:
  - `feat(mp3): MPRC-301 <short scope>`
  - `feat(mp3): MPRC-302 <short scope>`

2. **M2 gating prep**
- Before moving OLED rendering:
  - locate and list remaining MP3 render entry points in `../ui/esp8266_oled/src/main.cpp`
  - move only MP3 rendering, keep link/boot orchestration untouched
- Commit pattern:
  - `refactor(screen): MPRC-303 move mp3 rendering into mp3_app`

3. **M3 reliability sequence**
- Implement in this order:
  1) scan budget cap per tick
  2) progress status enrichment
  3) backend counters and `last_reason`
- Commit pattern:
  - `feat(mp3): MPRC-304 scan budget + progress`
  - `feat(mp3): MPRC-305 backend observability`

4. **M4 closure**
- Add/refresh:
  - `tools/qa/mp3_rc_smoke.sh`
  - `tools/qa/mp3_rc_runbook.md`
  - `tools/qa/mp3_review_checklist.md`
- Final doc pass only after runtime/build checks are green.

## PR Commands (per milestone)
- Create draft PR:
  - `gh pr create --draft --base codex/esp32-audio-mozzi-20260213 --head feature/MPRC-RC1-mp3-audio --title "<TITLE>" --body-file <BODY.md>`
- Update PR checklist after each push:
  - `gh pr comment <PR_NUMBER> --body "<status update>"`
- Switch draft -> ready:
  - `gh pr ready <PR_NUMBER>`

## Mandatory Checks per PR
- `make story-validate`
- `make story-gen`
- `make qa-story-v2`
- `bash tools/qa/mp3_rc_smoke.sh`
- `pio run -e esp32dev`
- `pio run -e esp8266_oled`
- `pio run -e ui_rp2040_ili9488`
- `pio run -e ui_rp2040_ili9486`

## Acceptance Gates
- NOW/BROWSE/QUEUE/SET complete and consistent across serial, keyboard, OLED.
- No blocking behavior during scan + navigation + FX overlay.
- Backend fallback traceability stable (`attempts/retries/fallback/last_reason`).
- MP3 screen app remains stable through reset/relink.

## Definition of Done (MPRC Branch)
- `app_orchestrator` no longer carries heavy MP3 command logic (dispatch/wiring only).
- ESP8266 `main.cpp` keeps orchestration only for MP3 path.
- All MP3 command replies are canonical and context-safe.
- Final branch passes full matrix + MP3 smoke script before merge request.

## Notes
- Do not commit `reports/live_story_v2_smoke_*.log`.
- Canonical MP3 responses remain strict:
  - `OK/BAD_ARGS/OUT_OF_CONTEXT/NOT_FOUND/BUSY/UNKNOWN`
