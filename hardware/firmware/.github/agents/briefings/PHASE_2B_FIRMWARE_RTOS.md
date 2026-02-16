# PHASE 2B: Firmware/RTOS (WiFi + RTOS Health)

## 📌 Briefing: Firmware_RTOS_Agent

**Your mission:** Act as the firmware expert owner for Story V2: coordinate all firmware work, audits, reviews, and fixes. You own ESP32 WiFi stack stability, RTOS task health, and system resilience. This phase supports Phase 2/3 and runs in parallel with Phase 2.

**Prerequisites:**
- ✅ Firmware builds on esp32dev
- ✅ Serial console access

---

### ✅ Required Deliverables (Agent Management)

- Update test scripts related to WiFi/RTOS checks.
- Update any AI generation tooling if it depends on RTOS/WiFi behaviors.
- Update docs that describe WiFi/RTOS health checks and limits.
- Sync changes with the Test & Script Coordinator (cross-team coherence).
- Reference: [docs/TEST_SCRIPT_COORDINATOR.md](docs/TEST_SCRIPT_COORDINATOR.md)

### ⚠️ Watchouts (Audit)

- Fail fast on reset markers (`PANIC`, `REBOOT`, power-on reset) during Story V2 actions.
- Gate tests on `UI_LINK_STATUS connected==1` and log HELLO/ACK if available.
- Capture disconnect reasons and recovery time for AP loss.
- Include RTOS snapshots for multiple tasks (not only current task) where possible.

---

### 📌 RTOS Implementation Audit (2026-02-16)

**Summary**
- RTOS runtime tasks are wired into boot and controlled by config.
- `/api/rtos` exposes per-task snapshots when runtime is available.
- Serial `SYS_RTOS_STATUS` prints global snapshot plus per-task data.
- Watchdog support is enabled for RTOS tasks (configurable).

**Current RTOS runtime**
- Runtime class: `RadioRuntime` (FreeRTOS tasks pinned per core).
- Tasks: `TaskAudioEngine`, `TaskStreamNet`, `TaskStorageScan`, `TaskWebControl`, `TaskUiOrchestrator`.
- Update flow:
	- RTOS mode: tasks tick and call WiFi/web updates.
	- Cooperative mode: updates are done from the main loop via `RadioRuntime::updateCooperative`.

**Health telemetry**
- Global snapshot: heap, task count, current task stack watermark.
- Per-task snapshot: stack watermark, ticks, last tick time, core id.
- HTTP: `/api/status` (rtos block) and `/api/rtos` (detailed).
- Serial: `SYS_RTOS_STATUS`.

**Watchdog**
- RTOS tasks register to the ESP task watchdog when enabled.
- Each RTOS task resets the watchdog in its loop.
- Config:
	- `kEnableRadioRuntimeWdt`
	- `kRadioRuntimeWdtTimeoutSec`

**Gaps / Follow-ups**
- Run RTOS/WiFi health script to generate artifact (`artifacts/rtos_wifi_health_<timestamp>.log`).
- Validate WiFi reconnect + WebSocket recovery on hardware.
- Confirm task stack margins under real load.

---

### 📋 Tasks

#### Task 2B.1: WiFi Stability & Reconnect

- Verify connection lifecycle (boot, reconnect, AP loss).
- Detect and log disconnect reasons (Serial + optional metrics).
- Ensure HTTP + WebSocket survive reconnect without crashing.

**Acceptance Criteria:**
- ✅ Reconnect works after AP loss within 30s.
- ✅ No crash/reboot during reconnect loop.
- ✅ WebSocket recovers after reconnect (client reconnect OK).

---

#### Task 2B.2: RTOS Task Health

- Audit Story V2 tasks: stack usage, priority, starvation risk.
- Add lightweight health snapshot (task count, free heap, min stack watermark).
- Ensure watchdog behavior is documented and stable.

**Acceptance Criteria:**
- ✅ No task starvation under normal load.
- ✅ Heap and stack watermarks logged on demand.
- ✅ Watchdog does not trigger under expected workloads.

---

#### Task 2B.3: Fault Handling & Recovery

- Define crash patterns (PANIC/Guru Meditation/REBOOT markers).
- Ensure logs contain enough context for post-mortem.
- Add a minimal recovery checklist for operators.

**Acceptance Criteria:**
- ✅ Crash markers reliably logged.
- ✅ Recovery steps documented.

---

#### Task 2B.4: Tests & Docs

- Add a small RTOS/WiFi health test script or checklist.
- Document WiFi/RTOS limits and expected behaviors.

**Acceptance Criteria:**
- ✅ Test artifact saved under artifacts/ (with timestamp).
- ✅ Docs updated with WiFi/RTOS health section.

---

### 🎯 Deliverables

**On completion, provide:**
1. ✅ Commit hash for RTOS/WiFi changes
2. ✅ Test results artifact (WiFi/RTOS health)
3. ✅ Updated docs reference(s)

**Report to Coordination Hub:**
```
**Phase 2B Complete**
- ✅ WiFi stability + reconnect verified
- ✅ RTOS task health verified
- ✅ Crash markers + recovery documented
- ✅ Code committed: [commit hash]
- 📁 Artifacts: artifacts/rtos_wifi_health_[timestamp].log
- 🎯 Next: Phase 2/3 validation unblocked
```
