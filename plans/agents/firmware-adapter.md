# Agent Firmware Adapter

## Scope
- Runtime loading/execution path in `hardware/firmware/`, excluding the read-only mirror under `hardware/firmware/esp32/`.

## Responsibilities
- Adapt firmware to Runtime 3 artifacts.
- Keep hardware APIs aligned with the studio/runtime contracts.
- Resolve Freenove memory pressure while preserving the combined-board route.

## Current Tasks
- Restore green Freenove builds with memory headroom.
- Introduce a Runtime 3 loading path that can coexist with Story V2 during migration.
- Keep evidence in `hardware/firmware/docs/AGENT_TODO.md`.
- Advance from Runtime 3 metadata discovery to actual Runtime 3 execution on device.
