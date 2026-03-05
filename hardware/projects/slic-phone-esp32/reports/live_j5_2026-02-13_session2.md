# Rapport Live USB J5/S6 — 2026-02-13 (session 2)

## Contexte

- Branche firmware active: `codex/esp32-audio-mozzi-20260213`
- Feature flag compile-time: `kStoryV2EnabledDefault=true`
- Ports detectes:
  - ESP32: `/dev/cu.SLAB_USBtoUART` (alias `/dev/cu.usbserial-0001`)
  - ESP8266 OLED: `/dev/cu.SLAB_USBtoUART7` (alias `/dev/cu.usbserial-6`)

## 1) Preflight

Commande:

```bash
make qa-story-v2
```

Resultat: `PASS`

- `story-validate` strict: OK
- `story-gen` strict + `spec_hash`: OK
- idempotence generation: OK
- builds: `esp32dev`, `esp8266_oled`, `screen:nodemcuv2` OK

## 2) Flash cartes

Commandes executees:

```bash
make upload-esp32 ESP32_PORT=/dev/cu.SLAB_USBtoUART
make uploadfs-esp32 ESP32_PORT=/dev/cu.SLAB_USBtoUART
make upload-screen SCREEN_PORT=/dev/cu.SLAB_USBtoUART7
```

Resultat: `PASS`

Note: un premier `upload-esp32` a echoue car `upload` et `uploadfs` avaient ete lances en parallele (port lock). Relance sequentielle ensuite: OK.

## 3) Runtime Story V2 (serie ESP32)

Log principal:

- `reports/live_j5_serial_20260213_171701.log`

Resultat: `PASS`

Preuves:

- validation scenario: `[STORY_V2] OK valid`
- progression:
  - `STEP_WAIT_UNLOCK -> STEP_WIN`
  - `STEP_WIN -> STEP_WAIT_ETAPE2`
  - `STEP_WAIT_ETAPE2 -> STEP_ETAPE2`
  - `STEP_ETAPE2 -> STEP_DONE`
- etat final:
  - `[STORY_V2] STATUS ... step=STEP_DONE ... gate=1`
- stress events:
  - `STORY_V2_EVENT ETAPE2_DUE` x20
  - `METRICS posted=25 accepted=25 rejected=0 storm_drop=0 queue_drop=0 transitions=5 max_queue=1`
- diagnostics:
  - `SYS_LOOP_BUDGET STATUS` repond
  - `SCREEN_LINK_STATUS` repond
  - `MP3_SCAN_PROGRESS` et `MP3_BACKEND_STATUS` repondent

## 4) Recovery ecran (ESP8266 reset)

Log recovery:

- `reports/live_recovery_screen_20260213_171940.log`

Test:

- reset/probe ESP8266 via esptool (`flash_id` + hard reset)
- verification `SCREEN_LINK_STATUS` avant/apres

Resultat: `PASS`

Preuves:

- `SCREEN_LINK_STATUS ... link_drop=0`
- `SCREEN_SYNC` continue apres reset
- pas de blocage ESP32 observe

## 5) Recovery ESP32

Test:

- reset ESP32 via esptool (`chip_id` + hard reset)
- verification serie: `STORY_V2_STATUS`, `SCREEN_LINK_STATUS`, `STORY_V2_HEALTH`

Resultat: `PASS`

Preuves:

- reboot complet puis reponses commandes OK
- `STORY_V2_STATUS ... step=STEP_WAIT_UNLOCK ... gate=1`
- `SCREEN_LINK_STATUS ... link_drop=0`

## 6) Anomalies triees

### Critique

- Aucune

### Majeure

- Aucune

### Mineure

1. `tx_drop` ecran eleve (`SCREEN_LINK_STATUS`) mais `link_drop=0` et link operationnel. A surveiller (charge UART + non-blocking drop policy).
2. Ouverture du port serie ESP32 via outil hote peut declencher un reset (DTR/RTS), attendu sur ce montage.

## Conclusion

- Validation live J5/S6 session 2: `PASS`
- Story V2 stable jusqu'a `STEP_DONE` avec gate MP3 ouvert.
- Recovery croise ESP32/ESP8266 confirme sans freeze observe.
