Configuration PlatformIO centralisée (racine firmware)
=====================================================

Ce dépôt utilise un seul fichier `platformio.ini` à la racine `hardware/firmware/`.

Environnements disponibles:
- esp32dev
- esp32_release
- ui_rp2040_ili9488
- ui_rp2040_ili9486
- esp8266_oled

Commandes:
- Build complet: `pio run`
- Build ciblé: `pio run -e <env>`
- Build script: `./build_all.sh`
- Upload ciblé: `pio run -e <env> -t upload --upload-port <PORT>`
- Monitor ciblé: `pio device monitor -e <env> --port <PORT>`

Règle de structure:
- Les sources sont sélectionnées par `build_src_filter` par environnement.
- Ne pas remettre de `platformio.ini` local dans `esp32/` ou `ui/*`.
