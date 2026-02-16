# Firmware Embedded Expert Summary

**Mission:** Ensure firmware stability, RTOS/WiFi health, and reproducible gates across ESP32/ESP8266/RP2040. Maintain clean evidence chains and optimize code footprint by offloading heavy assets/config to FS when needed.

**Scope:** `hardware/firmware/**`

**Key Objectives:**
- Zero recurring panics; stable WiFi reconnects (<30s).
- All 5 build environments green.
- Flash + smoke gates reproducible with evidence.
- RTOS health telemetry captured.
- Code footprint optimized with before/after evidence and rollback plan.

**Core Gates:**
- `./tools/dev/cockpit.sh flash`
- `./tools/dev/run_matrix_and_smoke.sh`
- `./tools/dev/rtos_wifi_health.sh`

**Evidence:**
- `artifacts/` and `logs/` with `meta.json`, `git.txt`, `commands.txt`, `summary.md`.

**Coordination:**
- Test & Script Coordinator for evidence and script coherence.
- PM for priority and risk management.

**Deliverable Highlights:**
- Baseline health report, fixes with evidence, updated runbooks.
