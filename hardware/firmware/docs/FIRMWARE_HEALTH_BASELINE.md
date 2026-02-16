# Firmware Health Baseline Report

**Date**: [Date of baseline run]  
**Phase**: Firmware Embedded Expert Phase 1 (Stabilize + Observe)  
**Status**: [PASS / FAIL]

---

## Executive Summary

This baseline captures the current health state of Story V2 firmware across all 5 build targets:
- ESP32 dev (primary)
- ESP32 release (optimized variant)
- ESP8266 OLED (legacy support)
- RP2040 TFT 9488
- RP2040 TFT 9486

**Key metrics from 10 smoke runs:**
- Panic-free sessions: `[X]/10` (target: 10/10)
- Average smoke duration: `[XXX]` sec (baseline: ~40 sec)
- WiFi disconnect incidents: `[X]` (target: 0)
- RTOS anomalies: `[X]` (target: 0)

---

## 1. Build Reproducibility

### Status: [PASS / FAIL]

**Command**: `pio run -e <env>`

| Environment | Build 1 | Build 2 | Build 3 | Status |
|-------------|---------|---------|---------|--------|
| esp32dev | [time] | [time] | [time] | [PASS/FAIL] |
| esp32_release | [time] | [time] | [time] | [PASS/FAIL] |
| esp8266_oled | [time] | [time] | [time] | [PASS/FAIL] |
| ui_rp2040_ili9488 | [time] | [time] | [time] | [PASS/FAIL] |
| ui_rp2040_ili9486 | [time] | [time] | [time] | [PASS/FAIL] |

**Notes**: [Any build issues, variance, or anomalies]

---

## 2. Flash Gate Reproducibility

### Status: [PASS / FAIL]

**Command**: `./tools/dev/cockpit.sh flash`

| Run | Port Config | Duration | Status | Notes |
|-----|------------|----------|--------|-------|
| 1 | Auto | [time] | PASS | [Port detected: /dev/...] |
| 2 | Auto | [time] | PASS | [Port detected: /dev/...] |
| 3 | Auto | [time] | PASS | [Port detected: /dev/...] |
| 4 | Explicit | [time] | PASS | [Manually specified port] |
| 5 | Explicit | [time] | PASS | [Manually specified port] |

**Target**: 100% reproducibility (10/10 runs)  
**Current**: `[X]/10` ✓ or ❌

**Issues found**:
- [ ] Port detection fails on RP2040
- [ ] Auto port resolution timeout
- [ ] Permission errors on /dev/ttyXXX
- [Other]

---

## 3. Smoke Test Results (10 runs)

### Status: [PASS / FAIL]

**command**: `./tools/dev/run_matrix_and_smoke.sh`

| Run | Duration | Build | Port Res | Smoke | UI Link | Panic? | Notes |
|-----|----------|-------|----------|-------|---------|--------|-------|
| 1 | [sec] | ✓ | ✓ | ✓ | ✓ | ❌ | [Marker details if any] |
| 2 | [sec] | ✓ | ✓ | ✓ | ✓ | ❌ | |
| 3 | [sec] | ✓ | ✓ | ✓ | ✓ | ❌ | |
| 4 | [sec] | ✓ | ✓ | ✓ | ✓ | ❌ | |
| 5 | [sec] | ✓ | ✓ | ✓ | ✓ | ❌ | |
| 6 | [sec] | ✓ | ✓ | ✓ | ✓ | ✓ | Guru Meditation Error (line XYZ) |
| 7 | [sec] | ✓ | ✓ | ✓ | ✓ | ❌ | |
| 8 | [sec] | ✓ | ✓ | ✓ | ✓ | ❌ | |
| 9 | [sec] | ✓ | ✓ | ✓ | ✓ | ❌ | |
| 10 | [sec] | ✓ | ✓ | ✓ | ✓ | ❌ | |

**Summary**:
- Panic-free runs: `[X]/10`
- Build failures: `[X]/10`
- Port resolution failures: `[X]/10`
- Smoke test failures: `[X]/10`
- UI link failures: `[X]/10`

---

## 4. Panic Markers Found

### Panic Incidents: `[X]`

If any panics detected, document each:

**Panic #1**
- Run: #6
- Time: 2026-02-16T12:34:56Z
- Context: [Build step / Test phase]
- Marker: `Guru Meditation Error: Core X panic'ed`
- Artifact: `artifacts/rc_live/[timestamp]/smoke_esp32.log` (lines XYZ)
- Root cause (if known): [Memory, task stack, WiFi state, etc.]
- Reproducible?: YES / NO / UNKNOWN

**Panic #2**
[Repeat for each panic]

---

## 5. WiFi Disconnect Reasons

### Disconnect Incidents: `[X]`

**Command to review**: `artifacts/rc_live/*/ports_resolve.json` + serial output

| Incident | Reason Code | Label | Recovery Time | Context |
|----------|-------------|-------|----------------|---------|
| #1 | [code] | [e.g., "no auth"] | [time] | During smoke step X |
| #2 | [code] | [e.g., "auth fail"] | [time] | During smoke step Y |

**Known disconnect patterns**:
- [ ] Consistent at specific step (e.g., WebSocket init)
- [ ] Random / intermittent
- [ ] Related to AP reboot / reset
- [ ] Related to signal strength dropping
- [Other]

---

## 6. RTOS Health Snapshot

### Command: `./tools/dev/rtos_wifi_health.sh`

**Metrics from healthcheck**:
```json
{
  "heap": {
    "free_bytes": XXXX,
    "min_bytes": XXXX,
    "allocated_bytes": XXXX
  },
  "rtos": {
    "task_count": XX,
    "stack_min_words": XX,
    "stack_min_bytes": XX
  },
  "wifi": {
    "connected": true,
    "disconnect_count": X,
    "last_disconnect_reason": "..."
  }
}
```

**Analysis**:
- Heap min threshold: >= 16 KB (healthy)  
- Stack min threshold: >= 1024 bytes per task (healthy)
- WiFi disconnect count: [X] incidents during baseline

**Red flags** (if any):
- [ ] Heap fragmentation detected
- [ ] Stack watermark < 1 KB on any task
- [ ] Frequent WiFi disconnects
- [Other]

---

## 7. Evidence Artifacts

**All baseline evidence saved under**:
```
artifacts/baseline_20260216_001-010/
├── 1_build/
├── 2_flash_tests/
├── 3_smoke_001-010/
│   ├── smoke_001/
│   │   ├── summary.json
│   │   ├── run_matrix_and_smoke.log
│   │   ├── smoke_esp32.log
│   │   ├── build_esp32dev.log
│   │   └── ui_link.log
│   ├── smoke_002/
│   └── ... (9 more)
├── 4_healthcheck/
└── README.md (this file)
```

**How to regenerate baseline**:
```bash
cd hardware/firmware
mkdir -p artifacts/baseline_$(date +%Y%m%d)_001-010

# Build 3 times
for i in 1 2 3; do
  echo "[Build $i]"
  ./tools/dev/cockpit.sh build 2>&1 | tee artifacts/baseline_*/build_$i.log
done

# Flash tests
for i in 1 2 3 4 5; do
  echo "[Flash test $i]"
  ./tools/dev/cockpit.sh flash 2>&1 | tee artifacts/baseline_*/flash_$i.log
done

# Smoke 10 times
for i in {1..10}; do
  echo "[Smoke run $i]"
  ./tools/dev/cockpit.sh rc 2>&1 | tee artifacts/baseline_*/smoke_$(printf '%03d' $i).log
done

# Health check
ESP_URL=http://192.168.1.100:8080 ./tools/dev/rtos_wifi_health.sh
```

---

## 8. Known Issues & Limitations

### Issue #1: [Title]
- Status: KNOWN / UNKNOWN ROOT CAUSE
- Frequency: [Every run / Intermittent / Once]
- Workaround: [If any]
- Blocker?: [YES / NO]

### Issue #2:
[Repeat as needed]

---

## 9. Success Criteria Assessment

| Criterion | Target | Actual | Status |
|-----------|--------|--------|--------|
| Panic-free sessions | 100% (10/10) | `[X]/10` | ✓ / ❌ |
| Build reproducibility | 100% (3 builds each) | [X]% | ✓ / ❌ |
| Flash reproducibility | 100% (5 runs) | [X]% | ✓ / ❌ |
| Smoke baseline duration | ~40 sec | [XX] sec avg | ✓ / ❌ |
| WiFi disconnect 0 | (during baseline) | [X] incidents | ✓ / ❌ |
| Heap min > 16 KB | Always | [Y] KB (min) | ✓ / ❌ |
| Evidence complete | All artifacts | [Y]/14 present | ✓ / ❌ |

**Overall Status**: [🟢 GREEN / 🟡 YELLOW / 🔴 RED]

---

## 10. Recommendations (Next Steps)

**Phase 2 priorities** (based on baseline findings):

1. **[If panics found]**
   - Analyze panic context (memory state, task stacks, logs)
   - Create GitHub issues with reproduction steps
   - Implement monitoring / watchdog to prevent recurrence

2. **[If flash gate fails]**
   - Debug resolve_ports.py for failed platform
   - Add fallback port detection method
   - Update port mapping cache

3. **[If WiFi disconnect pattern]**
   - Test reconnect flow on hardware
   - Validate AP settings vs. device config
   - Improve disconnect reason logging

4. **[If memory issues]**
   - Add heap fragmentation analysis
   - Consider garbage collection strategy
   - Review task stack allocations

---

## Sign-Off

| Role | Name | Date | Sign-off |
|------|------|------|----------|
| Firmware Embedded Expert | [Name] | 2026-02-XX | ☑️ |
| Test & Script Coordinator | [Name] | 2026-02-XX | ☑️ |
| Project Manager | [Name] | 2026-02-XX | ☑️ |

---

**Document Version**: 1.0  
**Last Updated**: [Date]  
**Status**: [DRAFT / FINAL]

---

### Appendix: Evidence File Descriptions

- **summary.json**: Structured RC test results (exit_code, build_status, smoke_status, ui_link_status)
- **run_matrix_and_smoke.log**: Main test execution log (build + port resolution + smoke)
- **smoke_esp32.log**: ESP32 serial output during smoke tests
- **smoke_esp8266_usb.log**: ESP8266 serial output (USB variant)
- **build_esp32dev.log**: PlatformIO build output
- **ui_link.log**: UI link status checks (expected: "connected=1")
- **ports_resolve.json**: Auto-detected serial port mapping

---

**Need help?** See [docs/QUICKSTART.md](QUICKSTART.md) or [docs/RTOS_WIFI_HEALTH.md](RTOS_WIFI_HEALTH.md)
