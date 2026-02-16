# Story V2 Pipeline (YAML -> Gen -> Runtime -> Apps -> Screen)

This document is the single, linear view of the Story V2 pipeline, from authoring to on-device behavior.

## 1) Authoring (source of truth)

- YAML scenarios live in docs/protocols/story_specs/scenarios
- Schema lives in docs/protocols/story_specs/schema/story_spec_v1.yaml
- Template lives in docs/protocols/story_specs/templates/scenario.template.yaml

## 2) Validation and generation

- Generator: esp32_audio/tools/story_gen/story_gen.py
- Commands (from hardware/firmware/esp32_audio):
  - make story-validate
  - make story-gen

Outputs (generated C++):
- esp32_audio/src/story/generated/scenarios_gen.h
- esp32_audio/src/story/generated/scenarios_gen.cpp
- esp32_audio/src/story/generated/apps_gen.h
- esp32_audio/src/story/generated/apps_gen.cpp

## 3) Runtime loading

- StoryControllerV2 loads a ScenarioDef from generated data and starts the engine.
- StoryEngineV2 runs a deterministic event-driven state machine.

Key files:
- esp32_audio/src/controllers/story/story_controller_v2.cpp
- esp32_audio/src/story/core/story_engine_v2.cpp

## 4) Apps and actions

- StoryAppHost starts apps bound to the current step and applies step actions.
- Apps produce hardware effects and may emit events back to the engine.

Apps:
- LaDetectorApp (unlock event)
- AudioPackApp (audio_done event)
- ScreenSceneApp (screen scene updates)
- Mp3GateApp (mp3 gate open/close)

Actions:
- ACTION_TRACE_STEP
- ACTION_QUEUE_SONAR
- ACTION_REFRESH_SD

Key files:
- esp32_audio/src/story/apps/story_app_host.cpp
- esp32_audio/src/story/resources/action_registry.cpp

## 5) Screen pipeline (YAML -> UI)

YAML declares screen_scene_id, which flows through the screen app and the screen link to the UI firmware.

Field mapping (logical):

| YAML field | Runtime app | Screen frame | UI Link v2 fields | UI spec |
|---|---|---|---|---|
| screen_scene_id | ScreenSceneApp | ScreenFrame scene_id + ui_page + app | KEYFRAME/STAT app + ui_page | UI_SPEC pages / scene mapping |

Key files:
- esp32_audio/src/services/screen/screen_sync_service.cpp
- protocol/ui_link_v2.h
- docs/protocols/UI_SPEC.md
- ui/esp8266_oled
- ui/rp2040_tft

## 6) Events and triggers

Events can come from:
- LA detector (unlock)
- Audio completion (audio_done)
- Timers (ETAPE2_DUE)
- Serial injection (STORY_V2_EVENT <name>)

## 7) Debug and validation

- STORY_V2_LIST, STORY_V2_VALIDATE, STORY_V2_STATUS
- STORY_V2_EVENT <name> for manual transitions

See docs/protocols/PROTOCOL.md for serial commands.
