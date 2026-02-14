# Runbook RC Radio V3

## Préflight
1. `pio device list`
2. Vérifier ESP32 + ESP8266 détectés.
3. Si une carte manque, afficher:
`Cartes USB non détectées. Merci de brancher/rebrancher ESP32 + ESP8266 puis confirmer.`

## Build + Flash
1. `bash tools/qa/radio_rc_smoke.sh`
2. `make upload-esp32 ESP32_PORT=<PORT_ESP32>`
3. `make uploadfs-esp32 ESP32_PORT=<PORT_ESP32>`
4. `make upload-screen SCREEN_PORT=<PORT_ESP8266>`

## Série ESP32
1. `RADIO_STATUS`
2. `RADIO_LIST 0 5`
3. `WIFI_STATUS`
4. `WIFI_AP_ON`
5. `RADIO_PLAY 1`
6. `RADIO_META`
7. `WEB_STATUS`
8. `RADIO_STOP`

## Critères PASS
1. Réponses canoniques `OK/BAD_ARGS/OUT_OF_CONTEXT/NOT_FOUND/BUSY/UNKNOWN`.
2. Aucune régression commandes MP3/SD.
3. Pas de freeze pendant navigation UI.
