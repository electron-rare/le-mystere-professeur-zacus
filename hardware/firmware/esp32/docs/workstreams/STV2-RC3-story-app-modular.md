# STV2-RC3 — Story/App Modular Workstream

## Base
- Branch: `feature/STV2-RC3-story-app-modular`
- Base branch: `codex/esp32-audio-mozzi-20260213`
- Owner scope:
  - `src/controllers/story/*`
  - `src/story/*`
  - `story_specs/*`
  - `src/services/la/*`
  - `src/services/serial/serial_commands_story.*`
  - ESP8266 non-MP3:
    - `screen_esp8266_hw630/src/apps/boot_app.*`
    - `screen_esp8266_hw630/src/apps/ulock_app.*`
    - `screen_esp8266_hw630/src/apps/link_app.*`
    - `screen_esp8266_hw630/src/core/*`

## Shared File Rule
- `src/app/app_orchestrator.cpp` is shared with MP3 branch.
- Allowed changes here for this branch: Story/App wiring only.
- Forbidden here for this branch: MP3 command behavior, MP3 UI internals.

## Delivery Sequence
1. **S1 (STV2-301/302)**
- Finalize `LA_detector` as full Story app owner.
- Align `StoryAppContext` with `LaDetectorRuntimeService`.
- Ensure single UNLOCK emission per hold cycle.

2. **S2 (STV2-303/304)**
- Extend StoryGen schema for `app_bindings[].config`.
- Add LA config support:
  - `hold_ms`
  - `unlock_event`
  - `require_listening`
- Harden `STORY_V2_VALIDATE` to include generated LA config.

3. **S3 (STV2-305/306)**
- Complete ESP8266 non-MP3 modular core and app selection.
- Keep parser `STAT` backward-compatible (`seq` + CRC path unchanged).

4. **S4 (STV2-307/308)**
- Harmonize docs:
  - `src/story/README.md`
  - `README.md`
  - `TESTING.md`
  - `screen_esp8266_hw630/README.md`
- Prepare Story convergence PR with QA evidence.

## PR Plan
- PR S1: LA runtime + app ownership
- PR S2: StoryGen config + validation
- PR S3: ESP8266 non-MP3 modular split
- PR S4: Docs + QA + convergence

## Mandatory Checks per PR
- `make story-validate`
- `make story-gen`
- `bash tools/qa/story_v2_ci.sh`
- `pio run -e esp32dev`
- `pio run -e esp8266_oled`
- `cd screen_esp8266_hw630 && pio run -e nodemcuv2`

## Acceptance Gates
- Story flow stable to `STEP_DONE` with coherent MP3 gate.
- LA detector fully controlled via Story app runtime.
- No duplicate UNLOCK under event storm.
- ESP8266 non-MP3 recovers after reset in <2s.

## Notes
- Do not commit `reports/live_story_v2_smoke_*.log`.
- Keep responses canonical in Story serial API:
  - `OK/BAD_ARGS/OUT_OF_CONTEXT/NOT_FOUND/BUSY/UNKNOWN`
