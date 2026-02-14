# RC V3 MP3/Radio - Plan Technique Exécutable

## Objectif
Livrer une stack unifiée SD MP3 + Radio Web + WiFi + pilotage Web, sans régression Story/OLED.

## Inspirations retenues
- yOradio: séparation config/stations + contrôle web + workflow terrain.
- ESP32-Radio-V2: architecture multi-tâches, séparation pipeline stream/audio.

## Principes d'architecture
- Un seul propriétaire audio I2S (audio engine).
- Tâches FreeRTOS spécialisées avec files de commandes/événements.
- Sérial canonique conservé (`MP3_*`, `SD_*`) et extension (`RADIO_*`, `WIFI_*`, `WEB_STATUS`).
- Fallback stable: stream indisponible -> source SD disponible.

## Roadmap PR incrémentale
1. PR V3-1 (fait): services `wifi/radio/web`, runtime radio, commandes série radio, runbook QA.
2. PR V3-2: pipeline stream robuste (reconnect exponentiel, timeout, metadata ICY).
3. PR V3-3: API Web UI (`/api/status`, `/api/radio`, `/api/wifi`, `/api/player`) + auth simple.
4. PR V3-4: unification UI OLED MP3/Radio (`LECTURE/BROWSER/QUEUE/REGLAGES`).
5. PR V3-5: hardening mémoire/perf (buffers adaptatifs, compteurs OOM/retry/fallback).
6. PR V3-6: smoke live final + dossier démo client + sync `codex -> main`.

## Tests obligatoires par PR
- `pio run -e esp32dev`
- `pio run -e esp32_release`
- `pio run -e esp8266_oled`
- `cd screen_esp8266_hw630 && pio run -e nodemcuv2`
- `tools/qa/radio_rc_smoke.sh`

## KPIs release
- Action->feedback < 250 ms.
- Reprise écran après reset croisé < 2 s.
- Aucune commande canonique cassée.
- Démo 12 min rejouable 2 fois.
