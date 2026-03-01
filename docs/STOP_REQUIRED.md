# STOP REQUIRED

Date: 2026-03-01 11:49 Europe/Paris

## Trigger condition
- Build/test regression not fixed quickly.

## What happened
- Firmware build succeeded for `freenove_esp32s3_full_with_ui`.
- Flash succeeded on `/dev/cu.usbmodem5AB90753301`.
- Serial validation failed at boot with repeated panic/reboot loop before scenario checks.

## Serial evidence
- `E (...) I2S: i2s_alloc_dma_buffer(741): Error malloc dma buffer`
- `E (...) I2S: i2s_driver_install(2027): I2S set clock failed`
- `Guru Meditation Error: Core 1 panic'ed (LoadProhibited)`
- Backtrace includes:
  - `Audio::Audio(...)` (`ESP32-audioI2S/src/Audio.cpp:231`)
  - `AudioManager::ensurePlayer()` (`hardware/firmware/ui_freenove_allinone/src/audio/audio_manager.cpp:395`)
  - `AudioManager::begin()` (`hardware/firmware/ui_freenove_allinone/src/audio/audio_manager.cpp:412`)
  - `setup()` (`hardware/firmware/ui_freenove_allinone/src/app/main.cpp:7212`)

## Impact
- Cannot complete runtime verification for `SCENE_GOTO SCENE_CREDIT` / `UI_SCENE_STATUS` because system reboots during startup.

## Requested next action
- Triage/fix audio boot allocation regression first, then rerun scene-credit serial validation.
