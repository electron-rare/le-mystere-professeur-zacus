# OSCILLO Firmware (ESP32-S3 + ESP8266 OLED)

Firmware Arduino/PlatformIO pour deux cibles dans ce dossier:
- `esp32-s3-devkitc-1-n16r8`
- `esp8266-hw630-oled`

## Etat Des Firmwares (snapshot)

Date: `2026-02-26`

- `esp32-s3-devkitc-1-n16r8`:
  - build: OK
  - upload: OK
  - ESP-NOW discovery runtime: actif (60s)
  - API peers: actif (`/api/espnow/peers`)
  - Morse: ralenti (profil enfant)
- `esp8266-hw630-oled`:
  - build: OK
  - upload: OK
  - OLED: mode oscilloscope plein ecran (sans texte)
  - overlay sinus glitch: actif, amplitude aleatoire variable
  - ESP-NOW discovery runtime: actif (60s)
  - API peers: actif (`/api/espnow/peers`)
  - Morse: ralenti (profil enfant)

## Environnements PlatformIO

- `esp32-s3-devkitc-1-n16r8`:
  - fichier: `src/main.cpp`
  - carte: ESP32-S3 DevKitC-1 (N16R8)
- `esp8266-hw630-oled`:
  - fichier: `src/main_esp8266_oled.cpp`
  - carte: ESP8266 OLED HW-630 (NodeMCU-like)

## Fonctionnalites ESP32-S3

- Joystick: `SW=GPIO5`, `VRx=GPIO6`, `VRy=GPIO7`
- Servos: `GPIO10` (X), `GPIO11` (Y)
- Morse `LEFOU`: sortie `GPIO4`
- Pseudo DAC 4-bit: `GPIO15/16/17/18`
- Wi-Fi STA (best RSSI pour SSID cible) + fallback AP
- ESP-NOW broadcast + discovery runtime
- Web UI + API HTTP
- LED systeme + LED RGB onboard

Morse (profil ralenti):
- unit dynamique joystick X: `180..500 ms`
- pauses inter-lettres allongees (adaptation enfant)

## Fonctionnalites ESP8266 OLED HW-630

- Joystick: `A0` + `SW=D3`
- Morse `LEFOU`: sortie `D4`
- Pseudo DAC 4-bit: `D5/D6/D7/D8` (LSB->MSB)
- OLED SSD1306:
  - I2C par defaut: `DA(D1)=SDA`, `D2=SCL`
  - auto-detect adresse `0x3C/0x3D`
  - mode affichage: oscilloscope plein ecran (sans texte)
  - overlay sinus "glitch" avec amplitude variable aleatoire
- Wi-Fi STA + fallback AP
- ESP-NOW broadcast + discovery runtime
- Web UI + API HTTP

Morse (profil ralenti):
- unit fixe: `350 ms`
- pauses inter-lettres/fin de mot fortement allongees

Attention bootstrap ESP8266:
- `D4` et `D8` sont des pins de strap boot.
- Eviter de les forcer au mauvais niveau au reset/power-on.

## ESP-NOW Discovery (les 2 cibles)

Comportement runtime:
- emission broadcast `type=\"discovery\"` toutes les `60 s`
- reponse unicast `type=\"announce\"` a la reception d'un discovery
- table locale de peers vue + timeout d'activite

Rythme:
- periode discovery: `60 s`
- peer considere actif pendant `180 s`

API peers:
- `GET /api/espnow/peers`

Commandes serie utiles:
- `espnow`
- `peers`
- `discover` (force un discovery immediat)

## Web API

Endpoints communs:
- `GET /`
- `GET /api/status`
- `GET /api/wifi/scan`
- `POST /api/wifi/select` (`ssid`, `pass`)
- `GET /api/espnow/peers`

## Build / Flash

Depuis ce dossier:

```bash
platformio run -e esp32-s3-devkitc-1-n16r8
platformio run -e esp32-s3-devkitc-1-n16r8 --target upload

platformio run -e esp8266-hw630-oled
platformio run -e esp8266-hw630-oled --target upload --upload-port /dev/cu.usbserial-8
```

Ports utilises recemment:
- ESP32-S3: `/dev/cu.usbmodem146301`
- ESP8266: `/dev/cu.usbserial-8`

## Monitor Serie

```bash
platformio device monitor --baud 115200 --port <PORT>
```

## Cablage pseudo DAC 4-bit (resistor ladder + RC)

Principe (applicable ESP32-S3 et ESP8266):
- bit3 -> `10k` -> noeud analogique
- bit2 -> `20k` -> noeud analogique
- bit1 -> `39k/40k` -> noeud analogique
- bit0 -> `82k/80k` -> noeud analogique
- noeud -> `3.3k` serie -> sortie
- sortie -> `100nF` vers GND
- bleeder `100k` du noeud vers GND

Ne pas connecter un HP directement sur la sortie pseudo analogique.

## Specs / Docs

- ESP8266 HW-630 notes:
  - `docs/specs/esp8266-oled-hw630/SPEC_ESP8266_OLED_HW-630.md`
- YD-ESP32-23 notes:
  - `docs/specs/yd-esp32-23/SPEC_YD-ESP32-23_2022-V1.3.md`

## Structure

- ESP32-S3: `src/main.cpp`
- ESP8266 OLED: `src/main_esp8266_oled.cpp`
- PlatformIO: `platformio.ini`
