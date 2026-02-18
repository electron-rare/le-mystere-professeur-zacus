# Checklist Review Radio RC V3

## Architecture
- [ ] `WifiService` isolé.
- [ ] `RadioService` isolé.
- [ ] `WebUiService` isolé.
- [ ] `RadioRuntime` tâches FreeRTOS créées.

## Série
- [ ] `RADIO_*` routé via `serial_commands_radio`.
- [ ] `WIFI_*` routé via `serial_commands_radio`.
- [ ] `WEB_STATUS` routé.

## Compat
- [ ] `MP3_*` inchangé.
- [ ] `SD_*` inchangé.
- [ ] `STORY_V2_*` inchangé.

## Tests
- [ ] `pio run -e esp32dev`.
- [ ] `pio run -e esp32_release`.
- [ ] `pio run -e esp8266_oled`.
- [ ] `pio run -e ui_rp2040_ili9488`.
- [ ] `pio run -e ui_rp2040_ili9486`.
- [ ] `bash tools/qa/radio_rc_smoke.sh`.
