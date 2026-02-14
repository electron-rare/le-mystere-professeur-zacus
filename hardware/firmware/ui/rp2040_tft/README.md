# Firmware UI RP2040 TFT (LVGL + UI Link v2)

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

## Configuration

- Pins TFT/touch/UART dans `include/ui_config.h`
- Contrat protocole dans `../../protocol/ui_link_v2.md`
- Cablage detaille dans `WIRING.md`
