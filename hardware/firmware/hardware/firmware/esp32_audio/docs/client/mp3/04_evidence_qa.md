# Evidence QA - Presentation client MP3

## Snapshot execution

- Date run: `2026-02-13 23:13 CET`
- Branche: `feature/MP3-client-demo-pack-v1`
- Base: `codex/esp32-audio-mozzi-20260213`
- Story spec hash: `4146e7c78895`

## Build / tooling (statique)

Resultats a maintenir a jour avant chaque presentation:

- `make story-validate` -> PASS
- `make story-gen` -> PASS
- `make qa-story-v2` -> PASS
- `pio run -e esp32dev` -> PASS
- `pio run -e esp32_release` -> PASS
- `pio run -e esp8266_oled` -> PASS
- `pio run -e ui_rp2040_ili9488` -> PASS
- `pio run -e ui_rp2040_ili9486` -> PASS

## Smoke MP3 client

- `tools/qa/mp3_rc_smoke.sh` -> PASS
- `tools/qa/mp3_client_demo_smoke.sh` -> PASS

Preuve smoke (extraits):

- `[mp3-rc-smoke] static checks OK`
- `[mp3-client-demo] PASS`

## Verifications serie attendues

1. `MP3_STATUS` -> etat SD/pistes coherent
2. `MP3_UI_STATUS` -> page/cursor/offset coherents
3. `MP3_SCAN_PROGRESS` -> scan evolutif sans gel
4. `MP3_BACKEND_STATUS` -> fallback + compteurs lisibles
5. `MP3_CAPS` -> matrice runtime backend/codec
6. `MP3_QUEUE_PREVIEW 5` -> preview stable

## Verifications live (terrain)

1. UX 4 pages NOW/BROWSE/QUEUE/SET
2. scan actif + navigation sans blocage
3. reset croise ESP32/ESP8266 et recovery ecran

## Anomalies (a remplir en repetition)

## Critique

- aucune

## Majeure

- aucune

## Mineure

- warnings toolchain tiers (deprecation/elf2bin) sans impact runtime
