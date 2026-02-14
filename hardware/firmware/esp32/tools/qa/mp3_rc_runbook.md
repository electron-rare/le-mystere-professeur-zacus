# MP3 RC Runbook

## 1) Preflight

1. `pio device list`
2. `tools/qa/mp3_rc_smoke.sh`

## 2) Flash

1. ESP32:
- `make upload-esp32 ESP32_PORT=<PORT_ESP32>`
- `make uploadfs-esp32 ESP32_PORT=<PORT_ESP32>`

2. ESP8266 screen:
- `make upload-screen SCREEN_PORT=<PORT_ESP8266>`

## 3) Smoke runtime (ESP32 serial)

1. `MP3_STATUS`
2. `SD_STATUS`
3. `SD_RESCAN`
4. `SD_SCAN_PROGRESS`
5. `MP3_UI_STATUS`
6. `MP3_SCAN START`
7. `MP3_SCAN_PROGRESS`
8. `MP3_UI PAGE NOW`
9. `MP3_UI PAGE BROWSE`
10. `MP3_UI PAGE QUEUE`
11. `MP3_UI PAGE SET`
12. `MP3_QUEUE_PREVIEW 5`
13. `MP3_BACKEND STATUS`
14. `MP3_BACKEND_STATUS`
15. `MP3_CAPS`
16. `RADIO_STATUS`
17. `WIFI_STATUS`
18. `WEB_STATUS`

Expected:
- responses use canonical status (`OK/BAD_ARGS/OUT_OF_CONTEXT/NOT_FOUND/BUSY/UNKNOWN`)
- no freeze while scan is active
- page state (`NOW/BROWSE/QUEUE/SET`) visible in `MP3_UI_STATUS`

## 4) Screen recovery

1. Reset ESP8266 only
2. Validate screen resumes state within ~2s
3. Reset ESP32 only
4. Validate handshake and rendering recovery

## 5) Result

- PASS if all steps are green
- Otherwise list anomalies as `Critique`, `Majeure`, `Mineure`
