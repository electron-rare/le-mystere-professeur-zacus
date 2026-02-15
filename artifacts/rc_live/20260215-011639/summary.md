# RC live summary

- Result: **PASS**
- ESP32 port: ``
- ESP8266 port: ``

| Step | Status | Exit | Log | Details |
|---|---|---:|---|---|
| resolve_ports | PASS | 0 | `ports_resolve.json` | ok |
| build_esp32 | SKIP | 0 | `esp32_upload.log` | dry-run |
| build_esp8266 | SKIP | 0 | `esp8266_upload.log` | dry-run |
| upload_esp32 | SKIP | 0 | `esp32_upload.log` | dry-run |
| upload_esp8266 | SKIP | 0 | `esp8266_upload.log` | dry-run |
| smoke_esp32 | SKIP | 0 | `smoke_esp32.log` | dry-run |
| smoke_esp8266 | SKIP | 0 | `smoke_esp8266.log` | dry-run |
| gate_s1 | SKIP | 0 | `gate_s1.log` | dry-run |
| gate_s2 | SKIP | 0 | `gate_s2.log` | dry-run |

## UI link check
- OLED OK if boot screen transitions to active UI, refreshes, and reacts to buttons.
- ESP32 serial check: run `UI_LINK_STATUS` and confirm `connected=1` with low `rx_age`.
- Passive hint: look for `[SCREEN] oled=OK link=OK ack=1 ...` in ESP32 logs.
