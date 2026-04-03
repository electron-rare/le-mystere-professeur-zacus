# GPIO Mapping — Zacus V3 Puzzle Boards

All puzzle nodes run ESP-NOW slave firmware. Pinout must match `ESP32_ZACUS/puzzles/common/espnow_slave.h`.
Master MAC address is hard-coded per production run and set via `espnow_slave_init()` at boot.

---

## P1 — Séquence Sonore (ESP32-S3-DevKitC-1-N16R8)

### GPIO Table

| GPIO | Function          | Direction | Notes                                          |
|------|-------------------|-----------|------------------------------------------------|
| 4    | I2S BCLK          | OUT       | MAX98357A bit clock                            |
| 5    | I2S LRCLK (WS)    | OUT       | MAX98357A left/right clock                     |
| 6    | I2S DIN           | OUT       | MAX98357A data in                              |
| 7    | MAX98357A SD_MODE | OUT       | HIGH = stereo / LOW = shutdown                 |
| 8    | WS2812B DIN       | OUT       | 4-LED strip via SN74HCT125 level shifter       |
| 15   | Button RED        | IN_PULLUP | Active LOW — 10kΩ external pull-up             |
| 16   | Button BLUE       | IN_PULLUP | Active LOW — 10kΩ external pull-up             |
| 17   | Button YELLOW     | IN_PULLUP | Active LOW — 10kΩ external pull-up             |
| 18   | Button GREEN      | IN_PULLUP | Active LOW — 10kΩ external pull-up             |

### Power Rails
- 5V from USB-C → AMS1117-3.3 → ESP32 3V3 rail
- 5V direct → MAX98357A VDD and WS2812B VCC

### Wiring Diagram

```
USB-C 5V ─────┬──────────────────────────── MAX98357A VDD
              │                         ┌── WS2812B VCC
              └── AMS1117-3.3 ── 3V3 ──┤── ESP32 3V3
                                        └── SN74HCT125 VCC

ESP32-S3
  GPIO4  ──────────────────────────────── MAX98357A BCLK
  GPIO5  ──────────────────────────────── MAX98357A LRC
  GPIO6  ──────────────────────────────── MAX98357A DIN
  GPIO7  ──── 3V3 (tied HIGH) ─────────── MAX98357A SD_MODE

  GPIO8  ──── SN74HCT125 A1 ── Y1 ─────── WS2812B DIN (4 LEDs)

  GPIO15 ──┬── 10kΩ ── 3V3
           └────────────────────────────── Arcade Button RED  (NO → GND)

  GPIO16 ──┬── 10kΩ ── 3V3
           └────────────────────────────── Arcade Button BLUE (NO → GND)

  GPIO17 ──┬── 10kΩ ── 3V3
           └────────────────────────────── Arcade Button YELLOW (NO → GND)

  GPIO18 ──┬── 10kΩ ── 3V3
           └────────────────────────────── Arcade Button GREEN  (NO → GND)

  MAX98357A OUTP/OUTN ─────────────────── Speaker 8Ω 2W
```

---

## P2 — Circuit LED (ESP32-DevKitC-32E)

### GPIO Table

| GPIO | Function             | Direction | Notes                                        |
|------|----------------------|-----------|----------------------------------------------|
| 4    | Reed Switch 1        | IN_PULLUP | Magnetic sensor — 10kΩ external pull-up      |
| 5    | Reed Switch 2        | IN_PULLUP | Magnetic sensor — 10kΩ external pull-up      |
| 6    | Reed Switch 3        | IN_PULLUP | Magnetic sensor — 10kΩ external pull-up      |
| 7    | Reed Switch 4        | IN_PULLUP | Magnetic sensor — 10kΩ external pull-up      |
| 8    | Reed Switch 5        | IN_PULLUP | Magnetic sensor — 10kΩ external pull-up      |
| 9    | Reed Switch 6        | IN_PULLUP | Magnetic sensor — 10kΩ external pull-up      |
| 10   | Reed Switch 7        | IN_PULLUP | Magnetic sensor — 10kΩ external pull-up      |
| 11   | Reed Switch 8        | IN_PULLUP | Magnetic sensor — 10kΩ external pull-up      |
| 12   | WS2812B DIN Seg A    | OUT       | LED segment 1–2 via 74HCT125                 |
| 13   | WS2812B DIN Seg B    | OUT       | LED segment 3–4 via 74HCT125                 |
| 14   | WS2812B DIN Seg C    | OUT       | LED segment 5–6 via 74HCT125                 |
| 15   | WS2812B DIN Seg D    | OUT       | LED segment 7–8 via 74HCT125                 |
| 16   | WS2812B DIN Seg E    | OUT       | LED segment 9–10 via 74HCT125                |
| 17   | WS2812B DIN Seg F    | OUT       | LED segment 11–12 via 74HCT125               |
| 18   | WS2812B DIN Seg G    | OUT       | LED segment 13–14 via 74HCT125               |
| 19   | WS2812B DIN Seg H    | OUT       | LED segment 15–16 via 74HCT125               |
| 21   | Buzzer               | OUT       | NPN 2N2222 base via 1kΩ → active buzzer 5V  |
| 22   | Circuit Valid LED    | OUT       | Green LED via 470Ω series resistor           |

### Wiring Diagram

```
USB-C 5V ────┬───────────────────────────── WS2812B VCC (each segment)
             │                         ┌── 1000µF bulk cap (5V decoupling)
             └── AMS1117-3.3 ── 3V3 ──── ESP32 3V3

ESP32
  GPIO4..11 ─── (each) ──┬── 10kΩ ── 3V3
                          └───────────────── Reed Switch (NO → GND)

  GPIO12..19 ─── SN74HCT125 Ax ── Yx ────── WS2812B DINx (2 LEDs each)

  GPIO21 ──── 1kΩ ──── Q1(2N2222) base
                Q1 collector ─ Buzzer+ (5V)
                Q1 emitter  ─ GND
                Buzzer- ─ GND

  GPIO22 ──── 470Ω ──── LED Green+ ──── GND

Magnetic board (folding 60x40cm):
  Reed switches are embedded in the board surface
  Magnetic component pegs activate the reed switches when placed
  Board connector: 10-pin ribbon → ESP32 GPIO4..11 + 2x GND
```

---

## P3 — QR Treasure (No dedicated electronics)

P3 uses the BOX-3 camera directly for QR scanning. No separate ESP32 board.

| Component | Notes |
|-----------|-------|
| 6× A5 laminated QR cards | Printed + laminated — QR content set at scenario compile time |
| BOX-3 camera | ESP32-S3-BOX-3 integrated camera — handles scanning |

QR card placement order encoded in `game/scenarios/zacus_v3_complete.yaml` under `P3_QR.correct_order`.

---

## P4 — Fréquence Radio (ESP32-DevKitC-32E)

### GPIO Table

| GPIO | Function           | Direction | Notes                                            |
|------|--------------------|-----------|--------------------------------------------------|
| 4    | I2S BCLK           | OUT       | MAX98357A — radio static synthesis               |
| 5    | I2S LRCLK (WS)     | OUT       | MAX98357A                                        |
| 6    | I2S DIN            | OUT       | MAX98357A data                                   |
| 12   | Rotary Encoder DT  | IN_PULLUP | KY-040 — direction detect                        |
| 13   | Rotary Encoder SW  | IN_PULLUP | KY-040 push button — confirm selection           |
| 14   | Rotary Encoder CLK | IN_PULLUP | KY-040 — clock signal                            |
| 21   | I2C SDA (OLED)     | BIDIR     | SSD1306 128x64 — 4.7kΩ pull-up to 3V3           |
| 22   | I2C SCL (OLED)     | OUT       | SSD1306 — 4.7kΩ pull-up to 3V3                  |
| 25   | DAC Output         | OUT       | RC low-pass → analog indicator LED (warm/cold)   |

### Power Rails
- 2× 18650 (2S) → TP4056 charger → MT3608 boost 5V → AMS1117-3.3 → 3V3
- Alternatively: USB-C 5V input for bench/testing

### Wiring Diagram

```
2x 18650 (2S = 7.4V nom) ── TP4056 ── MT3608 boost ── 5V rail
  5V rail ─────┬───────────────────────── MAX98357A VDD
               └── AMS1117-3.3 ── 3V3 ── ESP32 3V3

ESP32
  GPIO4  ──────────────────────────── MAX98357A BCLK
  GPIO5  ──────────────────────────── MAX98357A LRC
  GPIO6  ──────────────────────────── MAX98357A DIN
  MAX98357A OUTP/OUTN ─────────────── Speaker 8Ω 2W (in retro grille)

  GPIO14 ──┬── 10kΩ ── 3V3
           └───────────────────────── KY-040 CLK
  GPIO12 ──┬── 10kΩ ── 3V3
           └───────────────────────── KY-040 DT
  GPIO13 ──┬── 10kΩ ── 3V3
           └───────────────────────── KY-040 SW (GND when pressed)

  GPIO21 ──┬── 4.7kΩ ── 3V3
           └───────────────────────── SSD1306 SDA
  GPIO22 ──┬── 4.7kΩ ── 3V3
           └───────────────────────── SSD1306 SCL

  GPIO25 ── 10kΩ ──┬── 100nF ── GND  (RC low-pass filter)
                   └────────────────── LED warm/cold indicator anode
                     LED cathode ── 470Ω ── GND
```

---

## P5 — Code Morse (ESP32-DevKitC-32E)

### GPIO Table

| GPIO | Function             | Direction | Notes                                          |
|------|----------------------|-----------|------------------------------------------------|
| 4    | Telegraph Key        | IN_PULLUP | Brass key NO contact — 10kΩ external pull-up   |
| 5    | Buzzer               | OUT       | NPN 2N2222 base via 1kΩ — active buzzer 5V    |
| 6    | LED Red (key press)  | OUT       | Key pressed indicator — 470Ω series            |
| 7    | LED Green (validated)| OUT       | Morse message valid — 470Ω series              |
| 8    | WS2812B (light mode) | OUT       | 1 LED — NON_TECH visual morse pulses           |

### Wiring Diagram

```
USB-C 5V ────── AMS1117-3.3 ────── 3V3 ── ESP32 3V3

ESP32
  GPIO4 ──┬── 10kΩ ── 3V3
          └──────────────────────── Telegraph Key+ (brass contact)
               Telegraph Key- ───── GND

  GPIO5 ──── 1kΩ ──── Q1(2N2222) base
               Q1 collector ── Buzzer+ ── 5V
               Q1 emitter   ── GND
               Buzzer-       ── GND

  GPIO6 ──── 470Ω ──── LED Red anode ──── GND

  GPIO7 ──── 470Ω ──── LED Green anode ── GND

  GPIO8 ──────────────────────────── WS2812B DIN (1 LED, 5V)
    [3V3 → 5V: use SN74HCT125 or direct (WS2812B tolerates 3V3 DIN)]
```

---

## P6 — Symboles Alchimiques NFC (ESP32-DevKitC-32E)

### GPIO Table

| GPIO | Function          | Direction | Notes                                         |
|------|-------------------|-----------|-----------------------------------------------|
| 4    | MFRC522 RST       | OUT       | NFC module reset                              |
| 5    | MFRC522 SDA (SS)  | OUT       | SPI chip select                               |
| 18   | MFRC522 SCK       | OUT       | SPI clock                                     |
| 19   | MFRC522 MISO      | IN        | SPI data from MFRC522                         |
| 23   | MFRC522 MOSI      | OUT       | SPI data to MFRC522                           |
| 25   | Buzzer            | OUT       | NPN 2N2222 base via 1kΩ                       |
| 26   | LED Green (valid) | OUT       | Configuration valid — 470Ω                    |
| 27   | LED Red (wrong)   | OUT       | Wrong placement — 470Ω                        |

### Wiring Diagram

```
USB-C 5V ───── AMS1117-3.3 ───── 3V3 ── ESP32 3V3

ESP32
  GPIO5  (SDA/SS) ────────────── MFRC522 SDA
  GPIO18 (SCK)    ────────────── MFRC522 SCK
  GPIO19 (MISO)   ────────────── MFRC522 MISO
  GPIO23 (MOSI)   ────────────── MFRC522 MOSI
  GPIO4  (RST)    ────────────── MFRC522 RST
  3V3             ────────────── MFRC522 3.3V
  GND             ────────────── MFRC522 GND
  [MFRC522 IRQ pin: not connected — polling mode]

  GPIO25 ── 1kΩ ── Q1(2N2222) base
              Q1 collector ── Buzzer+ ── 5V
              Q1 emitter   ── GND

  GPIO26 ── 470Ω ── LED Green ── GND
  GPIO27 ── 470Ω ── LED Red   ── GND

Wooden tablet (300x200mm, 3mm plywood):
  12 recessed slots, each slot has an NTAG213 sticker wired via flat cable
  to a multiplexed antenna array — OR single MFRC522 scans one tag at a time
  (player places one piece → holds it → scans → places next)
  Correct order: [7,2,11,4,9,1,8,3,12,6,10,5]  (from zacus_v3_complete.yaml)
```

---

## P7 — Coffre Final (ESP32-DevKitC-32E)

### GPIO Table

| GPIO | Function             | Direction | Notes                                          |
|------|----------------------|-----------|------------------------------------------------|
| 4    | Keypad Row 1         | OUT       | Matrix scan — driven LOW one at a time         |
| 5    | WS2812B RGB LED      | OUT       | Status LED — idle/entering/error/open          |
| 6    | Keypad Row 2         | OUT       | Matrix scan                                    |
| 7    | Keypad Row 3         | OUT       | Matrix scan                                    |
| 8    | Keypad Row 4         | OUT       | Matrix scan                                    |
| 12   | Keypad Col 1         | IN_PULLUP | 10kΩ pull-up — reads HIGH unless row driven    |
| 13   | Keypad Col 2         | IN_PULLUP | 10kΩ pull-up                                   |
| 14   | Keypad Col 3         | IN_PULLUP | 10kΩ pull-up                                   |
| 18   | Buzzer               | OUT       | NPN 2N2222 base via 1kΩ                        |
| 21   | I2C SDA (OLED)       | BIDIR     | SSD1306 128x64 — 4.7kΩ pull-up                |
| 22   | I2C SCL (OLED)       | OUT       | SSD1306 — 4.7kΩ pull-up                       |
| 25   | SG90 Servo PWM       | OUT       | 50Hz PWM — 1ms=closed, 2ms=open               |

### Wiring Diagram

```
USB-C 5V ────┬─────────────────────────── SG90 Servo VCC (5V direct)
             │                       ┌── WS2812B VCC
             └── AMS1117-3.3 ── 3V3 ── ESP32 3V3

ESP32
  GPIO4,6,7,8 ──────────────────────── Keypad Rows R1..R4
    (each scanned LOW sequentially, others HIGH-Z)

  GPIO12 ──┬── 10kΩ ── 3V3 ────────── Keypad Col C1
  GPIO13 ──┬── 10kΩ ── 3V3 ────────── Keypad Col C2
  GPIO14 ──┬── 10kΩ ── 3V3 ────────── Keypad Col C3

  GPIO21 ──┬── 4.7kΩ ── 3V3 ─────── SSD1306 SDA
  GPIO22 ──┬── 4.7kΩ ── 3V3 ─────── SSD1306 SCL

  GPIO25 ─────────────────────────── SG90 Signal (orange wire)
    SG90 Red  ── 5V
    SG90 Brown ── GND

  GPIO5  ─────────────────────────── WS2812B DIN (1 LED)

  GPIO18 ── 1kΩ ── Q1(2N2222) base
               Q1 collector ── Buzzer+ ── 5V
               Q1 emitter   ── GND

Keypad 4x3 matrix layout:
  [1][2][3]
  [4][5][6]
  [7][8][9]
  [*][0][#]
  Rows: R1=top, R4=bottom
  Cols: C1=left, C3=right
```

---

## Hub — BOX-3 + RTC_PHONE

### BOX-3 (ESP32-S3-BOX-3)

All BOX-3 GPIO is managed by Espressif BSP. Key internal connections:

| Resource         | Notes                                              |
|------------------|----------------------------------------------------|
| ESP-NOW Master   | Coordinates all P1..P7 + RTC_PHONE via 2.4GHz     |
| Camera (GC0308)  | QR code scanning for P3                            |
| Display (ILI9342)| Game master dashboard, scoring, puzzle status      |
| I2S Speaker      | ES8311 audio codec — NPC voice output              |
| Mute button      | GPIO0 — NPC voice mute toggle                     |
| USB-C             | Programming + 5V power from Anker powerbank        |

### RTC_PHONE (Custom Board)

| GPIO/Pin | Function           | Notes                                    |
|----------|--------------------|------------------------------------------|
| GPIO4    | I2S BCLK → SLIC   | Si3220 voice codec clock                 |
| GPIO5    | I2S LRCLK → SLIC  | Si3220 left/right clock                  |
| GPIO6    | I2S DIN → SLIC    | Audio data to SLIC (NPC voice)           |
| GPIO7    | I2S DOUT ← SLIC   | Audio from handset (player speech)       |
| GPIO21   | I2C SDA → Si3220  | SLIC configuration                       |
| GPIO22   | I2C SCL → Si3220  | SLIC clock                               |
| GPIO18   | Hook switch detect | HIGH = handset lifted (interrupt)        |
| RJ11     | POTS line out      | Standard telephone handset connector     |
| USB-C    | 5V power           | From Anker powerbank                     |
