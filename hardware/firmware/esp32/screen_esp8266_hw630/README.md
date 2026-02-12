# Firmware ecran ESP8266 (HW-630)

Ce firmware transforme une carte ESP8266 (HW-630) en ecran de statut pour l'ESP32.

## Affichage

- `MODE U_LOCK` initial: pictogramme casse + attente appui touche
- `MODE U_LOCK` detection: volume micro + accordage + scope optionnel
- Pictogramme de validation lors du passage en module fonctionnel
- `MODULE U-SON FONCTIONNEL` (apres detection du LA)
- `MODE LECTEUR U-SON` (SD presente): vue lecteur MP3 enrichie (play/pause, piste, volume)
- Ecran explicite `LINK DOWN` si la telemetrie serie est perdue
- Uptime + derniere touche (`KEY`)

## Protocole UART

Trame texte envoyee par l'ESP32 (format etendu):

`STAT,<la>,<mp3>,<sd>,<uptime_ms>,<key>,<mode_mp3>,<track>,<track_count>,<volume_pct>,<u_lock>,<u_son_functional>,<tuning_offset>,<tuning_confidence>,<u_lock_listening>,<mic_level_pct>,<mic_scope>\n`

Exemple:

`STAT,1,0,0,12345,2,0,0,0,0,1,0,-2,68,1,42,1`

Compatibilite:
- le firmware ecran reste compatible avec l'ancien format court `STAT` (les champs additionnels sont optionnels).

## Cablage

- **Masse commune obligatoire**: ESP32 GND <-> ESP8266 GND
- ESP32 `GPIO22` (TX) -> ESP8266 `D6` (RX SoftwareSerial)
- ESP8266 `D5` (TX) non utilise (laisser deconnecte)
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
