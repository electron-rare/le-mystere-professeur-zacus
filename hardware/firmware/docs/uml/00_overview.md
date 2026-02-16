# UML Overview

This firmware is a multi-MCU system with three independent firmwares linked by UART.

## System topology

```
ESP32 Audio Kit (main firmware)
  - App -> AppOrchestrator -> Controllers -> Services
  - UI Link v2 (UART 57600)

    |-> ESP8266 OLED UI (lightweight UI, 128x64)
    |-> RP2040 TFT UI (LVGL, touch)
```

## Responsibilities by MCU

- ESP32: audio, story logic, web control, UI link master
- ESP8266: small OLED render + button UI
- RP2040: TFT render + touch UI

## Key concepts

- Controllers orchestrate runtime modes (story, mp3, radio).
- Services hold business logic (audio, input, screen sync, web).
- Story Engine V2 is an event-driven state machine.
- UI Link v2 is a frame-based UART protocol with heartbeat.

## Main folders

- esp32_audio/src/controllers
- esp32_audio/src/services
- esp32_audio/src/story
- esp32_audio/src/ui_link
- ui/esp8266_oled
- ui/rp2040_tft
- protocol/ui_link_v2.h
