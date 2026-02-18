# Live Story V2 RC Runbook (S6)

Objectif: valider la release candidate Story V2 en conditions terrain (soak + resets croises + rollback).

## 1) Preflight

Depuis `hardware/firmware/esp32_audio`:

```bash
make story-validate
make story-gen
make qa-story-v2
```

## 2) Flash cartes

```bash
make upload-esp32 ESP32_PORT=<PORT_ESP32>
make uploadfs-esp32 ESP32_PORT=<PORT_ESP32>
make upload-screen SCREEN_PORT=<PORT_ESP8266>
```

## 3) RC matrix scriptée

```bash
make qa-story-v2-rc ESP32_PORT=<PORT_ESP32> SOAK_MINUTES=20 POLL_SECONDS=15 TRACE_LEVEL=INFO
```

Equivalent direct:

```bash
bash tools/qa/story_v2_rc_matrix.sh --esp32-port <PORT_ESP32> --soak-minutes 20 --poll-seconds 15 --trace-level INFO
```

Attendu:

- progression Story vers `STEP_DONE`
- `STORY_V2_HEALTH` stable (`OK` ou `BUSY` transitoire)
- pas de gel serie/ecran/audio
- logs monitoring disponibles: `SCREEN_LINK_STATUS`, `SYS_LOOP_BUDGET STATUS`, `MP3_*_STATUS`

## 4) Recovery reset croise (manuel)

1. Reset ESP8266 seul: verifier reprise ecran < 2 s.
2. Reset ESP32 seul: verifier reprise scenario/audio/ecran.
3. Couper/rebrancher liaison UART ecran: verifier pas de blocage ESP32.

## 5) Decision flag default

Critere `ON`:

- aucun bloquant critique
- scenario Story V2 stable en run complet
- soak sans freeze ni perte d'etape

Sinon, rollback compile-time:

1. remettre `config::kStoryV2EnabledDefault=false` dans `src/config.h`
2. recompiler et reflasher ESP32
3. verifier `STORY_V2_ENABLE STATUS`

## 6) Rapport final RC

Classer les anomalies:

- `Critique`: bloque release
- `Majeure`: fix avant release
- `Mineure`: planifie post-release
