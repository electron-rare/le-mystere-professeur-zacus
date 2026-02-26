# Codex Prompt — Zacus Scenario Bundle (Firmware + Narrative)

You are acting as an embedded firmware engineer working on the Freenove ESP32-S3 Zacus firmware.
Goal: integrate/update the scenario files so the story runtime matches the canonical scenario and remains stable.

## Bundle contents
- scenario_promptable_template.yaml  (human editable input)
- scenario_canonical.yaml            (canonical, to be copied into docs/protocols/... target)
- scenario_runtime.json              (runtime, to be copied into data/story/scenarios/DEFAULT.json)
- fsm_mermaid.md                     (visual FSM)
- zacus_v2.yaml                      (narrative scenario aligned with runtime)

## Repo targets to update
- docs/protocols/story_specs/scenarios/default_unlock_win_etape2.yaml
- data/story/scenarios/DEFAULT.json

## Narrative anchors (do not break)
- Prototype: U-SON
- Acte 1: LA 440 Hz (STEP_LA_DETECTOR)
- Acte 2: Zone 4 piano-alphabet → jouer "LEFOU" (letters placed randomly; A on LA 440 Hz)
- QR final: payload "WIN", hidden behind the portrait in the Archives
- After QR success: Media Hub + persistent boot into media_manager (persist_after_validation=true)

## Required work (must do)
1) Replace/update the target YAML file with `scenario_canonical.yaml`.
2) Replace/update `data/story/scenarios/DEFAULT.json` with `scenario_runtime.json`.
3) Ensure the runtime loader accepts:
   - step_id naming convention STEP_*
   - transitions encoded as {channel,event,target}
   - led_policy as a sibling of transitions (not nested)
4) Ensure QR alignment:
   - QR rule scene_id is `SCENE_QR_DETECTOR`
   - QR success emits `UNLOCK_QR` and is wired to existing unlock pipeline.
5) Fail-safe:
   - `ACTION_QR_TIMEOUT_30S` must fire `QR_TIMEOUT` event after ~30s (or closest existing timer infra).
   - Transition `event:QR_TIMEOUT->STEP_RTC_ESP_ETAPE2` must work.

## Acceptance checks
- No PANIC/ASSERT/ABORT/REBOOT in serial logs.
- Each scene transition produces an ACK ok=1 (or existing equivalent).
- Ownership/locking of camera / mic / amp is consistent:
  - SCENE_QR_DETECTOR uses camera
  - audio packs do not deadlock mic/amp
- WS2812 behaviour matches per-step led_policy.
- After QR validated once, persist_after_validation=true causes subsequent boots to start in media_manager.

## Deliverables
- Updated repo files (2 targets)
- Any minimal loader patches required
- Short changelog entry
- Evidence: build log + short serial trace demonstrating path:
  STEP_U_SON_PROTO -> ... -> STEP_QR_DETECTOR -> STEP_FINAL_WIN -> SCENE_MEDIA_MANAGER
