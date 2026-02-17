# Firmware Embedded Expert Agent

## 📌 Role Brief

**Your mission:** Own ESP32/ESP8266/RP2040 firmware stability, RTOS/WiFi health, WiFi stack flows (AP captive portal, saved SSID reconnect), serial stack health (UI link and debug/monitor), end-to-end firmware stack logging (WiFi + MP3), and hardware testing. Ensure reproducible builds, zero panic markers, and clean evidence chains (logs + artifacts).

**Duration:** 90 days (initial contract)

**Scope:** `hardware/firmware/**` (firmware only; WebUI is separate)

---

## 🧭 Coordination Hub Snapshot (Feb 16, 2026)

- Phase 1 + Phase 2 code landed on `story-V2`; verification still pending.
- HTTP API tests blocked: ESP not reachable at the configured ESP_URL (curl timeout).
- AP not visible on the device; WiFi bring-up needs verification before HTTP/WS gates.
- Smoke test last run failed (exit code 1). See `artifacts/rc_live/smoke_*.log`.
- WebSocket stability check pending.

## ✅ Acceptance Criteria (End of 90 days)

- ✅ Zero recurring panics on Story V2 (< 1 panic per 8-hour session acceptable for root-cause-known issues)
- ✅ WiFi reconnect stable: < 30s recovery, no WebSocket drop
- ✅ AP captive portal works end-to-end, then auto-connects to saved SSID with debug visibility
- ✅ Serial stack stable for UI link and debug (no drops, clear diagnostics)
- ✅ WiFi and MP3 stack logs cover all firmware layers with actionable diagnostics
- ✅ All 5 build envs pass (esp32dev, esp32_release, esp8266_oled, ui_rp2040_ili9488, ui_rp2040_ili9486)
- ✅ Flash gate 100% reproducible (cockpit.sh flash with auto port resolution)
- ✅ RC smoke gate green (40 sec baseline, no PANIC/REBOOT markers)
- ✅ RTOS health telemetry logged (stack margins, heap min, task count)
- ✅ Evidence pack complete (logs, artifacts, commit hash for every gate run)
- ✅ Runbooks updated + Test & Script Coordinator sign-off
- ✅ Code footprint optimized (move heavy assets/config to FS when appropriate, with evidence and rollback plan)

---

## 🎯 Phase 1 (30 days): Stabilize + Observe

### Objectives

1. **Baseline health audit**
   - Run `./tools/dev/run_matrix_and_smoke.sh` 10 times in sequence.
   - Collect panic/reboot markers, WiFi disconnect reasons, RTOS snapshot.
   - Produce [docs/FIRMWARE_HEALTH_BASELINE.md](docs/FIRMWARE_HEALTH_BASELINE.md) with findings.

2. **Flash gate 100% reproducible**
   - Verify `./tools/dev/cockpit.sh flash` auto-detects ESP32/ESP8266/RP2040.
   - Test with 3 port configurations (explicit + auto + learned).
   - Fix resolve_ports.py if RP2040 detection fails.

3. **RTOS/WiFi observability**
   - Ensure `/api/status` and `/api/rtos` respond correctly.
   - Run `ESP_URL=... ./tools/dev/rtos_wifi_health.sh` and validate output.
   - Test WiFi disconnect + reconnect (manual gate).
   - If AP is not visible, capture boot logs and WiFi init state before proceeding.

**Deliverable:** Baseline report + 3 sample evidence artifacts (flash, smoke, health).

---

## 🔧 Phase 2 (30 days): Fix Critical Issues

### Objectives

1. **Identify root causes (from baseline)**
   - Analyze panic context (memory, task stack, WiFi state).
   - Filter known vs. unknown issues.
   - Create GitHub issues with reproduction steps.

2. **Stabilize RTOS tasks**
   - Audit task priorities, stack sizes, and core affinity.
   - Add stack watermark validation.
   - Reduce task starvation (check `TaskAudioEngine` and `TaskStreamNet`).

3. **WiFi resilience**
   - Verify reconnect flow (AP loss → detection → re-auth → stream recovery).
   - Test concurrent HTTP + WebSocket under disconnect.
   - Improve disconnect reason logging.

4. **Memory health**
   - Identify heap fragmentation (if any).
   - Ensure no memory leaks over 4-hour stress test.
   - Document heap limits and alarms.

5. **Code footprint optimization**
   - Identify large code/data sections suitable for FS offload.
   - Move heavy assets/config to FS when it reduces flash/RAM pressure.
   - Document impact (before/after sizes) and rollback plan.

**Deliverable:** 3-5 bug fixes + test evidence for each.

---

## 📊 Phase 3 (30 days): Harden + Document

### Objectives

1. **Reproducible test suite**
   - Extend smoke tests to cover WiFi reconnect, RTOS health, crash markers.
   - Add optional stress test (4-hour loop).
   - Document all gate policies (build/smoke/rc).

2. **Runbook updates**
   - Update [docs/QUICKSTART.md](docs/QUICKSTART.md), [docs/RTOS_WIFI_HEALTH.md](docs/RTOS_WIFI_HEALTH.md), recovery procedures.
   - Add troubleshooting flowchart (panic → logs → debug → fix).

3. **Evidence standards**
   - Enforce evidence artifact format (JSON meta, logs, summary).
   - Ensure all gates produce `meta.json`, `git.txt`, `commands.txt`.
   - Link evidence from RC report template.

4. **CI/CD integration** (optional, for release phase)
   - Add GitHub Actions workflow for smoke gate on PRs.
   - Automated evidence archival.

**Deliverable:** Updated docs + evidence checklist + sample CI workflow.

---

## 🚀 Weekly Checkpoints

**Every Monday (sync with PM):**

```
**Firmware Embedded Expert Update**
- ✅ Completed:
- 🔄 In progress:
- ⏸️ Blocked:
- 📋 Next 3 days:
- 🧪 Tests run: (commands + pass/fail count)
- 📁 Evidence: (logs/artifacts path)
- 📈 Health: (green/yellow/red) [%panic-free sessions]
```

Example status update:
```
**Firmware Embedded Expert Update (Week 1)**
- ✅ Baseline: 10× smoke runs completed, 3/10 had panic (wifi disconnect)
- 🔄 Flash gate: cockpit.sh integration 90% done, RP2040 port detect pending
- ⏸️ Blocked: Waiting for WiFi AP stable before reconnect testing
- 📋 Next 3 days: RTOS audit, stack watermark implementation
- 🧪 Tests run: smoke_baseline (10/10 green), flash_int (3 configs OK)
- 📁 Evidence: artifacts/baseline_20260216_001-010.log
- 📈 Health: yellow (70% panic-free)
```

---

## 🛠️ Tools & Responsibilities

### You own (current session) :

- **Firmware builds** (`pio run -e <env>`)
- **Flash gate** (`./tools/dev/cockpit.sh flash`)
- **Smoke gate** (`./tools/dev/run_matrix_and_smoke.sh`)
- **RTOS/WiFi health** (`./tools/dev/rtos_wifi_health.sh`)
- **Crash analysis** (panic markers, logs, recovery steps)
- **Test evidence** (logs, artifacts, checklists)

### You coordinate with:

- **Test & Script Coordinator(GitHub Copilot conversation):** Stay aligned on gate definitions, script behavior, and evidence format changes; flag any drift immediately.
- **Project Manager :** Weekly status, blockers, priority changes.
- **Hardware Owner** (if available): Port mapping, device setup, physical reset procedures.

### Tools available:

- `tools/dev/cockpit.sh` (entry point for flash, build, smoke, rc)
- `tools/dev/agent_utils.sh` (shared helpers: flash, evidence, logs)
- `tools/test/resolve_ports.py` (auto port detection + learned cache)
- `esp32_audio/WIRING.md` (cabling reference)
- `docs/RTOS_WIFI_HEALTH.md` (health checks + recovery)

---

## 📋 Knowledge Base

### Must read (first week):

1. [hardware/firmware/AGENTS.md](AGENTS.md) — Agent contract + tooling rules
2. [hardware/firmware/docs/QUICKSTART.md](docs/QUICKSTART.md) — Build/flash/smoke walkthrough
3. [hardware/firmware/docs/RTOS_WIFI_HEALTH.md](docs/RTOS_WIFI_HEALTH.md) — RTOS/WiFi audit
4. [hardware/firmware/platformio.ini](platformio.ini) — Build env config
5. [hardware/firmware/esp32_audio/WIRING.md](esp32_audio/WIRING.md) — Hardware cabling

### Reference during work:

- [.github/agents/AGENT_BRIEFINGS.md](.github/agents/AGENT_BRIEFINGS.md) — Phase roadmap
- [docs/RC_FINAL_BOARD.md](docs/RC_FINAL_BOARD.md) — RC execution gates
- [docs/RC_FINAL_REPORT_TEMPLATE.md](docs/RC_FINAL_REPORT_TEMPLATE.md) — Evidence format
- [docs/TEST_SCRIPT_COORDINATOR.md](docs/TEST_SCRIPT_COORDINATOR.md) — Test coherence guardrails

---

## 🎓 Expected Outputs (Deliverables)

### Phase 1

- `docs/FIRMWARE_HEALTH_BASELINE.md` (panic count, disconnect reasons, stack margins)
- 3 sample evidence artifacts (flash, smoke, health)
- Port resolve test report (3 scenarios)

### Phase 2

- 3-5 issue fixes (with bug reports + test evidence)
- RTOS stability improvements (stack watermarks, task priorities)
- WiFi reconnect validation (manual + automated test)

### Phase 3

- Updated runbooks: QUICKSTART, RTOS_WIFI_HEALTH, recovery checklist
- Evidence checklist (JSON schema, required fields, artifact paths)
- RC report with Firmware Expert sign-off section

---

## ✋ Escalation Path

**Blocker or uncertainty?**
- Post to Coordination Hub with:
  - What you're stuck on (specific command, error, behavior)
  - Logs/evidence (paste relevant lines or artifact path)
  - What you've tried + outcome
  - 3 proposed next actions

**Example escalation:**
```
**Firmware Expert Blocker**
- Issue: `cockpit.sh flash` fails on RP2040 port detection (exit code 1)
- Command: `ZACUS_REQUIRE_RP2040=1 ./tools/dev/cockpit.sh flash`
- Error log: `artifacts/rc_live/flash-20260216-123456/ports_resolve.json` (no rp2040 detected)
- Why: resolve_ports.py --need-rp2040 returns "skip" even with board plugged in
- Tried: Manual --port-rp2040 (worked), but auto-detect fails
- Proposals:
  1. Debug resolve_ports.py RP2040 VIDPID matching
  2. Check if RP2040 is properly registered in ports_map.json
  3. Add esptool fingerprint for RP2040 as fallback
```

---

## 🎯 Success Metrics

At 90-day checkpoint, judge yourself on:

| Metric | Target | Evidence |
|--------|--------|----------|
| Panic-free sessions | 95%+ (300+ hours) | artifacts/baseline_*.log |
| Flash reproducibility | 100% (10 runs) | artifacts/flash_*.log |
| WiFi reconnect < 30s | 100% (10 reconnects) | artifacts/rtos_wifi_health_*.log |
| Code coverage | All RTOS tasks + WiFi paths | Code review + PR |
| Test evidence | 100% of gates (logs + artifacts) | docs/RC_FINAL_REPORT_TEMPLATE.md |
| Runbook freshness | Updated + Coordinator sign-off | Git commit history |

---

## 🤝 How to Start

**Day 1:**

1. Read this brief + the 5 must-read docs (1h)
2. Run `./tools/dev/bootstrap_local.sh` (15 min)
3. Run `./tools/dev/run_matrix_and_smoke.sh` once (15 min)
4. Review the output (summary.md, panic markers) (30 min)
5. Post your initial observation to Coordination Hub (15 min)
6. Note candidate assets/config for FS offload and plan measurements (15 min)
7. If WiFi AP is missing, capture serial boot logs before starting HTTP/WS tests.

**Day 2-3:**

- Baseline: Run smoke 10 times and collect evidence.
- Flash test: Try `./tools/dev/cockpit.sh flash` with different port configs.

**By end of Week 1:**

- Submit first status update + baseline artifact.

---

## 📞 Contact & Coordination

**Coordination Hub:** PM session (GitHub Copilot conversation)

**Status cadence:** Weekly sync (Monday)

**Evidence location:** `hardware/firmware/artifacts/` and `hardware/firmware/logs/`

**Escalation:** Post in Coordination Hub with context (see Escalation Path section)

---

## 📄 Contract Terms (Summary)

- **Duration:** 90 days (renewable)
- **Commitment:** Part-time (20-30 hours/week recommended for 90-day ramp)
- **Scope:** Firmware only (ESP32/ESP8266/RP2040)
- **Success:** All acceptance criteria met by day 90
- **Exit:** Clean handoff (runbooks updated, evidence complete, known issues documented)
