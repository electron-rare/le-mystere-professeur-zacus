# Wiring ESP32 Audio <-> UI (OLED ou TFT)

Le firmware ESP32 Audio expose un lien UART UI Link v2 full duplex.

## UART UI Link v2 (obligatoire)

| ESP32 Audio | UI cible | Role |
|---|---|---|
| GPIO22 (TX) | RX UI | Trames ESP32 -> UI |
| GPIO19 (RX) | TX UI | Inputs/UI heartbeat -> ESP32 |
| GND | GND | Masse commune |

Contraintes:
- 3.3V TTL uniquement.
- Baud par defaut: 19200.
- Hot-swap supporte: debrancher OLED et brancher TFT (ou inverse) sans reflasher ESP32.

## Option A - UI ESP8266 OLED

| ESP8266 | ESP32 |
|---|---|
| D6 (RX) | GPIO22 (TX) |
| D5 (TX) | GPIO19 (RX) |
| GND | GND |

I2C OLED:
- SDA: D1 (fallback D2)
- SCL: D2 (fallback D1)

## Option B - UI RP2040 TFT

| RP2040 | ESP32 |
|---|---|
| GP1 (RX) | GPIO22 (TX) |
| GP0 (TX) | GPIO19 (RX) |
| GND | GND |

Voir `../ui/rp2040_tft/WIRING.md` pour SPI TFT + touch.

## Verification rapide

1. Flasher ESP32: `pio run -e esp32dev -t upload --upload-port <PORT_ESP32>`
2. Flasher UI choisie (`esp8266_oled` ou `ui_rp2040_ili9488`).
3. Ouvrir le moniteur ESP32.
4. Verifier `UI_LINK_STATUS`:
   - `connected=1` apres `HELLO/ACK`
   - `pong>0` apres heartbeat
