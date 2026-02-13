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
    - `screen_esp8266_hw630/src/apps/mp3_app.*`

## Shared File Rule
- `src/app/app_orchestrator.cpp` is shared with Story branch.
- Allowed changes here for this branch: MP3/Audio wiring only.
- Forbidden here for this branch: Story engine/state behavior.

## Delivery Sequence
1. **M1 (MPRC-301/302)**
- Complete ESP32 UI/UX NOW/BROWSE/QUEUE/SET via controller.
- Ensure keyboard/serial parity and feedback under 250ms.

2. **M2 (MPRC-303)**
- Move remaining MP3 OLED rendering from `screen_esp8266_hw630/src/main.cpp` into `mp3_app.cpp`.
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
  - `screen_esp8266_hw630/README.md`

## PR Plan
- PR M1: UI/parity
- PR M2: ESP8266 mp3_app extraction
- PR M3: runtime/backend hardening
- PR M4: QA/docs convergence

## Mandatory Checks per PR
- `make story-validate`
- `make story-gen`
- `make qa-story-v2`
- `bash tools/qa/mp3_rc_smoke.sh`
- `pio run -e esp32dev`
- `pio run -e esp8266_oled`
- `cd screen_esp8266_hw630 && pio run -e nodemcuv2`

## Acceptance Gates
- NOW/BROWSE/QUEUE/SET complete and consistent across serial, keyboard, OLED.
- No blocking behavior during scan + navigation + FX overlay.
- Backend fallback traceability stable (`attempts/retries/fallback/last_reason`).
- MP3 screen app remains stable through reset/relink.

## Notes
- Do not commit `reports/live_story_v2_smoke_*.log`.
- Canonical MP3 responses remain strict:
  - `OK/BAD_ARGS/OUT_OF_CONTEXT/NOT_FOUND/BUSY/UNKNOWN`
