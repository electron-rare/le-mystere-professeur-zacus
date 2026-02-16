# Story V2 Release Handbook

Ce document formalise la release candidate Story V2 (S6) et les options de rollback.

## Scope release

- STORY V2 YAML + generation C++
- mini-apps FSM (LA detector, audio pack, screen scene, mp3 gate)
- diagnostics runtime/screen/mp3
- runbooks QA live

## Gate release (obligatoires)

1. Tooling:
   - `make story-validate`
   - `make story-gen`
   - `make qa-story-v2`
2. Build matrix:
   - `pio run -e esp32dev`
   - `pio run -e esp32_release`
   - `pio run -e esp8266_oled`
   - `pio run -e ui_rp2040_ili9488`
   - `pio run -e ui_rp2040_ili9486`
3. Live RC:
   - `esp32_audio/tools/qa/live_story_v2_rc_runbook.md`
   - soak >= 20 min
   - reset croise ESP32/ESP8266 valide

## Commandes de monitoring (RC)

- `STORY_V2_HEALTH`
- `STORY_V2_METRICS`
- `SYS_LOOP_BUDGET STATUS`
- `SCREEN_LINK_STATUS`
- `MP3_SCAN_PROGRESS`
- `MP3_BACKEND_STATUS`

## Decision compile-time (default ON/OFF)

Valeur courante:

- `config::kStoryV2EnabledDefault = true`

Decision finale:

1. Garder `true` si tous les gates release passent sans anomalie critique.
2. Revenir a `false` si instabilites terrain persistent.

## Rollback

Runtime immediat:

- `STORY_V2_ENABLE OFF`

Release durable:

1. modifier `src/config.h`:
   - `constexpr bool kStoryV2EnabledDefault = false;`
2. recompiler/reflasher ESP32
3. rejouer `STORY_V2_ENABLE STATUS` + tests smoke

## Artefacts de release

- rapport live RC
- checklist STV2-41..48 completee
- logs QA/CI (workflow `.github/workflows/firmware-ci.yml`)
