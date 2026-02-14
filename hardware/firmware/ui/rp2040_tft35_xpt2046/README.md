# RP2040 Touch UI Option (3.5" TFT + XPT2046)

UI tactile optionnelle (100% touch) pour piloter le lecteur ESP32 Audio Kit via UART JSONL.

## Scope
- Ecran 480x320 (3.5" SPI, ILI9488 ou ILI9486)
- Touch résistif XPT2046 (SPI partagé)
- Protocole UART JSONL avec ESP32
- 3 écrans: `LECTURE`, `LISTE`, `REGLAGES`

## Build / Flash (PlatformIO)
Depuis ce dossier:

```bash
pio run -e rp2040_ili9488
pio run -e rp2040_ili9488 -t upload --upload-port <PORT_RP2040>
```

Alternative dalle ILI9486:

```bash
pio run -e rp2040_ili9486
pio run -e rp2040_ili9486 -t upload --upload-port <PORT_RP2040>
```

Monitor:

```bash
pio device monitor -e rp2040_ili9488 --port <PORT_RP2040>
```

## First Boot
1. L’UI démarre en mode "Connecting...".
2. Calibration tactile 3 points si aucune calibration enregistrée.
3. Demande d’état automatique à l’ESP32 (`request_state`).

## ESP32 Integration
Activer côté ESP32 via flags compile:
- `UI_SERIAL_ENABLED=1`
- `UI_SERIAL_BAUD=115200`
- `UI_SERIAL_RX_PIN=18`
- `UI_SERIAL_TX_PIN=19`

Par défaut dans le firmware ESP32, le module reste **désactivé** (`UI_SERIAL_ENABLED=0`).

## Docs
- Câblage: [`WIRING.md`](./WIRING.md)
- Protocole: [`PROTOCOL.md`](./PROTOCOL.md)
- Spécification UI: [`UI_SPEC.md`](./UI_SPEC.md)
