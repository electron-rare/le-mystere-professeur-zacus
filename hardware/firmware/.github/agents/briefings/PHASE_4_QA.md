# PHASE 4: QA Testing (E2E + Smoke + Stress)

## 📌 Briefing: QA_Agent

**Your mission:** Build comprehensive test suite for Story V2 covering E2E scenarios, smoke tests, and 4-hour stress tests. Integrate with CI (GitHub Actions). This phase depends on Phases 2-3 (API + WebUI stable).

**Prerequisites for this phase:**
- ✅ Phase 2 complete: 11 REST endpoints + WebSocket stable
- ✅ Phase 3 complete: WebUI Selector/Orchestrator/Designer functional
- ✅ Both layers integrated and ready for end-to-end testing

**Coordination Hub update (Feb 16, 2026):**
- Phase 4 assets/scripts exist but verification is pending.
- Do not mark Phase 4 complete until Phase 2 HTTP + WebSocket checks are green.

---

### ✅ Required Deliverables (Agent Management)

- Update smoke/E2E/stress scripts as needed.
- Update AI generation scripts if test data relies on them.
- Update testing docs (README/tests/protocoles).
- Sync changes with the Test & Script Coordinator (cross-team coherence).
- Reference: [docs/TEST_SCRIPT_COORDINATOR.md](docs/TEST_SCRIPT_COORDINATOR.md)

### ⚠️ Watchouts (Audit)

- Fail fast on reset markers (`PANIC`, `REBOOT`, power-on reset).
- Gate pass/fail on `UI_LINK_STATUS connected==1` unless explicitly waived.

### 📋 Tasks

#### Task 4.1: E2E Tests (Cypress / Playwright)

**What:** Test full user workflows from WebUI → API → Story Engine.

**Test suite: `esp32_audio/tests/e2e/`**

```javascript
describe('Story V2 E2E Tests', () => {
  beforeEach(() => {
    cy.visit('http://[ESP_IP]:8080/story-ui');
    cy.contains('Scenario Selector').should('be.visible');
  });

  it('should select and launch a scenario', () => {
    // Test: DEFAULT scenario
    cy.contains('DEFAULT').click();
    cy.contains('Play').click();
    cy.contains('LiveOrchestrator').should('be.visible');
    
    // Assert step display updates
    cy.contains('unlock_event', { timeout: 10000 }).should('be.visible');
  });

  it('should pause and resume execution', () => {
    // ... launch DEFAULT
    cy.contains('Pause').click();
    cy.contains('paused', { timeout: 2000 }).should('exist');
    
    cy.contains('Resume').click();
    cy.contains('running', { timeout: 2000 }).should('exist');
  });

  it('should skip to next step', () => {
    // ... launch DEFAULT
    const initialStep = cy.contains('[Step:').invoke('text');
    
    cy.contains('Skip').click();
    cy.contains('[Step:').invoke('text').should('not.equal', initialStep);
  });

  it('should complete a 4-scenario loop', () => {
    const scenarios = ['DEFAULT', 'EXPRESS', 'EXPRESS_DONE', 'SPECTRE'];
    
    scenarios.forEach((scenario) => {
      cy.contains(scenario).click();
      cy.contains('Play').click();
      
      // Wait for scenario to complete
      cy.contains('done', { timeout: 300000 }).should('exist');
      
      // Return to Selector
      cy.contains('Back').click();
    });
  });

  it('should validate YAML in Designer', () => {
    cy.contains('Designer').click();
    cy.get('textarea').clear().type('invalid: yaml: syntax');
    cy.contains('Validate').click();
    cy.contains('error', { timeout: 2000 }).should('exist');
  });

  it('should deploy a scenario', () => {
    cy.contains('Designer').click();
    cy.contains('Load template').select('EXPRESS');
    cy.contains('Deploy').click();
    cy.contains('deployed successfully', { timeout: 5000 }).should('exist');
  });
});
```

**Test data:**
- 4 scenarios: DEFAULT, EXPRESS, EXPRESS_DONE, SPECTRE
- YAML templates for Designer
- WebSocket message samples (for mocking)

**Framework:**
- Cypress (recommended for UI testing) or Playwright (faster, better mobile support)
- `cy.visit()`, `cy.contains()`, `cy.click()`, etc.

**Acceptance Criteria:**
- ✅ All 6 test cases pass
- ✅ Tests run in sequence (no parallel race conditions)
- ✅ No flaky tests (pass rate ≥95%)
- ✅ Test execution time ≤ 10 minutes (per scenario ~1-2 min)
- ✅ Screenshots/videos on failure (for debugging)

---

#### Task 4.2: Smoke Tests (Bash / Python)

**What:** Quick smoke tests (40 sec total) to verify core functionality: load scenario, arm, transition, verify UI link.

**Test script: `tools/dev/run_smoke_tests.sh`**

```bash
#!/bin/bash

# Smoke test: 4 scenarios (10 sec each)
# Total time: ~40 sec

PORT="${ZACUS_PORT_ESP32:-$(python3 tools/test/resolve_ports.py --need-esp32 --print-esp32 2>/dev/null)}"
if [ -z "$PORT" ]; then
  echo "FAIL: ESP32 port not found. Set ZACUS_PORT_ESP32 or run ./tools/dev/cockpit.sh ports"
  exit 1
fi

for scenario in DEFAULT EXPRESS EXPRESS_DONE SPECTRE; do
  echo "Smoke test: $scenario"
  
  # 1. Load scenario via serial
  echo "STORY_LOAD_SCENARIO $scenario" > "$PORT"
  sleep 0.5
  
  # 2. Verify loaded in serial output
  response=$(timeout 2 cat "$PORT")
  grep -q "STORY_LOAD_SCENARIO_OK" "$response" || { echo "FAIL: $scenario load"; exit 1; }
  
  # 3. Arm scenario
  echo "STORY_ARM" > "$PORT"
  sleep 0.5
  
  # 4. Verify armed
  response=$(timeout 2 cat "$PORT")
  grep -q "STORY_ARM_OK" "$response" || { echo "FAIL: $scenario arm"; exit 1; }
  
  # 5. Verify UI link is connected
  grep -q "UI_LINK_STATUS connected==1" "$response" || { echo "WARN: UI link not connected"; }
  
  # 6. Wait for scenario to complete
  sleep 8
  
  echo "✓ $scenario passed"
done

echo "✓ All smoke tests passed (40 sec)"
```

**Failure detection:**
- Regex patterns to detect:
  - `PANIC` (fatal error)
  - `REBOOT` (watchdog reset)
  - `UI_LINK_STATUS connected==0` (UI disconnected)
  - Missing expected log lines

**Acceptance Criteria:**
- ✅ All 4 scenarios pass smoke test
- ✅ Zero panics or reboots
- ✅ UI link connected throughout
- ✅ Total execution time ≤ 45 sec
- ✅ Script can run on macOS + Linux

---

#### Task 4.3: Stress Tests (Python)

**What:** 4-hour continuous loop testing resilience and stability.

**Test script: `tools/dev/run_stress_tests.py`**

```python
#!/usr/bin/env python3
import os
import serial
import time
import sys
from pathlib import Path

PORT = os.environ.get("ZACUS_PORT_ESP32")
if not PORT:
  raise RuntimeError("Set ZACUS_PORT_ESP32 or run ./tools/dev/cockpit.sh ports")
BAUD = 115200
DURATION_HOURS = 4
SCENARIOS = ['DEFAULT', 'EXPRESS', 'EXPRESS_DONE', 'SPECTRE']

logged_lines = []
errors = []

def log_output(line):
    global logged_lines
    logged_lines.append(line)
    if len(logged_lines) > 10000:
        logged_lines.pop(0)  # Keep only last 10k lines
    
    # Detect failure patterns
    if 'PANIC' in line or 'REBOOT' in line:
        errors.append(f"CRITICAL: {line}")
    if 'Guru Meditation Error' in line:
        errors.append(f"CRITICAL: {line}")

def run_scenario(scenario_id):
    """Run one scenario (duration ~10-20 sec)"""
    try:
        # Load + arm
        send_command(f"STORY_LOAD_SCENARIO {scenario_id}")
        time.sleep(1)
        send_command("STORY_ARM")
        time.sleep(1)
        
        # Wait for completion
        time.sleep(15)
        
        # Verify completed
        output = ''.join(logged_lines[-50:])
        if 'STORY_ENGINE_DONE' not in output and 'step: done' not in output:
            errors.append(f"Scenario {scenario_id} did not complete")
        
        return True
    except Exception as e:
        errors.append(f"Exception in {scenario_id}: {e}")
        return False

def send_command(cmd):
    """Send serial command"""
    with serial.Serial(PORT, BAUD, timeout=2) as ser:
        ser.write((cmd + '\n').encode())
        time.sleep(0.1)

def main():
    start_time = time.time()
    end_time = start_time + (DURATION_HOURS * 3600)
    iterations = 0
    
    print(f"Starting {DURATION_HOURS}h stress test on {PORT}...")
    
    while time.time() < end_time:
        for scenario in SCENARIOS:
            if time.time() >= end_time:
                break
            
            iterations += 1
            print(f"[{iterations}] Running {scenario}...", end='', flush=True)
            
            if run_scenario(scenario):
                print(" OK")
            else:
                print(" FAIL")
                break
    
    # Summary
    elapsed_hours = (time.time() - start_time) / 3600
    print(f"\nCompleted: {iterations} iterations in {elapsed_hours:.1f} hours")
    print(f"Success rate: {((iterations - len(errors)) / iterations * 100):.1f}%")
    
    if errors:
        print(f"\nErrors ({len(errors)}):")
        for err in errors[:20]:
            print(f"  - {err}")
        return 1
    else:
        print("✓ All iterations passed!")
        return 0

if __name__ == '__main__':
    sys.exit(main())
```

**Metrics tracked:**
- Total iterations completed
- Success rate (%)
- Errors + panic/reboot detection
- Memory trend (heap growth over time)
- Execution time per scenario

**Acceptance Criteria:**
- ✅ 4-hour test completes without panic or reboot
- ✅ Success rate ≥ 98% (allow 1-2 transient failures)
- ✅ Memory stable (heap growth ≤ 5KB over 4 hours)
- ✅ Zero memory leaks
- ✅ Test log: `artifacts/stress_test_4h_[timestamp].log`

---

#### Task 4.4: CI Integration (GitHub Actions)

**What:** Automate tests in GitHub Actions on every commit.

**Workflow file: `.github/workflows/firmware-story-v2.yml`**

```yaml
name: Story V2 Firmware Tests

on:
  push:
    branches: [ story-V2 ]
  pull_request:
    branches: [ story-V2 ]

jobs:
  build:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      
      - name: Set up Python
        uses: actions/setup-python@v4
        with:
          python-version: '3.9'
      
      - name: Install dependencies
        run: |
          cd hardware/firmware
          pip install -q platformio
          pip install -q pyyaml jsonschema pyserial
      
      - name: Build firmware
        run: |
          cd hardware/firmware
          pio run -e esp32dev
      
      - name: Run smoke tests (unit + schema)
        run: |
          cd hardware/firmware
          python3 tools/dev/test_story_gen.py
          python3 -m pytest esp32_audio/tests/test_story_fs_manager.py -v
      
      - name: Run cURL tests (mock API)
        run: |
          cd hardware/firmware
          bash esp32_audio/tests/test_story_http_api_mock.sh
      
      - name: Upload artifacts
        if: always()
        uses: actions/upload-artifact@v3
        with:
          name: test-results
          path: artifacts/

  hardware-smoke:
    runs-on: self-hosted
    needs: build
    if: github.event_name == 'push'
    steps:
      - uses: actions/checkout@v3
      
      - name: Flash firmware
        run: |
          cd hardware/firmware
          ./tools/dev/cockpit.sh flash
      
      - name: Run smoke tests (4 scenarios)
        run: |
          cd hardware/firmware
          bash tools/dev/run_smoke_tests.sh
      
      - name: Upload smoke log
        if: always()
        uses: actions/upload-artifact@v3
        with:
          name: smoke-test-log
          path: artifacts/rc_live/smoke_[timestamp].log
```

**CI gates:**
- Unit tests pass (Python + C++)
- Build succeeds (no compiler errors)
- Smoke tests pass (on hardware runner, if available)
- Artifacts collected

**Approval gates (optional):**
- PR requires CI green + code review before merge
- Nightly: run 4-hour stress test (report results)

**Acceptance Criteria:**
- ✅ Workflow file created in `.github/workflows/`
- ✅ Triggers on push to `story-V2` branch
- ✅ Build job passes
- ✅ Unit tests pass
- ✅ Artifacts collected (logs + binaries)

---

#### Task 4.5: Test Documentation

**What:** Write test procedures for other developers and test runners.

**Document: `esp32_audio/tests/README.md` or `docs/TESTING.md`**

```markdown
# Story V2 Testing Guide

## Quick Smoke Test (40 sec)

Run locally:
```bash
cd hardware/firmware
bash tools/dev/run_smoke_tests.sh
```

Expected output:
```
✓ DEFAULT passed
✓ EXPRESS passed
✓ EXPRESS_DONE passed
✓ SPECTRE passed
✓ All smoke tests passed (40 sec)
```

## Unit Tests

C++ tests:
```bash
pio run -e esp32dev --target test
```

Python tests:
```bash
python3 -m pytest esp32_audio/tests/test_story_*.py -v
```

## E2E Tests

Prerequisites:
- Firmware flashed and running
- WebUI deployed
- ESP at http://[IP]:8080

Run:
```bash
npx cypress run --spec "esp32_audio/tests/e2e/**/*.cy.js"
```

## Stress Test (4 hours)

Prerequisites:
- Serial connection to ESP
- Compile latest firmware

Run:
```bash
python3 tools/dev/run_stress_tests.py
```

Expected: ≥98% success rate, zero panics/reboots

## Troubleshooting

### Test fails: "PANIC: assertion failed"
- Check free heap (may be out of RAM)
- Recompile with optimizations enabled

### Smoke test times out
- Check serial port: `./tools/dev/cockpit.sh ports`
- Verify baud rate: 115200 for ESP32

### WebSocket disconnects
- Check WiFi signal strength
- Verify firewall allows port 8080

## Test Coverage

Current coverage:
- Unit tests: 40% (story_gen.py + StoryFsManager)
- E2E tests: 100% (user workflows)
- Smoke tests: 100% (all 4 scenarios)
- Stress tests: 4 hours × 4 scenarios = 1520 iterations

Target: ≥80% code coverage by Phase 5
```

**Sections:**
- Quick start (smoke test)
- How to run each test type
- What to expect (pass/fail criteria)
- Troubleshooting common issues
- Test coverage summary

**Acceptance Criteria:**
- ✅ README covers all test types (unit, E2E, smoke, stress)
- ✅ Commands copy-paste ready
- ✅ Clear pass/fail criteria
- ✅ Troubleshooting section helpful

---

### 📋 Acceptance Criteria (Phase 4 Complete)

- ✅ **E2E test suite** functional
  - All 6 test cases pass (launch, pause/resume, skip, 4-scenario loop, validate, deploy)
  - Zero flaky tests (≥95% pass rate)
  - Test execution ≤ 10 minutes
  
- ✅ **Smoke test** script working
  - All 4 scenarios pass
  - Zero panics/reboots
  - UI link connected
  - Total time ≤ 45 sec
  
- ✅ **Stress test** passes
  - 4-hour loop completes
  - Success rate ≥ 98%
  - Zero memory leaks
  - Heap stable (growth ≤ 5KB)
  
- ✅ **CI integration** operational
  - GitHub Actions workflow defined
  - Triggers on push to story-V2
  - Build + unit tests pass automatically
  - Artifacts collected
  
- ✅ **Test documentation** complete
  - README covers all test types
  - Quick start + troubleshooting
  - Clear pass/fail criteria
  
- ✅ **Code committed** to `story-V2` branch
  - No merge conflicts
  - CI passes
  
- ✅ **Artifacts collected**
  - Smoke test log: `artifacts/rc_live/smoke_[timestamp].log`
  - Stress test log: `artifacts/stress_test_4h_[timestamp].log`
  - E2E report with screenshots (on failure)

---

### ⏱️ Timeline

- **Depends on:** Phases 2-3 complete (Mar 5-9)
- **Start:** Mar 5-9 (parallel with Phase 3 end)
- **ETA:** Mar 16 (Sunday) or Mar 19 (Wednesday) EOD
- **Duration:** ~2 weeks

---

### 📊 Blockers & Escalation

If you encounter blockers:
1. **Phases 2-3 not stable:** Wait for handoff; don't start tests
2. **Serial port not available:** Running on CI runner or local? Check port name
3. **Test flakiness:** Increase timeouts; check ESP32 heap
4. **Hardware unavailable:** Skip hardware tests; mock API responses

---

### 🎯 Deliverables

**On completion, provide:**
1. ✅ Commit hash for Phase 4 work
2. ✅ E2E test results (pass count + execution time)
3. ✅ Smoke test log: `artifacts/smoke_test_[timestamp].log`
4. ✅ Stress test log: `artifacts/stress_test_4h_[timestamp].log`
5. ✅ Test coverage report (if available)

**Report to Coordination Hub:**
```
**Phase 4 Complete**
- ✅ E2E test suite: 6/6 tests passing
- ✅ Smoke tests: 4/4 scenarios passing (40 sec)
- ✅ Stress test: 4-hour loop completed (1520 iterations, 98% success)
- ✅ CI integrated: GitHub Actions workflow operational
- ✅ Test documentation: README + troubleshooting
- ✅ Code committed: [commit hash]
- 📁 Artifacts: smoke_[timestamp].log, stress_test_4h_[timestamp].log
- 🎯 Next: Phase 5 unblocked (Release + RC build)
```
