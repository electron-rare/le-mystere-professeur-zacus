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
    - `../ui/esp8266_oled/src/apps/boot_app.*`
    - `../ui/esp8266_oled/src/apps/ulock_app.*`
    - `../ui/esp8266_oled/src/apps/link_app.*`
    - `../ui/esp8266_oled/src/core/*`

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
  - `../ui/esp8266_oled/README.md`
- Prepare Story convergence PR with QA evidence.

## PR Plan
- PR S1: LA runtime + app ownership
- PR S2: StoryGen config + validation
- PR S3: ESP8266 non-MP3 modular split
- PR S4: Docs + QA + convergence

## Action Board (Execution-Ready)
1. **Start S1 immediately**
- Implement in this order:
  1) `LaDetectorRuntimeService` invariants/snapshot
  2) `LaDetectorApp` hold/unlock lifecycle
  3) anti-dup unlock emission
  4) wiring in `StoryControllerV2`
- Commit pattern:
  - `feat(story): STV2-301 la runtime service`
  - `feat(story): STV2-302 la detector app full-owned`

2. **S2 StoryGen extension**
- Extend schema/template first, then generator, then runtime validation.
- Add strict validation errors for unknown/missing config keys.
- Commit pattern:
  - `feat(story-gen): STV2-303 app binding config for LA`
  - `feat(story): STV2-304 validate generated LA config`

3. **S3 ESP8266 non-MP3 modularization**
- Move in this order:
  1) `core/stat_parser.*`
  2) `core/link_monitor.*`
  3) `core/render_scheduler.*`
  4) app selector path for `boot/ulock/link`
- Keep `main.cpp` as orchestrator only.
- Commit pattern:
  - `refactor(screen): STV2-305 split non-mp3 core`
  - `refactor(screen): STV2-306 modular non-mp3 apps`

4. **S4 closure**
- Run full Story checks and consolidate docs with exact runtime behavior.
- Prepare convergence PR summary with evidence commands and outputs.

## PR Commands (per milestone)
- Create draft PR:
  - `gh pr create --draft --base codex/esp32-audio-mozzi-20260213 --head feature/STV2-RC3-story-app-modular --title "<TITLE>" --body-file <BODY.md>`
- Post status/evidence updates:
  - `gh pr comment <PR_NUMBER> --body "<status update>"`
- Switch draft -> ready:
  - `gh pr ready <PR_NUMBER>`

## Mandatory Checks per PR
- `make story-validate`
- `make story-gen`
- `bash tools/qa/story_v2_ci.sh`
- `pio run -e esp32dev`
- `pio run -e esp8266_oled`
- `pio run -e ui_rp2040_ili9488`
- `pio run -e ui_rp2040_ili9486`

## Acceptance Gates
- Story flow stable to `STEP_DONE` with coherent MP3 gate.
- LA detector fully controlled via Story app runtime.
- No duplicate UNLOCK under event storm.
- ESP8266 non-MP3 recovers after reset in <2s.

## Definition of Done (STV2 Branch)
- `LA_detector` is fully app-owned in Story V2 path.
- StoryGen supports `app_bindings[].config` for LA with strict validation.
- ESP8266 non-MP3 flow is modularized and `main.cpp` stays orchestration-only.
- Full Story matrix passes before requesting merge.

## Notes
- Do not commit `reports/live_story_v2_smoke_*.log`.
- Keep responses canonical in Story serial API:
  - `OK/BAD_ARGS/OUT_OF_CONTEXT/NOT_FOUND/BUSY/UNKNOWN`
