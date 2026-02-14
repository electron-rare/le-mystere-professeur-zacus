Configuration PlatformIO centralisee (hardware/firmware)
=========================================================

Ce depot utilise un seul `platformio.ini` a la racine.

Environnements:
- esp32dev
- esp32_release
- esp8266_oled
- ui_rp2040_ili9488
- ui_rp2040_ili9486

Commandes utiles:
- Build complet: `pio run`
- Build cible: `pio run -e <env>`
- Upload: `pio run -e <env> -t upload --upload-port <PORT>`
- Monitor: `pio device monitor -e <env> --port <PORT>`
- Matrix locale: `./build_all.sh`

Rappels:
- `src_dir = esp32_audio/src` (sources ESP32 par defaut)
- Chaque env isole ses sources via `build_src_filter`
- Header protocole partage: `protocol/ui_link_v2.h` via `-Iprotocol`
