
# Firmware UI RP2040 TFT (LVGL + UI Link v2)

> **[Mise à jour 2026]**
>
> **Tous les assets LittleFS (scénarios, écrans, scènes, audio, etc.) sont désormais centralisés dans le dossier `hardware/firmware/data/` à la racine du projet.**
>
> Ce dossier unique sert de source pour le flash LittleFS sur ESP32, ESP8266 et RP2040. Les anciens dossiers `data/` dans les sous-projets doivent être migrés/supprimés (voir encart migration ci-dessous).


Firmware UI tactile RP2040 pour ecran TFT 3.5" + XPT2046.

## Points clefs

- UI framework: LVGL
- Drivers: TFT_eSPI + XPT2046_Touchscreen
- Lien serie: UI Link v2 (`HELLO/ACK/KEYFRAME/STAT/PING/PONG/BTN`)
- Touch -> boutons logiques (`PREV/NEXT/OK/BACK/VOL-/VOL+`)
- Ecran degrade `LINK DOWN` si timeout

## Build

Depuis `hardware/firmware`:

```sh
pio run -e ui_rp2040_ili9488
pio run -e ui_rp2040_ili9486
```

Flash:

```sh
pio run -e ui_rp2040_ili9488 -t upload --upload-port <PORT_RP2040>
```

Boucle rapide:

```sh
make fast-ui-tft UI_TFT_PORT=<PORT_RP2040>
```


---

## Migration LittleFS (2026)

- Déplacer tous les fichiers d’écrans/scènes dans `hardware/firmware/data/story/screens/`.
- Adapter les scripts de génération et de flash pour pointer vers ce dossier.
- Supprimer l’ancien dossier `ui/rp2040_tft/data/` après migration.

---

## Configuration

- Pins TFT/touch/UART dans `include/ui_config.h`
- Contrat protocole dans `../../protocol/ui_link_v2.md`
- Cablage detaille dans `WIRING.md`
