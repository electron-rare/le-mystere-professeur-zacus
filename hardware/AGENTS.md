# Hardware Agent Contract

Purpose: coordinate enclosure/wiring/firmware integration without changing licensing text.

Allowed scope:
- `hardware/bom/**`
- `hardware/enclosure/**`
- `hardware/wiring/**`
- `hardware/firmware/**` (firmware rules apply there)

Validate:
- `cd hardware/firmware && ./build_all.sh`
- `cd hardware/firmware && ./tools/dev/run_matrix_and_smoke.sh`

Common commands:
- `rg --files hardware`
- `rg -n "PORT|baud|UI_LINK_STATUS" hardware/firmware`

Do not:
- edit `hardware/firmware/esp32/` (read-only mirror for this repo)
- commit generated logs/artifacts
