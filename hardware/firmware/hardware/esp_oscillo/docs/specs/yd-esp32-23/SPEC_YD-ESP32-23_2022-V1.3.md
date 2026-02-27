# Spec Note - YD-ESP32-23 / YD-ESP32-S3 (2022-V1.3)

Date de collecte: 2026-02-26

## Contexte

Tu as demande la spec de la carte `YD-ESP32-23 2022-V1.3`.

Point important:
- Je n'ai pas trouve de document officiel public nomme exactement `2022-V1.3`.
- Les sources publiques disponibles donnent surtout:
  - un schema `YD-ESP32-S3-SCH-V1.4.pdf`
  - des pinouts/images de la famille YD-ESP32-23 / YD-ESP32-S3
  - une note communautaire indiquant qu'une revision ancienne 2022 est en `V1.2` (et la revision reference en `V1.4`).

## Docs telecharges dans ce dossier

- `YD-ESP32-S3-SCH-V1.4.pdf`
- `yd-esp32-s3-devkitc-1-clone-pinout.jpg`
- `YD-ESP32-23_V1120.jpg`

## Donnees utiles confirmees pour ton firmware

- MCU: ESP32-S3 (famille WROOM-1).
- Deux ports USB:
  - USB natif ESP32-S3 (`GPIO19/GPIO20`)
  - USB-UART via CH343P.
- RGB LED onboard adressable de type WS2812 pilotee par `GPIO48`.
- Bouton BOOT lie a `GPIO0`.
- `GPIO43/44` utilises par TX/RX LEDs selon certaines variantes.

Ces points sont coherents avec le firmware actuel (`RGB` sur 48, usage des deux ports USB, etc.).

## Sources

- Repo YD-ESP32-23 (fichiers hardware):
  - https://github.com/rtek1000/YD-ESP32-23
- Schema:
  - https://raw.githubusercontent.com/rtek1000/YD-ESP32-23/main/YD-ESP32-S3-SCH-V1.4.pdf
- Pinout image:
  - https://raw.githubusercontent.com/rtek1000/YD-ESP32-23/main/yd-esp32-s3-devkitc-1-clone-pinout.jpg
- Board image:
  - https://raw.githubusercontent.com/rtek1000/YD-ESP32-23/main/YD-ESP32-23_V1120.jpg
- Repo VCC-GND (infos hardware + notes de versions):
  - https://github.com/vcc-gnd/YD-ESP32-S3

