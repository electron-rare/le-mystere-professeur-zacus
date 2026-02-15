# Agent Contract (hardware/firmware)

## Role
Firmware PM/TL/QA contract for PlatformIO and hardware validation.

## Scope
Applies to `hardware/firmware/**` and overrides root defaults where specified.

## Must
- Run commands from repository root; use explicit paths/workdir for `hardware/firmware`.
- Keep `hardware/firmware/esp32/` read-only.
- Store operational logs in `hardware/firmware/logs/` only.
- Enforce local USB wait gate in bash/python scripts (no chat wait loops):
  - print `⚠️ BRANCHE L’USB MAINTENANT ⚠️` exactly 3 times
  - ring bell (`\a`)
  - block until Enter
- Serial smoke policy:
  - ESP32 USB default baud: `115200`
  - ESP8266 USB (CP2102) default baud: `115200`, monitor-only (no binary TX)
  - UI link verdict from ESP32 side: `UI_LINK_STATUS connected==1`
  - panic/reboot markers are strict FAIL

## Must Not
- Do not transmit binary payloads to ESP8266 monitor-only serial path.
- Do not place runtime logs outside `hardware/firmware/logs/`.

## Execution Flow
1. Run safety checkpoint.
2. Run/adjust local scripts under `hardware/firmware/tools/dev` for waits/smokes.
3. Run firmware gates.
4. Commit and report.

## Gates
- `pio run -e esp32dev`
- `pio run -e esp32_release`
- `pio run -e esp8266_oled`
- `pio run -e ui_rp2040_ili9488`
- `pio run -e ui_rp2040_ili9486`
- Optional smoke entrypoint: `bash hardware/firmware/tools/test/hw_now.sh`

## Reporting
Use root reporting format. Keep output concise and evidence-based.

## Stop Conditions
Use root stop conditions.
