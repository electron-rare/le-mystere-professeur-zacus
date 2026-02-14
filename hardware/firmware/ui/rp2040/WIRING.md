# Wiring RP2040 UI <-> TFT Touch <-> ESP32

## 1) RP2040 -> TFT + Touch (SPI shared)

| Function | RP2040 pin | Note |
|---|---:|---|
| SPI SCK | GP2 | partagé TFT + XPT2046 |
| SPI MOSI | GP3 | partagé TFT + XPT2046 |
| SPI MISO | GP4 | partagé TFT + XPT2046 |
| TFT CS | GP5 | chip select écran |
| TFT DC/RS | GP6 | data/command |
| TFT RST | GP7 | reset écran |
| Touch CS | GP9 | chip select XPT2046 |
| Touch IRQ | GP15 | optionnel mais recommandé |
| 5V | 5V | alim module écran |
| GND | GND | masse commune |

## 2) RP2040 -> ESP32 (UART JSONL)

| UART | RP2040 | ESP32 |
|---|---|---|
| TX | GP0 | RX GPIO18 |
| RX | GP1 | TX GPIO19 |
| GND | GND | GND |

## 3) Mapping vers header 40 pins type Raspberry (module LCD)

| Header 40p (phys) | Function | RP2040 |
|---:|---|---|
| 2 / 4 | 5V | 5V |
| 6 | GND | GND |
| 23 | SPI SCLK | GP2 |
| 19 | SPI MOSI | GP3 |
| 21 | SPI MISO | GP4 |
| 24 | LCD_CS | GP5 |
| 18 | LCD_DC | GP6 |
| 22 | LCD_RST | GP7 |
| 26 | TP_CS | GP9 |
| 11 | TP_IRQ | GP15 |

## 4) Notes critiques
1. Le bus touch doit rester plus lent que le bus TFT (`SPI_TOUCH_FREQUENCY` plus bas).
2. Toujours partager la masse RP2040 / ESP32 / écran.
3. Si l’écran reste noir: vérifier alimentation/backlight et test ILI9488 vs ILI9486.
