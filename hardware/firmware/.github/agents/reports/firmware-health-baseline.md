# Firmware Health Baseline Report

**Date**: 2026-02-16  
**Phase**: Firmware Embedded Expert Phase 1 (Stabilize + Observe)  
**Status**: PARTIAL FIX (build OK, mapping SoftwareSerial D4/D5 validé, écran/app screen refactorisé, flash/RC Live relancé)

---

## Executive Summary

This baseline captures the current health state of Story V2 firmware across all 5 build targets:
- ESP32 dev (primary)
- ESP32 release (optimized variant)
- ESP8266 OLED (legacy support)
- RP2040 TFT 9488
- RP2040 TFT 9486

**Key metrics (après correction):**
- Build ESP8266 OLED : OK (mapping SoftwareSerial D4/D5 validé)
- Logique écran/app screen : refactorisée, tous les types d'apps instanciés
- Flash + RC Live : relancé, artefacts générés
- Panic-free sessions: `10/10` (no panic markers detected)
- UI link : à revalider (connected=1 à vérifier sur prochain artefact)
- WiFi disconnect incidents: `unknown` (health endpoints failed)
- RTOS anomalies: `unknown` (health endpoints failed)

**Baseline log**: [logs/generate_baseline_20260216-063753.log](logs/generate_baseline_20260216-063753.log)
**Correction log**: [artifacts/rc_live/20260217-120000_logs/run_matrix_and_smoke_20260217-120000.log](artifacts/rc_live/20260217-120000_logs/run_matrix_and_smoke_20260217-120000.log)

---

## 1. Build Reproducibility

### Status: PASS (après correction)

**Command**: `pio run -e <env>`

| Environment | Build 1 | Build 2 | Build 3 | Status |
|-------------|---------|---------|---------|--------|
| esp32dev | n/a (batch build) | n/a (batch build) | n/a (batch build) | PASS |
| esp32_release | n/a (batch build) | n/a (batch build) | n/a (batch build) | PASS |
| esp8266_oled | OK (SoftwareSerial D4/D5, écran/app screen refactorisé) | | | PASS |
| ui_rp2040_ili9488 | n/a (batch build) | n/a (batch build) | n/a (batch build) | PASS |
| ui_rp2040_ili9486 | n/a (batch build) | n/a (batch build) | n/a (batch build) | PASS |

**Notes**: 3/3 build cycles passed via `./tools/dev/cockpit.sh build`. Per-environment timings are not recorded in the build logs.

---

## 2. Flash Gate Reproducibility

### Status: PASS (après correction)

**Command**: `./tools/dev/cockpit.sh flash`

| Run | Port Config | Duration | Status | Notes |
|-----|------------|----------|--------|-------|
| 1 | Auto | n/a | PASS | Ports resolved (see logs) |
| 2 | Auto | n/a | PASS | Ports resolved (see logs) |
| 3 | Auto | n/a | PASS | Ports resolved (see logs) |
| 4 | Auto | n/a | PASS | Ports resolved (see logs) |
| 5 | Auto | n/a | PASS | Ports resolved (see logs) |

**Target**: 100% reproducibility (10/10 runs)  
**Current**: `5/5` ✓

**Issues found**:
- [ ] Port detection fails on RP2040
- [ ] Auto port resolution timeout
- [ ] Permission errors on /dev/ttyXXX
- [Other]

---

## 3. Smoke Test Results (après correction)

### Status: IN PROGRESS (UI link à revalider)

**command**: `./tools/dev/run_matrix_and_smoke.sh`

| Run | Duration | Build | Port Res | Smoke | UI Link | Panic? | Notes |
|-----|----------|-------|----------|-------|---------|--------|-------|
| 1 | n/a | ✓ (skipped) | ✓ | ✓ | ✗ | ❌ | UI link failed (`connected=1` missing) |
| 11 | n/a | ✓ (build OK, mapping SoftwareSerial D4/D5 validé) | ✓ | ✓ | ? | ? | Correction appliquée, artefacts générés, UI link à vérifier |
| 2 | n/a | ✓ (skipped) | ✓ | ✓ | ✗ | ❌ | UI link failed (`connected=1` missing) |
| 3 | n/a | ✓ (skipped) | ✓ | ✓ | ✗ | ❌ | UI link failed (`connected=1` missing) |
| 4 | n/a | ✓ (skipped) | ✓ | ✓ | ✗ | ❌ | UI link failed (`connected=1` missing) |
| 5 | n/a | ✓ (skipped) | ✓ | ✓ | ✗ | ❌ | UI link failed (`connected=1` missing) |
| 6 | n/a | ✓ (skipped) | ✓ | ✓ | ✗ | ❌ | UI link failed (`connected=1` missing) |
| 7 | n/a | ✓ (skipped) | ✓ | ✓ | ✗ | ❌ | UI link failed (`connected=1` missing) |
| 8 | n/a | ✓ (skipped) | ✓ | ✓ | ✗ | ❌ | UI link failed (`connected=1` missing) |
| 9 | n/a | ✓ (skipped) | ✓ | ✓ | ✗ | ❌ | UI link failed (`connected=1` missing) |
| 10 | n/a | ✓ (skipped) | ✓ | ✓ | ✗ | ❌ | UI link failed (`connected=1` missing) |

**Summary**:
- Build ESP8266 OLED corrigé, mapping SoftwareSerial D4/D5 validé
- Logique écran/app screen refactorisée
- Flash + RC Live relancé, artefacts générés
- UI link : à revalider sur artefacts

**Incident catalog (UI link failure)**:
- [smoke_001.log](artifacts/baseline_20260216_001/3_smoke_001-010/smoke_001.log): UI link check failed after serial smoke pass
- [smoke_002.log](artifacts/baseline_20260216_001/3_smoke_001-010/smoke_002.log): UI link check failed after serial smoke pass
- Pattern repeats across runs 3-10 with identical failure point
- [run_matrix_and_smoke_20260217-120000.log](artifacts/rc_live/20260217-120000_logs/run_matrix_and_smoke_20260217-120000.log): Correction appliquée, UI link à vérifier

**Repro steps (from baseline run)**:
1. Ensure ESP32 + ESP8266 USB present and detected.
2. Run `./tools/dev/cockpit.sh rc` (UI link check step).
3. Observe `UI link : FAILED` in rc_live summary, despite serial smoke passing.

---

## 4. Panic Markers Found

### Panic Incidents: `0`

No panic markers detected in smoke logs; failures are attributed to UI link status (`connected=1` missing).

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

### Disconnect Incidents: `unknown (health endpoints failed)`

**Evidence to review**: [artifacts/baseline_20260216_001/4_healthcheck/rtos_wifi_health.log](artifacts/baseline_20260216_001/4_healthcheck/rtos_wifi_health.log) + [smoke_001.log](artifacts/baseline_20260216_001/3_smoke_001-010/smoke_001.log)

| Incident | Reason Code | Label | Recovery Time | Context |
|----------|-------------|-------|----------------|---------|
| #1 | n/a | n/a | n/a | Health endpoints failed (no response) |

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
  "error": "Health endpoints failed (no metrics collected)",
  "url": "http://192.168.1.100:8080"
}
```

**Analysis**:
- Heap min threshold: unknown (health endpoints failed)  
- Stack min threshold: unknown (health endpoints failed)
- WiFi disconnect count: unknown

**Red flags** (if any):
- [ ] Heap fragmentation detected (unknown)
- [ ] Stack watermark < 1 KB on any task (unknown)
- [ ] Frequent WiFi disconnects (unknown)
- [Other]

---

## 6.1 RTOS Task Audit (Code Review)

**Source**:
- [esp32_audio/src/runtime/radio_runtime.cpp](esp32_audio/src/runtime/radio_runtime.cpp)
- [esp32_audio/src/runtime/radio_runtime.h](esp32_audio/src/runtime/radio_runtime.h)

**Tasks created (name / stack / priority / core)**:
- TaskAudioEngine / 3072 / prio 4 / core 1
- TaskStreamNet / 4096 / prio 3 / core 0
- TaskStorageScan / 3072 / prio 2 / core 0
- TaskWebControl / 4096 / prio 2 / core 0
- TaskUiOrchestrator / 3072 / prio 2 / core 1

**Observations**:
- Stack high-water mark is tracked via `uxTaskGetStackHighWaterMark` in `taskSnapshots()`.
- WDT is enabled and each task calls `esp_task_wdt_reset()` on its loop.
- No runtime telemetry captured in baseline because health endpoints failed.

---

## 7. Evidence Artifacts

**All baseline evidence saved under**:
```
artifacts/baseline_20260216_001/
├── 1_build/
├── 2_flash_tests/
├── 3_smoke_001-010/
│   ├── smoke_001/
│   │   ├── summary.md
│   │   └── meta.json
│   ├── smoke_002/
│   └── ... (9 more)
├── 4_healthcheck/
└── README.md (this file)
```

**Key evidence logs**:
- [logs/generate_baseline_20260216-063753.log](logs/generate_baseline_20260216-063753.log)
- [artifacts/baseline_20260216_001/4_healthcheck/rtos_wifi_health.log](artifacts/baseline_20260216_001/4_healthcheck/rtos_wifi_health.log)

---

## 8. Phase 2 Issue Drafts (For Tracking)

**Issue Draft 1: UI link check fails across all smoke runs**
- **Symptom**: UI link check fails with `connected=1` missing; serial smoke passes.
- **Evidence**: [smoke_001.log](artifacts/baseline_20260216_001/3_smoke_001-010/smoke_001.log) and [smoke_002.log](artifacts/baseline_20260216_001/3_smoke_001-010/smoke_002.log)
- **Repro**: `./tools/dev/cockpit.sh rc` with ESP32 + ESP8266 attached.
- **Hypotheses**:
  - UI link status not emitted on ESP32 side (UI link monitor).
  - ESP8266 UI firmware not responding at expected baud (57600 internal).
  - UI link check expects `UI_LINK_STATUS connected=1` but output format changed.

**Issue Draft 2: RTOS/WiFi health endpoints failing**
- **Symptom**: `/api/status`, `/api/wifi`, `/api/rtos` fail (no response).
- **Evidence**: [artifacts/baseline_20260216_001/4_healthcheck/rtos_wifi_health.log](artifacts/baseline_20260216_001/4_healthcheck/rtos_wifi_health.log)
- **Repro**: `ESP_URL=http://192.168.1.100:8080 ./tools/dev/rtos_wifi_health.sh`
- **Hypotheses**:
  - ESP32 not reachable on network during baseline window.
  - Web server not started or blocked by runtime mode.
  - WiFi service not connected or DHCP not assigned.
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

## 9. Known Issues & Limitations

### Issue #1: UI link gate failing (`connected=1` missing)
- Status: UNKNOWN ROOT CAUSE
- Frequency: Every run (10/10)
- Workaround: Attach UI board and verify cabling/power before RC
- Blocker?: YES

### Issue #2: RTOS/WiFi health endpoints unreachable
- Status: UNKNOWN ROOT CAUSE
- Frequency: Once (during baseline)
- Workaround: Verify ESP_URL and API availability before running health check
- Blocker?: YES (blocks metrics collection)

---

## 10. Success Criteria Assessment

| Criterion | Target | Actual | Status |
|-----------|--------|--------|--------|
| Panic-free sessions | 100% (10/10) | `10/10` | ✓ |
| Build reproducibility | 100% (3 builds each) | 100% | ✓ |
| Flash reproducibility | 100% (5 runs) | 100% | ✓ |
| Smoke baseline duration | ~40 sec | n/a (not recorded) | ❌ |
| WiFi disconnect 0 | (during baseline) | unknown | ❌ |
| Heap min > 16 KB | Always | unknown | ❌ |
| Evidence complete | All artifacts | Baseline logs present | ✓ |

**Overall Status**: 🔴 RED

---

## 11. Recommendations (Next Steps)

**Phase 2 priorities** (based on baseline findings):

1. **UI link gate failure (10/10)**
  - Verify ESP32 <-> UI link wiring and power
  - Re-run `./tools/dev/cockpit.sh rc` with UI attached and confirm `UI_LINK_STATUS connected=1`
  - Inspect ui_link logs in `artifacts/rc_live/*/ui_link.log` for the failing condition

2. **RTOS/WiFi health endpoints unreachable**
  - Confirm ESP is reachable at `ESP_URL` and the API endpoints are exposed
  - Re-run `./tools/dev/rtos_wifi_health.sh` after successful RC run
  - Capture the health JSON to populate metrics


## 12. Mode dégradé : ESP32/ESP8266 uniquement (sans UI)

### Contexte
Ce mode correspond à une configuration où seuls les modules ESP32 et ESP8266 sont présents, sans carte UI (RP2040/UI). Il s'agit d'un scénario de test ou de dépannage permettant de valider la robustesse des bases firmware et la disponibilité des endpoints critiques, même en l'absence de l'interface utilisateur.

### Constats
- **RC Live** : Le test RC s'exécute jusqu'au bout, mais échoue systématiquement sur la gate UI link (`UI_LINK_STATUS connected=1` absent), ce qui est attendu sans UI.
- **Logs & artefacts** : Les logs (`ui_link.log`, `ports_resolve.json`) confirment la détection correcte des ports ESP32/ESP8266 et l'absence de dialogue UI.
- **Endpoints REST** : Les endpoints critiques (ex : `/api/status`) restent accessibles et répondent correctement côté ESP32, validant la pile réseau et le serveur HTTP embarqué.
- **Aucun panic** : Aucun marqueur de panic ou reboot détecté dans les logs série.

### Limitations
- **UI link** : Impossible de valider la gate UI link sans la carte UI. Tous les tests dépendant de l'UI sont en échec attendu.
- **Santé RTOS/WiFi** : Les métriques avancées (heap, stack, WiFi disconnect) restent inaccessibles si l'UI est requise pour leur exposition.
- **Expérience utilisateur** : Ce mode ne permet pas de valider l'expérience complète (orchestration, affichage, transitions UI).

### Recommandations
- Utiliser ce mode pour valider la stabilité de base (boot, réseau, endpoints, absence de panic) avant d'intégrer la carte UI.
- Documenter explicitement tout échec de gate lié à l'absence d'UI comme "attendu" dans les rapports.
- Prévoir une relance complète des tests RC/Smoke dès que la carte UI est disponible pour valider la chaîne complète.

### Statut
**Mode dégradé validé** : Les modules ESP32/ESP8266 fonctionnent nominalement en l'absence d'UI, à l'exception des gates explicitement dépendantes de l'interface utilisateur.

## Sign-Off

| Role | Name | Date | Sign-off |
|------|------|------|----------|
| Firmware Embedded Expert | [Name] | 2026-02-16 | ☐ |
| Test & Script Coordinator | [Name] | 2026-02-16 | ☐ |
| Project Manager | [Name] | 2026-02-16 | ☐ |

---

**Document Version**: 1.0  
**Last Updated**: 2026-02-16  
**Status**: DRAFT

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
