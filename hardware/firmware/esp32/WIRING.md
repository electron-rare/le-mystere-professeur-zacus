# WIRING ESP32 <-> ESP8266 OLED

Ce document decrit le branchement entre:
- la carte principale ESP32 (firmware principal),
- la carte ESP8266 NodeMCU + ecran OLED (firmware `esp8266_oled`).

## 1) Liaison serie entre les 2 cartes (obligatoire)

| ESP32 | ESP8266 NodeMCU | Role |
|---|---|---|
| `GPIO22 (TX)` | `D6 (RX)` | Telemetrie ESP32 -> ecran |
| `GND` | `GND` | Masse commune obligatoire |

Notes:
- Le lien est unidirectionnel: ESP32 vers ESP8266.
- `D5 (TX)` de l'ESP8266 n'est pas utilise.
- Debit serie: `57600 bauds`.

## 2) OLED sur la carte ESP8266

Branchement recommande:

| OLED | ESP8266 NodeMCU |
|---|---|
| `VCC` | `3V3` |
| `GND` | `GND` |
| `SDA` | `D1` (recommande) |
| `SCL` | `D2` (recommande) |

Notes:
- Logique en `3.3V` uniquement.
- Ne pas alimenter/sortir de signal en `5V` sur ESP32/ESP8266.
- Le firmware OLED accepte aussi le cablage inverse `SDA=D2` / `SCL=D1` en fallback auto.

## 3) Alimentation

Options possibles:
- Chaque carte alimentee par son propre USB.
- Une seule alimentation 5V externe, a condition de respecter les tensions de chaque carte.

Dans tous les cas:
- `GND ESP32` et `GND ESP8266` doivent etre relies.

## 4) Schema rapide

```text
ESP32                     ESP8266 NodeMCU + OLED
-----                     ----------------------
GPIO22 (TX) ----------->  D6 (RX)
GND       -------------   GND

OLED VCC  -------------   3V3
OLED GND  -------------   GND
OLED SDA  -------------   D1 (recommande)
OLED SCL  -------------   D2 (recommande)
```

## 5) Verification rapide

1. Flasher l'ESP32: `pio run -e esp32dev -t upload --upload-port <PORT_ESP32>`.
2. Flasher l'ESP8266: `pio run -e esp8266_oled -t upload --upload-port <PORT_ESP8266>`.
3. Ouvrir le moniteur ESP8266: `pio device monitor -e esp8266_oled --port <PORT_ESP8266>`.
4. Verifier que l'ecran passe de `Demarrage...` a un ecran de mode (`U_LOCK`, `U-SON FONCTIONNEL` ou `LECTEUR U-SON`) apres reception des trames.

Equivalents Makefile:
- `make upload-esp32 ESP32_PORT=<PORT_ESP32>`
- `make upload-screen SCREEN_PORT=<PORT_ESP8266>`
- `make monitor-screen SCREEN_PORT=<PORT_ESP8266>`
