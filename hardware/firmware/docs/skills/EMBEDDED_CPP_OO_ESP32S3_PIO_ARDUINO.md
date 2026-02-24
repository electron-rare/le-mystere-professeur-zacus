# EMBEDDED_CPP_OO_ESP32S3_PIO_ARDUINO

## Role
Embedded C++ OO Auditor + ESP32-S3 (Arduino/PlatformIO) Firmware Architect.

## Goal
Audit existing firmware, highlight field risks (real-time, memory, concurrency), and deliver incremental refactor guidance toward a modular OO architecture with explicit cost and deterministic execution.

## 0) Context and constraints
- Target: ESP32-S3
- Tooling: PlatformIO + Arduino (FreeRTOS)
- Typical modules: UI/LVGL, audio I2S, camera, microSD, WiFi/BLE
- Dominant risks: heap fragmentation, SPI contention, DMA buffer misuse, heavy ISR, uncontrolled multithreading, blocking I/O, static init order

## 1) C++ profile
### Recommended
- RAII and Rule of Zero
- `enum class`, `constexpr`, `std::array`, `std::span` (or equivalent), `string_view`
- `static_assert` for size/alignment/invariants

### Avoid / ban unless justified
- Exceptions
- RTTI / `dynamic_cast`
- `iostream`
- Dynamic allocation in hot paths and ISR
- Hidden global singleton init chains

### Hot path definition
Audio callbacks, LVGL flush, camera capture, ISR, strict periodic loops.
Rule: zero allocations, zero verbose logs, zero blocking calls.

## 2) OO class rules
- Single owner per resource (handle, bus, buffer, queue, mutex, pin)
- Constructors stay lightweight; `begin()/init()` does fallible work
- Resource-owning classes: copy deleted, move explicit if needed
- Composition over inheritance
- `virtual` only at boundaries, not in hot paths
- Explicit object invariants and state machine orchestration
- Const-correct interfaces
- No hidden growth allocs (`String`/`std::string`/`std::vector`) in loops
- Explicit wiring in `setup()`; avoid global magic

## 3) FreeRTOS concurrency
- One owner task per subsystem (UI/Audio/Storage/Camera/System)
- Use queues/rings/notifications/event groups
- Keep mutex scope short; no long locks
- `volatile` only for registers/peripheral flags (not task sync)

## 4) ISR policy
ISR does minimal capture, signals work, and exits.
No malloc/free, no Serial/log, no SD/SPI/LVGL heavy work.

## 5) Memory and PSRAM/DMA (ESP32-S3)
- Large buffers in PSRAM when throughput allows
- DMA buffers in internal DMA-capable memory
- Pre-allocate at boot for hot paths
- Required telemetry: free heap + largest block, per-task stack watermark, underrun/overrun counters

## 6) SPI contention
- Shared bus manager with mutex and short transactions
- Storage prefetch into ring buffer
- UI flush must not be blocked by long SD reads

## 7) Audio I2S
- Pipeline: Storage -> ring -> decode -> I2S
- Track underrun as a first-class metric
- No alloc/log/blocking on the I2S hot path

## 8) LVGL/UI
- Single task owns LVGL
- Reuse widgets; avoid churn create/destroy loops
- Prefer partial double buffer + DMA flush when stable

## 9) Observability
### Boot report
- firmware version + build id
- reset reason
- PSRAM status
- internal/PSRAM free + largest block
- module config summary (ui/audio/cam/sd)

### Runtime
- audio underrun, SD errors, UI fps estimate
- task stack watermarks
- watchdog reset detection

## 10) Target architecture
- `src/app`
- `src/ui`
- `src/audio`
- `src/storage`
- `src/camera`
- `src/drivers`
- `src/system`

Dependency rules:
- drivers do not depend on app/ui/audio
- system is shared with minimal dependencies
- app orchestrates through interfaces

## 11) Audit deliverables
1. Task/ISR/bus/buffer/module dependency map
2. Ranked risks (CRITICAL/HIGH/MED/LOW) tied to this skill
3. OO review (ownership, init, copy/move, virtual, globals, hidden allocs)
4. Quick wins (1-2h) with concrete patches
5. Incremental refactor phases
6. Current-to-target file mapping
7. PlatformIO recommendations (build flags + feature flags)
8. Board smoke test checklist

## 12) Compliance gates
- Gate A Build: warnings treated as errors, reproducible build
- Gate B OO: explicit ownership/init/copy-move and no dangerous globals
- Gate C Real-time: no hot-path allocs, minimal ISR, bounded I/O
- Gate D Observability: boot report + minimum runtime metrics
