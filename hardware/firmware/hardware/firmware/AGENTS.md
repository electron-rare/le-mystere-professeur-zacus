# Firmware Agent Contract (Nested Workspace)

Purpose: same firmware contract when VS Code is opened at `hardware/firmware/hardware/firmware`.

Canonical workspace:
- real tool/build path is `../../` (repo `hardware/firmware`)

Bootstrap:
- `cd ../../ && ./tools/dev/bootstrap_local.sh`

Build gates:
- `cd ../../ && ./build_all.sh`
- or explicit PlatformIO envs: `esp32dev`, `esp32_release`, `esp8266_oled`, `ui_rp2040_ili9488`, `ui_rp2040_ili9486`

Smoke gates:
- `cd ../../ && ./tools/dev/run_matrix_and_smoke.sh`
- CP2102 LOCATION mapping: `20-6.1.1=esp32`, `20-6.1.2=esp8266_usb`

Baud policy:
- USB monitor: `115200`
- internal ESP8266 UI SoftwareSerial link: `19200`

Fast targets:
- `cd ../../ && make fast-esp32`
- `cd ../../ && make fast-ui-oled`
- `cd ../../ && make fast-ui-tft`

Logs/artifacts:
- `../../logs/` and `../../artifacts/`
