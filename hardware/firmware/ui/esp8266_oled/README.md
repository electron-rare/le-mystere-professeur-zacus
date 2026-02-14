# Firmware écran ESP8266 (HW-630)
## Centralisation

La documentation des protocoles, templates et specs partagés est désormais centralisée dans `../../docs/protocols/`.


Ce firmware transforme une carte ESP8266 (HW-630) en ecran de statut pour l'ESP32.

## Structure logicielle (RC2)

Le code ecran est decoupe en modules pour faciliter l'evolution par app:

- `src/core/telemetry_state.h`: modele de telemetrie (`STAT`)
- `src/core/stat_parser.*`: parser `STAT` + validation CRC
- `src/core/link_monitor.*`: etat lien serie + watchdog/recovery
- `src/core/render_scheduler.*`: selection non bloquante de l'app ecran
- `src/apps/screen_app.h`: contrat `matches()/render()`
- `src/apps/boot_app.*`, `src/apps/link_app.*`, `src/apps/mp3_app.*`, `src/apps/ulock_app.*`
- `src/main.cpp`: orchestration setup/loop + rendu concret

Ce decoupage garde la compatibilite protocole tout en permettant d'ajouter de nouvelles apps ecran sans reouvrir un monolithe unique.

## Affichage

- `MODE U_LOCK` initial: module casse avec effet glitch (sans texte)
- `MODE U_LOCK` detection: volume micro + accordage + scope optionnel
- Pictogramme de validation lors du passage en module fonctionnel
- `MODULE U-SON FONCTIONNEL` (apres detection du LA)
- `MODE LECTEUR U-SON` (SD presente): vue lecteur MP3 enrichie (play/pause, piste, volume)
- Ecran explicite `LINK DOWN` si la telemetrie serie est perdue (avec anti-flicker)
- Uptime + derniere touche (`KEY`)

## Protocole UART

Trame texte envoyee par l'ESP32 (format etendu v2):

`STAT,<la>,<mp3>,<sd>,<uptime_ms>,<key>,<mode_mp3>,<track>,<track_count>,<volume_pct>,<u_lock>,<u_son_functional>,<tuning_offset>,<tuning_confidence>,<u_lock_listening>,<mic_level_pct>,<mic_scope>,<unlock_hold_pct>,<startup_stage>,<app_stage>,<seq>,<ui_page>,<repeat_mode>,<fx_active>,<backend_mode>,<scan_busy>,<error_code>,<ui_cursor>,<ui_offset>,<ui_count>,<queue_count>,<crc8_hex>\n`

Exemple:

`STAT,1,0,0,12345,2,0,0,0,0,1,0,-2,68,1,42,1,57,1,0,77,1,0,0,1,0,0,2,1,34,5,5A`

Compatibilite:
- le parser ecran accepte encore les trames `STAT` sans CRC (format legacy).
- si CRC present, la trame est validee et rejetee si checksum invalide.
- les champs UI MP3 (`ui_cursor/ui_offset/ui_count/queue_count`) sont optionnels et parses seulement s'ils sont presents.

## Cablage

- **Masse commune obligatoire**: ESP32 GND <-> ESP8266 GND
- ESP32 `GPIO22` (TX) -> ESP8266 `D6` (RX SoftwareSerial)
- ESP8266 `D5` (TX) non utilise (laisser deconnecte)
- Debit UART: `19200 bauds`
- OLED I2C sur ESP8266:
  - recommande: `D1` SDA, `D2` SCL
  - fallback supporte: `D2` SDA, `D1` SCL

## Build / Flash

Depuis `hardware/firmware/esp32` (racine commune):

1. `make build-screen`
2. `make upload-screen SCREEN_PORT=/dev/ttyUSB1`
3. `make monitor-screen SCREEN_PORT=/dev/ttyUSB1`

Alternative PlatformIO direct:

1. `pio run -e esp8266_oled`
2. `pio run -e esp8266_oled -t upload --upload-port /dev/ttyUSB1`
3. `pio device monitor -e esp8266_oled --port /dev/ttyUSB1`

Sinon, depuis ce dossier:

1. `pio run`
2. `pio run -t upload`
3. `pio device monitor`
