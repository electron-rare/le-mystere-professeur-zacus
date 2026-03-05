# STOP REQUIRED

Date: 2026-03-05 15:17 Europe/Paris

## Status
- Resolved for the requested V2 scope.

## Resolution applied
- Adopted V2-only firmware gates:
  - `freenove_esp32s3`
  - `esp8266_oled`
- Updated gate contracts/scripts accordingly:
  - `AGENTS.md`
  - `hardware/firmware/AGENTS.md`
  - `hardware/firmware/build_all.sh`
  - `hardware/firmware/tools/dev/run_matrix_and_smoke.sh`
- Kept Story runtime compile scope narrowed for Freenove via `hardware/firmware/lib/story/library.json`.

## Verification
- `pio run -d hardware/firmware -e freenove_esp32s3 -e esp8266_oled` -> SUCCESS (2/2).

## Remaining limitations (out of V2 gate scope)
- `esp32dev` / `esp32_release`: missing legacy Story audio/service header contract (`audio/effects/audio_effect_id.h`).
- `ui_rp2040_ili9488` / `ui_rp2040_ili9486`: `input in flex scanner failed` while building `.pio.pio.h`.
