# ESP8266 OLED HW-630 - Notes de spec

Date de collecte: 2026-02-26.

## Ce qu'on peut fixer avec confiance

- MCU: famille ESP8266 (souvent ESP-12E/ESP-12F selon clones).
- OLED onboard: SSD1306 I2C, 128x64, adresse habituelle `0x3C` (parfois `0x3D`).
- Le pinout I2C change selon revision/fabricant clone.

## Conflit de pinout observe dans la doc publique

1. Mapping "Wemos style" (le plus frequent):
- SDA = `D1` (GPIO5)
- SCL = `D2` (GPIO4)

2. Mapping alternatif mentionne sur cartes OLED integrees:
- SDA = `D5` (GPIO14)
- SCL = `D6` (GPIO12)

## Decision firmware dans ce repo

Pour coller a la demande projet (`DA = SDA`, `D2 = SCL`):
- mapping par defaut compile: `SDA=D1 (DA)` et `SCL=D2`.
- fallback prevu via flag `OLED_ALT_D5_D6=1` si la carte reelle est une revision D5/D6.

## Sources consultees

- HomeDing (pin mapping board OLED integree en D1/D2):
  - https://homeding.github.io/boards/esp8266/oled.htm
- Manuals+ (mapping alternatif D5/D6 selon modele):
  - https://manuals.plus/m/e765021ba7791ebb8f7dc64f945a3670a3ea9d0cb9af20de4ec1c9bc8214065e
- Paterweb (variantes ESP8266 OLED et I2C non standard possibles):
  - https://www.paterweb.com/esp8266-oled-development-board-a-compact-wifi-enabled-board-with-built-in-display/
- Projet de reference qui distingue revisions HW-364A / HW-630 et impose de verifier les pins:
  - https://github.com/arduinocelentano/ssid

## Conseils de validation rapide sur cible

1. Flasher firmware `esp8266-hw630-oled`.
2. Ouvrir serie 115200 et lancer commande `i2c`.
3. Si aucun peripherique trouve sur `0x3C/0x3D`, recompiler avec `-DOLED_ALT_D5_D6=1`.
4. Verifier que l'OLED s'initialise et affiche l'IP/mode Wi-Fi.
