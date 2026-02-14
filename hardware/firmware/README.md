# Hardware Firmware

Workspace PlatformIO unique pour 3 firmwares:

- `esp32_audio/`: firmware principal audio
- `ui/esp8266_oled/`: UI OLED legere
- `ui/rp2040_tft/`: UI TFT tactile (LVGL)
- `protocol/`: contrat UART partage (`ui_link_v2.md`, `ui_link_v2.h`)

## Build

```sh
pio run
```

Build cible:

```sh
pio run -e esp32dev
pio run -e esp32_release
pio run -e esp8266_oled
pio run -e ui_rp2040_ili9488
pio run -e ui_rp2040_ili9486
```

Script global:

```sh
./build_all.sh
```

## Boucle dev rapide

```sh
make fast-esp32 ESP32_PORT=<PORT_ESP32>
make fast-ui-oled UI_OLED_PORT=<PORT_ESP8266>
make fast-ui-tft UI_TFT_PORT=<PORT_RP2040>
```

Smoke serie:

```sh
python3 tools/dev/serial_smoke.py --role auto --wait-port 180
```

MacOS CP2102 duplicates share VID/PID=10C4:EA60/0001; the LOCATION (20-6.1.1=ESP32, 20-6.1.2=ESP8266) drives the detector. Adjust `tools/dev/ports_map.json` if your rig changes and prefer `/dev/cu.SLAB_*` over `usbserial-*`.

## Docs

- Cablage ESP32/UI: `esp32_audio/WIRING.md`
- Cablage TFT: `ui/rp2040_tft/WIRING.md`
- Quickstart flash: `docs/QUICKSTART.md`
- Protocole: `protocol/ui_link_v2.md`
