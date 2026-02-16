# Footprint Optimization Report (2026-02-16)

## Scope
Reduce ESP32 firmware flash/RAM footprint by disabling unused audio codecs (AAC/FLAC/OPUS) while keeping MP3/WAV enabled.

## Changes
- Compile-time codec gates added in `config.h`.
- Decoder selection and FS audio detection now respect codec flags in:
  - `mp3_player.cpp`
  - `async_audio_service.cpp`

## Measurement (before/after)

### esp32dev
- Before (baseline build):
  - RAM: 124456 bytes (38.0%)
  - Flash: 1840897 bytes (87.8%)
  - Source: artifacts/baseline_20260216_001/1_build/build_1.log
- After (codec gating):
  - RAM: 121200 bytes (37.0%)
  - Flash: 1632681 bytes (77.9%)
  - Source: logs/agent_build.log
- Delta:
  - RAM: -3256 bytes
  - Flash: -208216 bytes

### esp32_release
- Before (baseline build):
  - RAM: 124456 bytes (38.0%)
  - Flash: 1841029 bytes (87.8%)
  - Source: artifacts/baseline_20260216_001/1_build/build_1.log
- After (codec gating):
  - RAM: 121200 bytes (37.0%)
  - Flash: 1632789 bytes (77.9%)
  - Source: logs/agent_build.log
- Delta:
  - RAM: -3256 bytes
  - Flash: -208240 bytes

### Other envs
No meaningful change detected in ui_rp2040_ili9488, ui_rp2040_ili9486, esp8266_oled.

## Impact
- Enabled codecs: MP3, WAV
- Disabled codecs by default: AAC, FLAC, OPUS
- Boot FX and SD/FS playback must use MP3/WAV to succeed.

## Rollback Plan
1) Re-enable codecs by setting build flags (example):
   -DUSON_ENABLE_CODEC_AAC=1 -DUSON_ENABLE_CODEC_FLAC=1 -DUSON_ENABLE_CODEC_OPUS=1
2) Rebuild and flash with cockpit:
   ./tools/dev/cockpit.sh build
   ./tools/dev/cockpit.sh flash

## Evidence
- Baseline build log: artifacts/baseline_20260216_001/1_build/build_1.log
- After build log: logs/agent_build.log
