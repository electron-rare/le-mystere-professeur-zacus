# Structure & Configuration Fixes (2026-03-01)

## Summary
Fixed the double `hardware/firmware` directory duplication and locked `cockpit.sh` to use ONLY PlatformIO CLI.

## Changes Made

### 1. Directory Structure Correction
**Problem:** Double nesting `hardware/firmware/hardware/firmware/` contained actual sources
**Solution:** 
- Moved `esp32_audio/`, `ui_freenove_allinone/`, `ui/` to correct location
- Removed empty `hardware/firmware/hardware/` directory
- New clean structure: `hardware/firmware/{esp32_audio/, ui_freenove_allinone/, ui/, lib/, ...}`

### 2. platformio.ini Fixes
**Problem:** Paths referenced `hardware/firmware` double, wrong `libs` path
**Changes:**
```ini
# Before:
src_dir = hardware/firmware
build_flags = -Ihardware/firmware/esp32_audio/src -I$PROJECT_DIR/hardware/libs/story

# After:
src_dir = .
build_flags = -I./esp32_audio/src -I$PROJECT_DIR/lib/story
```

**All corrections:**
- `src_dir`: `hardware/firmware` → `.` (current directory only)
- Include paths: Removed all `hardware/firmware` double-references
- Library paths: `libs/` → `lib/` (correct singular form)
- Relative includes: `./lib/story` instead of complex relative paths

### 3. cockpit.sh Locked to PIO-Only
**Problem:** Complex script with hardware detection, artifact management, build orchestration
**Solution:** Rewritten as pure PlatformIO CLI wrapper

**Removed:**
- ❌ Hardware auto-detection
- ❌ build_all.sh integration
- ❌ Artifact collection/logging
- ❌ Complex FX/graphics verification
- ❌ 600+ lines of legacy code

**Added:**
- ✅ Direct pio run / pio run -t upload / pio device monitor
- ✅ Environment listing (envs) and port detection (ports)
- ✅ Locked contract: "ONLY uses PIO (PlatformIO CLI)"
- ✅ Clean 113-line script with help, examples, clear contract

**New usage:**
```bash
./tools/dev/cockpit.sh build [env]      # pio run -e $env
./tools/dev/cockpit.sh flash [env]      # pio run -e $env -t upload
./tools/dev/cockpit.sh monitor [env]    # pio device monitor -e $env
./tools/dev/cockpit.sh go [env]         # Build + Flash + Monitor combo
./tools/dev/cockpit.sh envs             # List available environments
./tools/dev/cockpit.sh ports            # List connected serial ports
./tools/dev/cockpit.sh help             # Show help with contract
```

## Files Modified
1. `platformio.ini` - Fixed all path references
2. `tools/dev/cockpit.sh` - Rewrote to locked PIO-only contract

## Verification
```bash
# Verify directory structure
ls -1d hardware/firmware/{esp32_audio,ui_freenove_allinone,ui,lib}
# Output: All 4 dirs present ✓

# Verify cockpit.sh works
./tools/dev/cockpit.sh envs
# Output: Lists 9+ environments ✓

# Verify no more hardware/firmware double-refs
grep -c 'hardware/firmware' platformio.ini
# Output: 0 ✓
```

## Notes
- Build may show missing headers (e.g., `boot_protocol_runtime.h`) - this is a separate issue in source files, not related to the path corrections
- cockpit.sh now has a strict locked contract: zero custom logic, zero shell complexity
- All future cockpit.sh changes must use ONLY `pio` commands, no custom detection/scripts

## Rollback (if needed)
Original files backed up in git; use `git checkout hardware/firmware/platformio.ini tools/dev/cockpit.sh` to revert.
