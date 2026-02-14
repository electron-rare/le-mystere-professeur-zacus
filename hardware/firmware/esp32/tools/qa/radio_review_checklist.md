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
- [ ] `cd screen_esp8266_hw630 && pio run -e nodemcuv2`.
- [ ] `bash tools/qa/radio_rc_smoke.sh`.
