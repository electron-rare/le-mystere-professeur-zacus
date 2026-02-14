# Protocole UART JSONL (UI <-> ESP32)

Ce document est la source de vérité du protocole UART entre UI et ESP32.

## Transport
- UART texte UTF-8, une ligne JSON par message (`\n`).
- Débit recommandé: `115200`.
- Longueur maximale de ligne: `512` octets.
- Les clés inconnues sont ignorées.
- Les champs absents sont tolérés (compatibilité additive).

## UI -> ESP32 (`t=cmd`)

Format canonique:
```json
{"t":"cmd","a":"<action>","v":<optionnel>}
```

Actions supportées:
```json
{"t":"cmd","a":"play_pause"}
{"t":"cmd","a":"next"}
{"t":"cmd","a":"prev"}
{"t":"cmd","a":"vol_delta","v":1}
{"t":"cmd","a":"vol_set","v":42}
{"t":"cmd","a":"source_set","v":"radio"}
{"t":"cmd","a":"seek","v":120}
{"t":"cmd","a":"station_delta","v":1}
{"t":"cmd","a":"request_state"}
```

## ESP32 -> UI

### `state`
```json
{"t":"state","playing":true,"source":"radio","title":"...","artist":"","station":"Station A","pos":12,"dur":0,"vol":42,"rssi":-61,"buffer":87,"error":""}
```

### `tick`
Recommandé à 5 Hz.
```json
{"t":"tick","pos":13,"buffer":86,"vu":0.42}
```

### `hb`
Heartbeat recommandé à 1 Hz.
```json
{"t":"hb","ms":12345678}
```

### `list`
Extension additive pour navigation catalogue:
```json
{"t":"list","source":"sd","offset":12,"total":75,"cursor":14,"items":["Track A","Track B","Track C","Track D"]}
```

## Politique de reconnexion UI
1. Si aucun `hb` reçu pendant `3s`, l'UI passe en état offline.
2. Tant que le lien est offline, l'UI envoie `request_state` toutes les `1s`.

## Commandes série canonique (debug/ops)

Le moniteur série ESP32 utilise des tokens texte (`PREFIX_ACTION`) routés vers des handlers dédiés.

### Matrice de traçabilité

| Domaine | Préfixe | Handler principal |
|---|---|---|
| Boot/audio boot | `BOOT_*` | `esp32/src/app/app_orchestrator.cpp` |
| Codec audio | `CODEC_*` | `esp32/src/app/app_orchestrator.cpp` |
| Clavier analogique | `KEY_*` | `esp32/src/app/app_orchestrator.cpp` |
| Story legacy + V2 | `STORY_*` | `esp32/src/services/serial/serial_commands_story.cpp` |
| MP3/UI player | `MP3_*` | `esp32/src/services/serial/serial_commands_mp3.cpp` |
| Radio/Wi-Fi/Web | `RADIO_*`, `WIFI_*`, `WEB_*` | `esp32/src/services/serial/serial_commands_radio.cpp` |
| Système runtime | `SYS_*`, `SCREEN_LINK_*` | `esp32/src/app/app_orchestrator.cpp` |

### Tokens canoniques exposés

- `BOOT_*`: `BOOT_STATUS`, `BOOT_HELP`, `BOOT_NEXT`, `BOOT_REPLAY`, `BOOT_REOPEN`, `BOOT_TEST_TONE`, `BOOT_TEST_DIAG`, `BOOT_PA_ON`, `BOOT_PA_OFF`, `BOOT_PA_STATUS`, `BOOT_PA_INV`, `BOOT_FS_INFO`, `BOOT_FS_LIST`, `BOOT_FS_TEST`, `BOOT_FX_FM`, `BOOT_FX_SONAR`, `BOOT_FX_MORSE`, `BOOT_FX_WIN`
- `STORY_*`: `STORY_STATUS`, `STORY_HELP`, `STORY_RESET`, `STORY_ARM`, `STORY_FORCE_ETAPE2`, `STORY_TEST_ON`, `STORY_TEST_OFF`, `STORY_TEST_DELAY`, `STORY_V2_ENABLE`, `STORY_V2_STATUS`, `STORY_V2_LIST`, `STORY_V2_VALIDATE`, `STORY_V2_HEALTH`, `STORY_V2_TRACE`, `STORY_V2_TRACE_LEVEL`, `STORY_V2_METRICS`, `STORY_V2_METRICS_RESET`, `STORY_V2_EVENT`, `STORY_V2_STEP`, `STORY_V2_SCENARIO`
- `MP3_*`: `MP3_HELP`, `MP3_STATUS`, `MP3_UNLOCK`, `MP3_REFRESH`, `MP3_LIST`, `MP3_NEXT`, `MP3_PREV`, `MP3_RESTART`, `MP3_PLAY`, `MP3_FX_MODE`, `MP3_FX_GAIN`, `MP3_FX`, `MP3_FX_STOP`, `MP3_TEST_START`, `MP3_TEST_STOP`, `MP3_BACKEND`, `MP3_BACKEND_STATUS`, `MP3_SCAN`, `MP3_SCAN_PROGRESS`, `MP3_BROWSE`, `MP3_PLAY_PATH`, `MP3_UI`, `MP3_UI_STATUS`, `MP3_QUEUE_PREVIEW`, `MP3_CAPS`, `MP3_STATE`
- `KEY_*`: `KEY_HELP`, `KEY_STATUS`, `KEY_RAW_ON`, `KEY_RAW_OFF`, `KEY_RESET`, `KEY_SET`, `KEY_SET_ALL`, `KEY_TEST_START`, `KEY_TEST_STATUS`, `KEY_TEST_RESET`, `KEY_TEST_STOP`
- `CODEC_*`: `CODEC_HELP`, `CODEC_STATUS`, `CODEC_DUMP`, `CODEC_RD`, `CODEC_WR`, `CODEC_VOL`, `CODEC_VOL_RAW`
- `SYS_*`: `SYS_LOOP_BUDGET`
- `SCREEN_LINK_*`: `SCREEN_LINK_STATUS`, `SCREEN_LINK_RESET_STATS`
- `RADIO/WIFI/WEB`: `RADIO_HELP`, `RADIO_STATUS`, `RADIO_LIST`, `RADIO_PLAY`, `RADIO_STOP`, `RADIO_NEXT`, `RADIO_PREV`, `RADIO_META`, `WIFI_STATUS`, `WIFI_SCAN`, `WIFI_CONNECT`, `WIFI_AP_ON`, `WIFI_AP_OFF`, `WEB_STATUS`

## Compatibilité et versioning

- Politique par défaut: compatibilité additive.
- Interdit: suppression brutale d'un champ JSON sans dépréciation.
- Dépréciation:
  - documenter le changement dans ce fichier,
  - conserver l'ancien champ/token pendant au moins un cycle release.
- Les alias non canoniques ne doivent pas être ajoutés; conserver `PREFIX_ACTION`.
