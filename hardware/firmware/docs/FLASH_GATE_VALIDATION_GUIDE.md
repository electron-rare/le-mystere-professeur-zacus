# Flash Gate Validation Guide

## Purpose

Validate that `./tools/dev/cockpit.sh flash` correctly **auto-detects ESP32/ESP8266/RP2040 serial ports** across 3 configuration scenarios:
1. **Auto-detect (default)** — Automatic port resolution
2. **Explicit ports** — Manual port overrides via environment variables
3. **Learned cache** — Using previously discovered port mappings

---

## Prerequisites

✅ Firmware is built:
```bash
./tools/dev/cockpit.sh build
```

✅ Hardware is connected (at minimum: ESP32 + ESP8266)

✅ Bootstrap is complete:
```bash
./tools/dev/bootstrap_local.sh
```

---

## Test 1: Auto-Detect (Default)

### Command

```bash
./tools/dev/cockpit.sh ports
# Check available ports
```

Then execute:

```bash
./tools/dev/cockpit.sh flash
```

### Expected Behavior

- **Port resolution** triggers automatically
- **resolve_ports.py** searches for ESP32 (VID:PID + location + fingerprint)
- **resolve_ports.py** searches for ESP8266 (same order)
- **Detection completes** within 5 seconds (default ZACUS_PORT_WAIT)
- **Both ports detected** → Flash executes immediately
- **Missing one port** → `[FAIL] port resolution failed` → Exit code 1

### Success Indicators

✅ Output shows:
```
[step] resolve ports
[port-resolution] esp32=<port> esp8266=<port>
[step] flash ESP32
[step] flash ESP8266
[complete] flash for <env>
```

✅ Log file created: `logs/flash_YYYYMMDD-HHMMSS.log`

✅ Artifact directory: `artifacts/rc_live/flash-YYYYMMDD-HHMMSS/ports_resolve.json`

### Troubleshooting

**Symptom**: `[FAIL] port resolution failed (esp32= esp8266=)`

**Diagnosis**:
```bash
# List detected ports
ls -la /dev/tty* | grep -E "USB|serial"

# Manually check resolution JSON
cat artifacts/rc_live/flash-*/ports_resolve.json | python3 -m json.tool
```

**Quick Fixes**:
- **Device not detected**: Unplug USB, wait 2 sec, replug → retry
- **Wrong device**: Check VIDPID mapping in `tools/dev/ports_map.json`
- **Location mapping issue**: Inspect port location in `ports_resolve.json` → compare with `ports_map.json`

---

## Test 2: Explicit Ports (Manual Override)

### Setup

```bash
# Find current ports
./tools/dev/cockpit.sh ports
# Example output:
# /dev/cu.SLAB_USBtoUART → CP2102 (ESP32 likely)
# /dev/cu.usbserial-1410 → CH340 (ESP8266 likely)
```

### Command

With explicit port overrides:

```bash
export ZACUS_PORT_ESP32="/dev/cu.SLAB_USBtoUART"
export ZACUS_PORT_ESP8266="/dev/cu.usbserial-1410"
./tools/dev/cockpit.sh flash
```

Or as inline args:

```bash
ZACUS_PORT_ESP32="/dev/cu.SLAB_USBtoUART" \
ZACUS_PORT_ESP8266="/dev/cu.usbserial-1410" \
./tools/dev/cockpit.sh flash
```

### Expected Behavior

- **Port resolution skips auto-detect** (manual overrides provided)
- **No fingerprint scanning** (if ports are explicitly specified)
- **Flash executes immediately** against specified ports
- **Wrong port choice** → Flash fails (no recovery)

### Success Indicators

✅ Output shows:
```
[port-resolution] manual override esp32=/dev/cu.SLAB_USBtoUART
[port-resolution] manual override esp8266=/dev/cu.usbserial-1410
[step] flash ESP32 (via /dev/cu.SLAB_USBtoUART)
[step] flash ESP8266 (via /dev/cu.usbserial-1410)
[complete] flash
```

✅ `ports_resolve.json` contains:
```json
{
  "status": "pass",
  "ports": {
    "esp32": "/dev/cu.SLAB_USBtoUART",
    "esp8266": "/dev/cu.usbserial-1410",
    "rp2040": ""
  },
  "reasons": {
    "esp32": "manual-override",
    "esp8266": "manual-override"
  },
  "notes": ["manual override esp32 -> /dev/cu.SLAB_USBtoUART", ...]
}
```

### Validation

```bash
# Verify ports in JSON
cat artifacts/rc_live/flash-*/ports_resolve.json | jq '.reasons'
# Should show: {"esp32": "manual-override", "esp8266": "manual-override"}
```

---

## Test 3: Learned Cache (Persistent Port Mapping)

### Scenario

After successful auto-detect runs, resolved ports are **learned** into `.local/ports_map.learned.json`.

### Command (After successful Test 1)

```bash
# Verify learned cache exists
cat .local/ports_map.learned.json
```

Expected output:
```json
{
  "location": {
    "20-6.1.1": "esp32",
    "20-6.1.2": "esp8266_usb"
  }
}
```

### Next Run

```bash
./tools/dev/cockpit.sh flash
# Port resolution uses learned cache + auto-detect fallback
```

### Expected Behavior

- **Learned entries are tried first** (before generic patterns)
- **Faster detection** (cached LOCATION → role mapping)
- **Fallback to auto-detect** if hardware changed
- **Cache updated** if new devices found with fingerprinting

### Success Indicators

✅ Port resolution in output shows:
```
[port-resolution] esp32 found via location-map:20-6.1.1 (learned)
[port-resolution] esp8266 found via location-map:20-6.1.2 (learned)
```

✅ Detection time < 1 second (cache hit)

### Verify Learned Cache

```bash
# Print learned map
python3 - <<'PY'
import json
cache = json.load(open(".local/ports_map.learned.json"))
for loc, role in cache.get("location", {}).items():
    print(f"{loc} -> {role}")
PY
```

---

## Test 4: RP2040 Detection (Optional)

### Prerequisites

- RP2040 Pico board plugged into USB
- ZACUS_REQUIRE_RP2040=1 environment variable set

### Command

```bash
ZACUS_REQUIRE_RP2040=1 ./tools/dev/cockpit.sh flash
```

### Expected Behavior

- **Port resolution searches for RP2040** (VIDPID: 2e8a:0005 or 2e8a:000a)
- **Waits up to 5 seconds** if not found (ZACUS_PORT_WAIT)
- **Fails if missing** (unless ZACUS_ALLOW_NO_HW=1)

### Success Indicators

✅ Output shows:
```
[port-resolution] esp32=<port>
[port-resolution] esp8266=<port>
[port-resolution] rp2040=<port>
[step] flash RP2040
```

✅ `ports_resolve.json` contains RP2040 entry:
```json
{
  "ports": {
    "esp32": "...",
    "esp8266": "...",
    "rp2040": "/dev/cu.usbmodem..."
  }
}
```

### Troubleshooting RP2040

**Symptom**: `[FAIL] port resolution failed (rp2040=)`

**Diagnosis**:
```bash
# Check if RP2040 is visible
system_profiler SPUSBDataType | grep -i "rp2040\|pico\|2e8a"

# Try manual override
ZACUS_PORT_RP2040="/dev/cu.usbmodem..." ./tools/dev/cockpit.sh flash
```

**Root Causes**:
- RP2040 not plugged in
- RP2040 driver missing (macOS: install CP210x drivers if needed)
- VIDPID not in ports_map.json (editable; add to `tools/dev/ports_map.json`)

---

## Full Test Checklist

Run all three tests in sequence and record results:

### Test 1: Auto-Detect

- [ ] `./tools/dev/cockpit.sh flash` completes successfully
- [ ] Both ESP32 and ESP8266 detected
- [ ] Duration < 5 seconds
- [ ] `ports_resolve.json` exists
- [ ] No timeouts or errors

**Result**: ✅ PASS / ❌ FAIL

**Notes**: _________________________________

### Test 2: Explicit Ports

- [ ] Identify correct ports via `./tools/dev/cockpit.sh ports`
- [ ] Export ZACUS_PORT_ESP32 and ZACUS_PORT_ESP8266
- [ ] `./tools/dev/cockpit.sh flash` uses exported ports
- [ ] Flash succeeds against correct devices
- [ ] `ports_resolve.json` shows "manual-override" reasons

**Result**: ✅ PASS / ❌ FAIL

**Notes**: _________________________________

### Test 3: Learned Cache

- [ ] `.local/ports_map.learned.json` exists (after Test 1)
- [ ] Contains LOCATION → role mappings
- [ ] `./tools/dev/cockpit.sh flash` uses learned cache first
- [ ] Detection time < 1 second (cache hit)
- [ ] `ports_resolve.json` shows "location-map" reasons

**Result**: ✅ PASS / ❌ FAIL

**Notes**: _________________________________

### Test 4: RP2040 (if hardware available)

- [ ] RP2040 board plugged in
- [ ] `ZACUS_REQUIRE_RP2040=1 ./tools/dev/cockpit.sh flash` attempts detection
- [ ] RP2040 port detected correctly
- [ ] Flash succeeds or hangs (no crash/error)

**Result**: ✅ PASS / ❌ N/A (no hardware)

**Notes**: _________________________________

---

## Phase 1 Deliverables

After completing all 3-4 tests, produce:

### 1. `artifacts/flash_gate_validation_report.txt`

```
FLASH GATE VALIDATION REPORT
Date: 2026-02-16
Hardware: ESP32 + ESP8266 + [RP2040 if available]

TEST 1: Auto-Detect
Status: PASS
Duration: 2.3 sec
Ports detected: esp32=/dev/cu.SLAB_USBtoUART esp8266=/dev/cu.usbserial-1410
Artifact: artifacts/rc_live/flash-20260216-123456/ports_resolve.json

TEST 2: Explicit Ports
Status: PASS
Ports used: ZACUS_PORT_ESP32=/dev/cu.SLAB_USBtoUART ZACUS_PORT_ESP8266=/dev/cu.usbserial-1410
Flash result: SUCCESS

TEST 3: Learned Cache
Status: PASS
Cache hit: yes (location-map:20-6.1.1 -> esp32)
Duration: 0.8 sec

TEST 4: RP2040
Status: N/A (no hardware)

OVERALL RESULT: ALL TESTS PASSED ✓
Recommendation: Flash gate is 100% reproducible and ready for Phase 2
```

### 2. `artifacts/flash_gate_validation_logs/`

Collect logs from all test runs:
```
flash_gate_validation_logs/
  ├── test1_auto_detect.log
  ├── test2_explicit_ports.log
  ├── test3_learned_cache.log
  ├── test4_rp2040.log (if run)
  └── ports_resolve_examples.json
```

### 3. Evidence in Git

```bash
ZACUS_GIT_ALLOW_WRITE=1 \
./tools/dev/cockpit.sh git add artifacts/flash_gate_validation_* logs/flash_*

ZACUS_GIT_ALLOW_WRITE=1 \
./tools/dev/cockpit.sh git commit -m "Phase 1: Flash gate validation complete (3/5 tests passed)"
```

---

## Automation (Optional)

For repeated validation, use this script:

```bash
#!/usr/bin/env bash
set -euo pipefail

echo "=== Flash Gate Validation Suite ==="

# Test 1: Auto-detect
echo "Test 1: Auto-detect..."
./tools/dev/cockpit.sh flash || echo "FAIL"

# Test 2: Explicit
echo "Test 2: Explicit ports..."
ESP_PORT=${ESP_PORT:-$(./tools/dev/cockpit.sh ports | grep "SLAB" | head -1 | awk '{print $1}')}
ESP8266_PORT=${ESP8266PORT:-$(./tools/dev/cockpit.sh ports | grep "usbserial" | head -1 | awk '{print $1}')}
ZACUS_PORT_ESP32="$ESP_PORT" ZACUS_PORT_ESP8266="$ESP8266_PORT" ./tools/dev/cockpit.sh flash || echo "FAIL"

# Test 3: Learned (no setup needed)
echo "Test 3: Learned cache..."
./tools/dev/cockpit.sh flash || echo "FAIL"

echo "=== Validation Complete ==="
```

---

## References

- [resolve_ports.py](../../tools/test/resolve_ports.py) — Port detection logic
- [tools/dev/ports_map.json](../../tools/dev/ports_map.json) — VIDPID + LOCATION mappings
- [.local/ports_map.learned.json](.local/ports_map.learned.json) — Learned cache (auto-created)
- [FIRMWARE_EMBEDDED_EXPERT.md](#phase-1-objectives) — Phase 1 acceptance criteria
- [AGENT_BRIEFINGS.md](../../.github/agents/AGENT_BRIEFINGS.md#firmware-embedded-expert) — Full Firmware Expert contract
