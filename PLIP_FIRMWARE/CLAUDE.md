# PLIP Firmware

ESP32 firmware for the retro telephone annex. Bringup target: ESP32-A1S
Audio Kit (ES8388 codec). End target: custom PCB with Si3210 SLIC.

See `README.md` for architecture, REST contract, pin map, and the
submodule conversion procedure when a remote URL is ready.

## Build

```bash
cd PLIP_FIRMWARE
pio run                  # compile
pio run -t upload        # flash via USB
pio device monitor       # serial @ 115200
```

## Source layout

| File | Role |
|------|------|
| `src/main.cpp` | `setup()` boots the three FreeRTOS tasks |
| `src/phone_task.cpp` | Off-hook GPIO interrupt + ring control |
| `src/audio_task.cpp` | ES8388 / Si3210 audio routing, drains a command queue |
| `src/network_task.cpp` | WiFi station + REST server (`/ring`, `/play`, `/stop`, `/status`) |

## Anti-patterns

- Hardcoding pin numbers in source — they live in `platformio.ini`
  `build_flags` so the dev kit ↔ PCB swap stays mechanical.
- Adding I2S/SPI calls outside `audio_task` or `phone_task` — codec
  access must stay single-threaded per peripheral.
- Skipping NVS for WiFi credentials in committed code. Compile-time
  defaults are OK for local bringup but never merge them.
- Implementing OTA before the bringup loop (REST `/ring` round-trip) is
  rock solid — chasing OTA bugs on top of audio bugs is misery.
