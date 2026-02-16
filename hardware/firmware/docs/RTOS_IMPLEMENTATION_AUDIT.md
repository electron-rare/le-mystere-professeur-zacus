# RTOS Implementation Audit (Story V2)

Date: 2026-02-16
Scope: ESP32 RTOS runtime, WiFi/RTOS health, watchdog, API exposure.

## Summary
- RTOS runtime tasks are wired into boot and controlled by config.
- /api/rtos now exposes per-task snapshots when runtime is available.
- Serial SYS_RTOS_STATUS prints global snapshot plus per-task data.
- Watchdog support is enabled for RTOS tasks (configurable).

## Current RTOS Runtime
- Runtime class: RadioRuntime (FreeRTOS tasks pinned per core).
- Tasks: TaskAudioEngine, TaskStreamNet, TaskStorageScan, TaskWebControl, TaskUiOrchestrator.
- Update flow:
  - RTOS mode: tasks tick and call wifi/web updates.
  - Cooperative mode: updates are done from the main loop via RadioRuntime::updateCooperative.

## Health Telemetry
- Global snapshot: heap, task count, current task stack watermark.
- Per-task snapshot: stack watermark, ticks, last tick time, core id.
- HTTP: /api/status (rtos block) and /api/rtos (detailed).
- Serial: SYS_RTOS_STATUS.

## Watchdog
- RTOS tasks register to the ESP task watchdog when enabled.
- Each RTOS task resets the watchdog in its loop.
- Config:
  - kEnableRadioRuntimeWdt
  - kRadioRuntimeWdtTimeoutSec

## Gaps / Follow-ups
- Run RTOS/WiFi health script to generate artifact (artifacts/rtos_wifi_health_<timestamp>.log).
- Validate WiFi reconnect + WebSocket recovery on hardware.
- Confirm task stack margins under real load.

## References
- Runtime: esp32_audio/src/runtime/radio_runtime.{h,cpp}
- Orchestrator wiring: esp32_audio/src/app/app_orchestrator.cpp
- RTOS API: esp32_audio/src/services/web/web_ui_service.{h,cpp}
- Health doc: docs/RTOS_WIFI_HEALTH.md
