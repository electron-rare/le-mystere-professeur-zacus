# README - Hardware Firmware (racine)

Ce dossier regroupe tous les firmwares et ressources partagées du projet :

- esp32/ : firmware principal ESP32 Audio Kit
- ui/rp2040/ : firmware UI tactile RP2040
- ui/esp8266_oled/ : firmware écran OLED ESP8266 (optionnel)
- docs/protocols/ : protocoles, specs, templates et exemples centralisés
- build_all.sh : script pour builder tous les firmwares
- .clang-format : style C++ commun

## Centralisation

Tous les protocoles, templates, schémas et exemples STORY sont désormais dans `docs/protocols/`.

## Build global

Pour builder tous les firmwares :
```sh
./build_all.sh
```

Ou directement avec PlatformIO depuis cette racine:
```sh
pio run
```

## Build ciblé

Depuis `hardware/firmware/`:
```sh
pio run -e esp32dev
pio run -e esp32_release
pio run -e ui_rp2040_ili9488
pio run -e ui_rp2040_ili9486
pio run -e esp8266_oled
```

Voir `README_PLATFORMIO.txt` pour les détails.

## Convention
- Un README harmonisé dans chaque firmware
- Style C++ imposé par `.clang-format`
- Documentation et specs partagées dans `docs/protocols/`

---

*Merci de maintenir cette organisation pour faciliter la maintenance et l’intégration.*
