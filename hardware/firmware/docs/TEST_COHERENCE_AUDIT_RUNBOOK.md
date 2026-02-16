# Test Coherence Audit Runbook

## Purpose

Validate that all test gates (build, flash, smoke, baseline) produce **coherent artifacts** with:
- Expected directory structure
- All mandatory metadata files
- Consistent logging format
- Traceable timestamps + commit references

## Tools

- **Primary audit tool**: `python3 tools/test/audit_coherence.py`
- **Manual inspection**: Use this runbook for spot-checks and failure diagnosis
- **Evidence record**: All audit results go to `logs/audit_coherence_*.log`

---

## Section 1: Pre-Baseline Audit (Phase 1 Prep)

Run this **before** executing the firmware health baseline.

### 1.1 Verify cockpit_commands.yaml is valid

```bash
# Syntax check
python3 -c "import yaml; yaml.safe_load(open('tools/dev/cockpit_commands.yaml'))"
```

Expected: No errors.

### 1.2 Regenerate cockpit docs (if changed)

```bash
# Regenerate from cockpit_commands.yaml
python3 tools/dev/gen_cockpit_docs.py

# Check for new commands
git diff --stat docs/_generated/COCKPIT_COMMANDS.md
```

Expected: All commands from cockpit_commands.yaml appear in generated docs.

### 1.3 Run full coherence audit

```bash
# Audit: scripts vs cockpit vs docs
python3 tools/test/audit_coherence.py

# Check output
tail -100 logs/audit_coherence_*.log
```

Expected output:

```
[AUDIT] Checking cockpit_commands.yaml...
[OK] 5 git commands registered
[OK] 8 core commands registered
[OK] 2 phase-specific commands registered

[AUDIT] Checking evidence paths...
[OK] artifacts/rc_live/ (rc gate)
[OK] artifacts/baseline_*/ (baseline runner)

[AUDIT] Cross-validation...
[WARN] flash gate missing health snapshot reference
[OK] Smoke tests have panic marker tracking

[AUDIT] Summary: 15 checks passed, 1 warning, 0 failures
```

### 1.4 Spot-check gate scripts exist

```bash
# Check executability
for script in \
  ./tools/dev/cockpit.sh \
  ./tools/dev/run_matrix_and_smoke.sh \
  ./tools/dev/generate_baseline.sh \
  tools/dev/agent_utils.sh; do
  [[ -x "$script" ]] && echo "✓ $script" || echo "✗ $script MISSING"
done
```

Expected: All 4 scripts are executable.

---

## Section 2: Post-Baseline Audit (Phase 1 Complete)

Run this **after** executing the firmware health baseline.

### 2.1 Check baseline directory structure

```bash
# Find latest baseline
BASELINE=$(ls -dt artifacts/baseline_*/ | head -1)
echo "Checking: $BASELINE"

# Check subdirectories
ls -la "$BASELINE"
```

Expected output:

```
drwxr-xr-x  4_healthcheck
drwxr-xr-x  1_build
drwxr-xr-x  2_flash_tests
drwxr-xr-x  3_smoke_001-010
```

### 2.2 Verify build phase artifacts

```bash
BASELINE=$(ls -dt artifacts/baseline_*/ | head -1)

# Count build logs
ls -1 "$BASELINE/1_build/build_*.log" | wc -l
```

Expected: 3 build logs.

**Check each log for completion**:

```bash
for log in "$BASELINE/1_build/build_*.log"; do
  echo "=== $(basename $log) ==="
  tail -5 "$log"  # Should show successful build message
done
```

### 2.3 Verify flash phase artifacts

```bash
BASELINE=$(ls -dt artifacts/baseline_*/ | head -1)

# Count flash logs
ls -1 "$BASELINE/2_flash_tests/flash_*.log" | wc -l
```

Expected: 5 flash logs.

**Check for flash success markers**:

```bash
for log in "$BASELINE/2_flash_tests/flash_*.log"; do
  status="UNKNOWN"
  grep -q "Flash completed successfully" "$log" && status="OK"
  grep -q "ERROR\|FAIL" "$log" && status="FAIL"
  echo "$(basename $log): $status"
done
```

### 2.4 Verify smoke phase artifacts

```bash
BASELINE=$(ls -dt artifacts/baseline_*/ | head -1)

# Count smoke runs
ls -1d "$BASELINE/3_smoke_001-010/smoke_"??? | wc -l
```

Expected: 10 smoke run directories.

**Check each smoke run has required evidence**:

```bash
for dir in "$BASELINE/3_smoke_001-010/smoke_"???; do
  run_num=$(basename "$dir")
  
  # Check mandatory files
  [[ -f "$dir/run_matrix_and_smoke.log" ]] && echo "✓ $run_num log" || echo "✗ $run_num MISSING log"
  [[ -f "$dir/meta.json" ]] && echo "✓ $run_num meta" || echo "✗ $run_num MISSING meta"
  [[ -f "$dir/commands.txt" ]] && echo "✓ $run_num commands" || echo "✗ $run_num MISSING commands"
done
```

Expected: Each run has all 3 mandatory files.

### 2.5 Parse panic markers from smoke logs

```bash
BASELINE=$(ls -dt artifacts/baseline_*/ | head -1)

echo "=== Panic Incidents ==="
panic_count=0

for log in "$BASELINE/3_smoke_001-010/smoke_"???"/run_matrix_and_smoke.log"; do
  if grep -q "Guru Meditation\|Core.*panic\|rst:0x\|abort()" "$log"; then
    run_num=$(basename $(dirname "$log"))
    echo "⚠️  $run_num: Panic detected"
    grep "Guru Meditation\|Core.*panic\|rst:0x\|abort()" "$log" | head -3
    ((panic_count++)) || true
  fi
done

echo ""
echo "Total panic incidents: $panic_count / 10"
```

### 2.6 Verify health check snapshot

```bash
BASELINE=$(ls -dt artifacts/baseline_*/ | head -1)

# Check snapshot exists
ls -la "$BASELINE/4_healthcheck/health_snapshot_*.txt"

# Sample content
echo "=== Health Snapshot Preview ==="
head -50 "$BASELINE/4_healthcheck/health_snapshot_"*.txt
```

Expected: One health snapshot with RTOS metrics.

### 2.7 Run automated coherence audit on results

```bash
BASELINE=$(ls -dt artifacts/baseline_*/ | head -1)

# Use audit tool to validate baseline structure
python3 tools/test/audit_coherence.py --baseline "$BASELINE"
```

Expected output:

```
[AUDIT] Validating baseline structure...
[OK] 3 builds found in 1_build/
[OK] 5 flash tests found in 2_flash_tests/
[OK] 10 smoke runs found in 3_smoke_001-010/
[OK] Meta files complete (30 / 30 present)

[AUDIT] Checking panic incidents...
[REPORT] 0 panic incidents found
[OK] Baseline is panic-free ✓

[AUDIT] Summary: BASELINE PASSED
```

---

## Section 3: Failure Diagnosis

### 3.1 Build failed

**Symptom**: Less than 3 build logs in `1_build/`

**Investigation**:

```bash
BASELINE=$(ls -dt artifacts/baseline_*/ | head -1)
grep -i error "$BASELINE/1_build/"*.log | head -20
```

**Root causes**:
- Missing dependency (check `pio run --dry-run`)
- Corrupted source file (check git status)
- PlatformIO cache issue (try `pio run --clean-build`)

---

### 3.2 Flash failed

**Symptom**: Flash logs show `ERROR` or missing device

**Investigation**:

```bash
BASELINE=$(ls -dt artifacts/baseline_*/ | head -1)
cat "$BASELINE/2_flash_tests/flash_1.log" | grep -A5 "ERROR\|No such device"
```

**Root causes**:
- USB device not connected
- Wrong port mapping (check `./tools/dev/cockpit.sh ports`)
- Permission denied (check `/dev/ttyUSB* -l`)

---

### 3.3 Smoke panic detected

**Symptom**: Panic marker in smoke log

**Investigation**:

```bash
BASELINE=$(ls -dt artifacts/baseline_*/ | head -1)

# Find smoke run with panic
panic_run=$(for d in "$BASELINE/3_smoke_001-010/smoke_"???; do
  grep -l "Guru Meditation\|Core.*panic" "$d/run_matrix_and_smoke.log" 2>/dev/null && echo "$d"
done | head -1)

if [[ -n "$panic_run" ]]; then
  echo "Panic found in: $(basename $panic_run)"
  
  # Extract panic context
  grep -B5 -A10 "Guru Meditation\|panic\|abort()" "$panic_run/run_matrix_and_smoke.log"
fi
```

**Root causes**:
- Stack overflow (check RTOS task stack sizes)
- Null pointer dereference (check FW source for bounds)
- WiFi disconnect handler crash (check WiFi event loop)

See [FIRMWARE_HEALTH_BASELINE.md](./FIRMWARE_HEALTH_BASELINE.md#panic-markers-found) for root cause analysis template.

---

## Section 4: Evidence Validation Checklist

Use this checklist to sign off on a completed baseline:

### Build Phase

- [ ] 3 build logs present (1_build/build_1.log, build_2.log, build_3.log)
- [ ] Each build log ends with success message
- [ ] No compilation errors in any log
- [ ] Build timestamps are sequential
- [ ] All 5 environments built (esp32dev, esp32_release, esp8266_oled, ui_rp2040_ili9488, ui_rp2040_ili9486)

### Flash Phase

- [ ] 5 flash test logs present (2_flash_tests/flash_1.log through flash_5.log)
- [ ] Each flash completes without permission errors
- [ ] USB device stable across 5 flashes
- [ ] Flash timestamps don't overlap (sequential)
- [ ] No device disconnects reported

### Smoke Phase

- [ ] 10 smoke run directories present (smoke_001 through smoke_010)
- [ ] Each run has:
  - [ ] `run_matrix_and_smoke.log`
  - [ ] `meta.json`
  - [ ] `commands.txt`
  - [ ] `summary.md` (if applicable)
- [ ] Panic markers tracked (count = 0 for approval)
- [ ] All 10 runs complete without hanging
- [ ] Duration reasonable (~3-5 min per run)

### Health Phase

- [ ] Health snapshot file ≥ 100 bytes
- [ ] Contains valid JSON or structured output
- [ ] RTOS metrics present (heap, tasks, stack)
- [ ] WiFi signal strength recorded
- [ ] Timestamp matches baseline run time

### Cross-Phase

- [ ] Git commit hash recorded in evidence
- [ ] All files readable (check permissions: `ls -la`)
- [ ] No truncated logs
- [ ] All paths match cockpit_commands.yaml specs
- [ ] Baseline duration matches estimate (45-65 min for full run)

---

## Section 5: Coherence Audit Script Usage

### Running audit_coherence.py

```bash
# Full audit
python3 tools/test/audit_coherence.py

# Audit specific baseline
python3 tools/test/audit_coherence.py --baseline artifacts/baseline_20260216_001

# Quick check (fast mode)
python3 tools/test/audit_coherence.py --quick

# Verbose output
python3 tools/test/audit_coherence.py --verbose
```

### Interpreting audit report

```
[AUDIT] Checking baseline structure...
[OK] 1_build: 3 / 3 logs present
[OK] 2_flash_tests: 5 / 5 logs present
[OK] 3_smoke_001-010: 10 / 10 directories present
[OK] 4_healthcheck: 1 health snapshot present

[AUDIT] Evidence completeness...
[OK] build logs: 3 / 3 complete
[OK] flash logs: 5 / 5 complete
[OK] smoke metadata: 30 / 30 files present
[WARN] health snapshot: JSON parsing failed (expected but not critical)

[AUDIT] Panic tracking...
[OK] Panic markers: 0 found (panic-free baseline ✓)

[AUDIT] Report: PASS
```

---

## Section 6: Continuous Validation

### Weekly Check

Run this every week to catch drift:

```bash
# 1. Regenerate docs from registry
python3 tools/dev/gen_cockpit_docs.py

# 2. Check for unexpected changes
git diff --stat docs/_generated/COCKPIT_COMMANDS.md

# 3. Run coherence audit
python3 tools/test/audit_coherence.py

# 4. Spot-check a recent artifact
ls -la artifacts/rc_live/$(ls -t artifacts/rc_live/ | head -1)
```

### Phase Transition Checkpoints

Before moving to next phase (Baseline → Phase 2 Investigation):

```bash
# 1. Validate latest baseline
BASELINE=$(ls -dt artifacts/baseline_*/ | head -1)
python3 tools/test/audit_coherence.py --baseline "$BASELINE"

# 2. Check for unresolved panics
grep -r "Guru Meditation\|panic" "$BASELINE/3_smoke_001-010/"

# 3. Sign-off
echo "Phase 1 baseline APPROVED" >> logs/phase1_signoff.txt
git add logs/phase1_signoff.txt
ZACUS_GIT_ALLOW_WRITE=1 ./tools/dev/cockpit.sh git commit -m "Phase 1: Baseline approved"
```

---

## Key References

- [FIRMWARE_HEALTH_BASELINE.md](./FIRMWARE_HEALTH_BASELINE.md) — Phase 1 baseline interpretation guide
- [TEST_SCRIPT_COORDINATOR.md](./TEST_SCRIPT_COORDINATOR.md) — Gate definitions and evidence standards
- `tools/test/audit_coherence.py` — Automated validation script
- `tools/dev/cockpit_commands.yaml` — Command registry (source of truth)
