# Wiring RP2040 TFT <-> ESP32 Audio

## RP2040 -> TFT + touch (SPI partage)

| Fonction | RP2040 |
|---|---|
| SPI SCK | GP2 |
| SPI MOSI | GP3 |
| SPI MISO | GP4 |
| TFT CS | GP5 |
| TFT DC | GP6 |
| TFT RST | GP7 |
| TOUCH CS | GP9 |
| TOUCH IRQ | GP15 |
| 5V | 5V |
| GND | GND |

## RP2040 <-> ESP32 (UI Link v2 UART)

| Lien | RP2040 | ESP32 Audio |
|---|---|---|
| UI TX -> ESP RX | GP0 | GPIO19 |
| UI RX <- ESP TX | GP1 | GPIO22 |
| Masse | GND | GND |

Notes:
- Niveau logique 3.3V uniquement.
- UART full duplex obligatoire pour hot-swap et heartbeat.
- Baud par defaut: 19200.
