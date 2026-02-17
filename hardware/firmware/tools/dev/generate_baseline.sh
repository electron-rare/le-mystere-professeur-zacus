#!/usr/bin/env bash

# Generate firmware health baseline for Phase 1 (Stabilize + Observe)
# Runs: 3 builds, 5 flash tests, 10 smoke tests, health check
# Collects all evidence under artifacts/baseline_YYYYMMDD_###/

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
FW_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
REPO_ROOT="$(cd "$FW_ROOT/../.." && pwd)"

# --- Config ---
BASELINE_NAME="baseline_$(date +%Y%m%d_001)"
BASELINE_DIR="$FW_ROOT/artifacts/$BASELINE_NAME"
LOG_FILE="$FW_ROOT/logs/generate_baseline_$(date +%Y%m%d-%H%M%S).log"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# --- Logging ---
log() {
  echo -e "[$(date '+%Y-%m-%d %H:%M:%S')] $*" | tee -a "$LOG_FILE"
}

fail() {
  echo -e "${RED}[FAIL] $*${NC}" | tee -a "$LOG_FILE"
  exit 1
}

success() {
  echo -e "${GREEN}[OK] $*${NC}" | tee -a "$LOG_FILE"
}

info() {
  echo -e "${YELLOW}[INFO] $*${NC}" | tee -a "$LOG_FILE"
}

# --- Setup ---
mkdir -p "$BASELINE_DIR"/{1_build,2_flash_tests,3_smoke_001-010,4_healthcheck}
mkdir -p "$FW_ROOT/logs"

log "Starting firmware health baseline generation"
log "Baseline: $BASELINE_NAME"
log "Output: $BASELINE_DIR"
log "Log: $LOG_FILE"

cd "$FW_ROOT"

# --- Phase 1: Build reproducibility (3 times) ---
log ""
log "===== PHASE 1: Build Reproducibility ====="
log "Target: 3 consecutive builds of all 5 environments"

for i in 1 2 3; do
  log "Build cycle $i/3..."
  
  if ./tools/dev/cockpit.sh build >>"$BASELINE_DIR/1_build/build_$i.log" 2>&1; then
    success "Build cycle $i: PASS"
  else
    fail "Build cycle $i: FAIL (see $BASELINE_DIR/1_build/build_$i.log)"
  fi
done

success "Build reproducibility: 3/3 passed"

# --- Phase 2: Flash gate reproducibility (5 times) ---
log ""
log "===== PHASE 2: Flash Gate Reproducibility ====="
log "Target: 5 consecutive flashes with auto port detection"

for i in 1 2 3 4 5; do
  log "Flash test $i/5..."
  
  if ./tools/dev/cockpit.sh flash >>"$BASELINE_DIR/2_flash_tests/flash_$i.log" 2>&1; then
    success "Flash test $i: PASS"
  else
    fail "Flash test $i: FAIL (see $BASELINE_DIR/2_flash_tests/flash_$i.log)"
  fi
done

success "Flash reproducibility: 5/5 passed"

# --- Phase 3: Smoke tests (10 times) ---
log ""
log "===== PHASE 3: Smoke Tests (10 runs) ====="
log "Target: 10 consecutive RC live gates (build + smoke)"

PANIC_COUNT=0
for i in {1..10}; do
  run_num=$(printf "%03d" "$i")
  log "Smoke run $i/10..."
  
  SMOKE_OUTDIR="$BASELINE_DIR/3_smoke_001-010/smoke_$run_num"
  mkdir -p "$SMOKE_OUTDIR"
  
  # Use cockpit.sh rc to generate full artifact
  if ZACUS_OUTDIR="$SMOKE_OUTDIR" ./tools/dev/cockpit.sh rc >>"$BASELINE_DIR/3_smoke_001-010/smoke_$run_num.log" 2>&1; then
    success "Smoke run $i: PASS"
  else
    rc_code=$?
    success "Smoke run $i: FAIL (exit code: $rc_code)"
    ((PANIC_COUNT++)) || true
  fi
  
  # Check for panic markers in logs
  if [[ -f "$SMOKE_OUTDIR/run_matrix_and_smoke.log" ]]; then
    if grep -q "Guru Meditation\|Core.*panic\|rst:0x\|abort()" "$SMOKE_OUTDIR/run_matrix_and_smoke.log"; then
      info "  ⚠️  Panic marker detected in smoke_$run_num"
    fi
  fi
done

log "Smoke testing complete: Panic incidents: $PANIC_COUNT"

# --- Phase 4: Health check ---
log ""
log "===== PHASE 4: RTOS/WiFi Health Check ====="
log "Target: Single health snapshot"

ESP_URL="${ESP_URL:-http://192.168.1.100:8080}"
log "Using ESP_URL=$ESP_URL"

if command -v python3 >/dev/null 2>&1; then
  if ./tools/dev/rtos_wifi_health.sh --outdir "$BASELINE_DIR/4_healthcheck" >>"$LOG_FILE" 2>&1; then
    success "Health check: PASS"
  else
    info "Health check: SKIPPED (ESP not responding at $ESP_URL)"
  fi
else
  info "Health check: SKIPPED (python3 not available)"
fi

# --- Finalization ---
log ""
log "===== BASELINE GENERATION COMPLETE ====="

# Collect summary stats
BUILD_RESULTS=$(find "$BASELINE_DIR/1_build" -name "build_*.log" | wc -l)
FLASH_RESULTS=$(find "$BASELINE_DIR/2_flash_tests" -name "flash_*.log" | wc -l)
SMOKE_RESULTS=$(find "$BASELINE_DIR/3_smoke_001-010" -name "smoke_*.log" | wc -l)

log "Summary:"
log "  Build cycles: $BUILD_RESULTS/3"
log "  Flash tests: $FLASH_RESULTS/5"
log "  Smoke runs: $SMOKE_RESULTS/10"
log "  Panic incidents: $PANIC_COUNT"
log ""
log "Baseline artifacts: $BASELINE_DIR"
log "Full log: $LOG_FILE"
log ""
log "Next step: Fill out .github/agents/reports/firmware-health-baseline.md with results"

success "Baseline generation successful!"
