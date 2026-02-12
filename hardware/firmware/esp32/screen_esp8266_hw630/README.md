# Firmware ecran ESP8266 (HW-630)

Ce firmware transforme une carte ESP8266 (HW-630) en ecran de statut pour l'ESP32.

## Affichage

- Etat detection LA (440 Hz)
- Etat lecture MP3
- Etat SD
- Uptime ESP32
- Derniere touche (`KEY`)

## Protocole UART

Trame texte envoyee par l'ESP32:

`STAT,<la>,<mp3>,<sd>,<uptime_ms>,<key>\n`

Exemple:

`STAT,1,1,1,12345,3`

## Cablage

- **Masse commune obligatoire**: ESP32 GND <-> ESP8266 GND
- ESP32 `GPIO33` (TX2) -> ESP8266 `D6` (RX SoftwareSerial)
- ESP32 `GPIO21` (RX2) <- ESP8266 `D5` (TX SoftwareSerial)
- OLED I2C sur ESP8266:
  - `D2` SDA
  - `D1` SCL

## Build / Flash

Depuis ce dossier:

1. `pio run`
2. `pio run -t upload`
3. `pio device monitor`
