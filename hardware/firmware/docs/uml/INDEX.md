# UML Docs Index

Human-readable UML documentation split by major blocks.

## Files

- [00_overview.md](00_overview.md) - System overview and key concepts
- [01_story_engine.md](01_story_engine.md) - Story Engine V2 core model
- [02_controllers.md](02_controllers.md) - Controllers layer
- [03_services.md](03_services.md) - Services layer
- [04_audio.md](04_audio.md) - Audio subsystem
- [05_ui_link.md](05_ui_link.md) - UI Link v2 protocol and classes
- [06_ui_esp8266.md](06_ui_esp8266.md) - ESP8266 OLED UI structure
- [07_ui_rp2040.md](07_ui_rp2040.md) - RP2040 TFT UI structure
- [08_sequences.md](08_sequences.md) - Sequence flows (boot, story, reconnection)

## Reading paths

- New dev: 00_overview -> 01_story_engine -> 02_controllers -> 03_services
- UI work: 05_ui_link -> 06_ui_esp8266 -> 07_ui_rp2040
- Audio work: 04_audio -> 02_controllers
- Release review: 00_overview -> 08_sequences
