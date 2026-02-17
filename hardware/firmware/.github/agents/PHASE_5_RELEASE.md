# PHASE 5: Release (Docs + RC Build + Launch)

## 📌 Briefing: Release_Agent

**Your mission:** Document Story V2, build RC firmware, prepare client launch materials, and execute the release. This is the final phase; all prior phases must be stable.

**Prerequisites for this phase:**
- ✅ Phases 2-4 complete and stable
- ✅ All tests passing (E2E, smoke, stress)
- ✅ Zero critical blockers
- ✅ Client review + approval in progress

---

### ✅ Required Deliverables (Agent Management)

- Update release-related test scripts (build/RC verification).
- Update AI generation scripts if part of release workflow.
- Update docs (user/install/API/troubleshooting/release notes).

### 📋 Tasks

#### Task 5.1: Documentation

**What:** Write complete user and developer guides for Story V2.

**Documents to create:**

1. **User Guide: Story V2 Scenario Authoring** (`docs/guides/STORY_V2_USER_GUIDE.md`)
   ```markdown
   # Story V2 Scenario Authoring Guide
   
   ## Overview
   Story V2 is an event-driven story engine for embedded systems.
   You define scenarios in YAML; the engine executes them on hardware.
   
   ## Quickstart
   1. Use template: `docs/protocols/story_specs/scenarios/example_unlock_express.yaml`
   2. Edit metadata (id, description, estimated_duration_s)
   3. Add steps: unlock_event -> action_1 -> done
   4. Define transitions: on unlock → step action_1
   5. Deploy via WebUI Designer or CLI
   
   ## Full Spec
   - Schema: `docs/protocols/story_specs/schema/story_spec_v1.yaml`
   - Example scenarios: `docs/protocols/story_specs/scenarios/`
   - Prompts (AI-assisted): `docs/protocols/story_specs/prompts/`
   
   ## Common Patterns
   - Unlock flow: unlock_event → action → result
   - Multi-step: unlock → step1 → step2 → done
   - Loops: step → branching transition → loop back
   - Timing: use afterMs for delays
   
   ## Troubleshooting
   - Validation error: Check schema alignment
   - Deployment fails: Verify device storage (GET /api/story/fs-info)
   - Scenario hangs: Add timeout transitions
   ```

2. **Installation Guide** (`docs/guides/STORY_V2_INSTALL.md`)
   ```markdown
   # Story V2 Installation
   
   ## Hardware
   - ESP32 with WiFi (main processor)
   - RP2040 with TFT/OLED display (UI)
   - Audio module (optional)
   - 4+ GB SPIFFS filesystem for /story/
   
   ## Firmware
   1. Clone repo: `git clone ... le-mystere-professeur-zacus`
   2. Checkout branch: `git checkout story-V2`
   3. Build + flash: `./tools/dev/cockpit.sh flash`
   4. Verify: Serial monitor shows "Story V2 Engine ready"
   
   ## WebUI
   1. Deploy: Copy `ui/` to `/static/` on ESP
   2. Access: http://[ESP_IP]:8080/story-ui
   3. Connected? Look for "Device connected" indicator
   
   ## Initial Setup
   - Factory reset (if needed): `STORY_FS_CLEAR`
   - Deploy default scenarios: See Scenario Authoring Guide
   - Test: Run smoke test (`bash tools/dev/run_smoke_tests.sh`)
   ```

3. **API Reference** (update `docs/protocols/STORY_V2_WEBUI.md`)
   ```markdown
   # Story V2 REST API + WebSocket
   
   ## Endpoints
   
   ### GET /api/story/list
   Lists all available scenarios.
   
   Response: 200 OK
   ```json
   {
     "scenarios": [
       {
         "id": "DEFAULT",
         "estimated_duration_s": 120,
         "description": "Unlock the mystery"
       }
     ]
   }
   ```
   
   ... (continue for all 11 endpoints)
   
   ## WebSocket
   
   ### ws://[ESP]:8080/api/story/stream
   Real-time events.
   ```
   Server -> Client:
   {"type": "step_change", "data": {"current_step": "..."}}
   ```
   ```

4. **Troubleshooting Guide** (`docs/guides/STORY_V2_TROUBLESHOOTING.md`)
   ```markdown
   # Story V2 Troubleshooting
   
   | Problem | Cause | Fix |
   |---------|-------|-----|
   | Scenario won't load | Invalid YAML | Run validator first |
   | WebUI won't connect | Firewall blocks port 8080 | Check router settings |
   | Story hangs mid-execution | Missing transition | Add timeout transition |
   | Device storage full | Too many scenarios | Delete old ones |
   | Serial monitor shows garbage | Wrong baud rate | Use 115200 |
   
   ... (add more troubleshooting)
   ```

**Reference:**
- Spec: `docs/protocols/story_specs/schema/story_spec_v1.yaml`
- Examples: `docs/protocols/story_specs/scenarios/`
- API: `docs/protocols/STORY_V2_WEBUI.md`

**Acceptance Criteria:**
- ✅ User Guide covers authoring, templates, common patterns
- ✅ Installation Guide covers hardware + firmware + WebUI
- ✅ API Reference complete (all 11 endpoints + WebSocket)
- ✅ Troubleshooting Guide covers ≥5 common issues
- ✅ All docs are copy-paste ready (no placeholders)
- ✅ Docs reviewed by team (spelling + clarity)

---

#### Task 5.2: Release Notes

**What:** Document what's new, migration path, known issues, and performance metrics.

**Document: `CHANGELOG.md` (add entry) or `RELEASE_NOTES_V2.md`**

```markdown
# Story V2 Release Notes

**Version:** 2.0.0  
**Date:** Feb 2026  
**Status:** Release Candidate (RC)

## What's New

### Core
- ✨ Event-driven state machine (Story V2 Engine)
- 📁 Filesystem-based scenario storage (no C++ recompilation)
- ⚡ Zero-latency step transitions
- 📊 Real-time audit logging

### WebUI
- 🎨 Responsive Scenario Selector
- ⏱️ Live Orchestration component (pause/resume/skip)
- ✏️ YAML Story Designer (validate + deploy on-device)

### API
- 🔌 REST API (11 endpoints)
- 📡 WebSocket real-time updates
- 🔧 Serial command integration

### Testing
- ✅ E2E test suite (Cypress)
- 🧪 4-hour stress tests
- 📈 CI integration (GitHub Actions)

## Migration Path (V1 → V2)

Old V1 workflow:
1. Write C++ scenario code
2. Compile firmware
3. Flash device

New V2 workflow:
1. Write scenario YAML
2. Deploy via WebUI
3. Device executes immediately

**Benefits:**
- No compilation needed (iterate faster)
- Multiple scenarios on one device
- Non-technical authors can create stories

## Known Issues

1. **Issue:** WebSocket reconnection takes 3-5 seconds
   - Workaround: Wait for "Device connected" indicator before interacting
   
2. **Issue:** Filesystem deployment limited to 4 scenarios per 4MB SPIFFS
   - Workaround: Delete old scenarios before deploying new ones
   
3. **Issue:** Story timings may drift on heavily loaded system
   - Workaround: Use generous afterMs margins; plan for ±500ms variance

## Performance Metrics

| Metric | Value |
|--------|-------|
| Step transition latency | <50ms |
| WebSocket event latency | <100ms |
| Scenario load time | <1sec |
| Concurrent connections | 10+ |
| Memory (4 scenarios loaded) | ~200KB |
| Storage (per scenario) | depends |
| Max execution time | no limit |

## Supported Hardware

- ✅ ESP32 (main processor)
- ✅ RP2040 with TFT/OLED (UI)
- ✅ USB serial (debugging)
- ✅ WiFi (WebUI access)

## Testing Summary

| Test Type | Result | Details |
|-----------|--------|---------|
| Unit tests | ✅ Pass | story_gen.py, StoryFsManager, REST API |
| E2E tests | ✅ Pass | All 6 user workflows |
| Smoke tests | ✅ Pass | 4 scenarios, 40 sec total |
| Stress test | ✅ Pass | 4-hour loop, 1520 iterations, 98% success |

## Breaking Changes

None (initial release).

## Future Roadmap

- [ ] V2.1: Multi-device synchronization
- [ ] V2.2: Branching story paths (player choices)
- [ ] V2.3: Embedded ML (adaptive stories)

## Support

- Docs: `docs/guides/STORY_V2_*.md`
- Issues: GitHub Issues
- Contact: [maintainer email]

---

## Installation

See [Installation Guide](docs/guides/STORY_V2_INSTALL.md).

## Credits

Developed by [team names].
```

**Acceptance Criteria:**
- ✅ Release notes document new features clearly
- ✅ Migration path explained (V1 → V2)
- ✅ Known issues listed with workarounds
- ✅ Performance metrics included
- ✅ Testing summary shows all phases passed
- ✅ Hardware compatibility clear

---

#### Task 5.3: RC Build

**What:** Compile firmware for all target platforms and verify on hardware.

**Platforms to build:**

```
platformio.ini environments:
  - esp32dev (main)
  - esp32_release (optimized variant)
  - esp8266_oled (legacy support)
  - ui_rp2040_ili9488
  - ui_rp2040_ili9486
```

**Build steps:**

```bash
#!/bin/bash

cd hardware/firmware

# 1. Clean old builds
rm -rf .pio/build/*

# 2. Build all firmware variants
echo "Building RC firmware..."
pio run -e esp32dev
pio run -e esp32_release
pio run -e esp8266_oled
pio run -e ui_rp2040_ili9488
pio run -e ui_rp2040_ili9486

# 3. Copy binaries to artifacts
mkdir -p artifacts/rc-v2.0.0
cp .pio/build/esp32dev/firmware.bin artifacts/rc-v2.0.0/story-v2-esp32dev.bin
cp .pio/build/esp32_release/firmware.bin artifacts/rc-v2.0.0/story-v2-esp32-release.bin
cp .pio/build/esp8266_oled/firmware.bin artifacts/rc-v2.0.0/story-v2-esp8266-oled.bin
cp .pio/build/ui_rp2040_ili9488/firmware.bin artifacts/rc-v2.0.0/story-v2-ui-rp2040-ili9488.bin
cp .pio/build/ui_rp2040_ili9486/firmware.bin artifacts/rc-v2.0.0/story-v2-ui-rp2040-ili9486.bin

# 4. Compute checksums
cd artifacts/rc-v2.0.0
sha256sum *.bin > CHECKSUMS.txt

# 5. Verify on hardware (4 units minimum)
echo "Verifying on hardware..."
PORT="${ZACUS_PORT_ESP32:-$(python3 tools/test/resolve_ports.py --need-esp32 --print-esp32 2>/dev/null)}"
if [ -z "$PORT" ]; then
  echo "FAIL: ESP32 port not found. Set ZACUS_PORT_ESP32 or run ./tools/dev/cockpit.sh ports"
  exit 1
fi
for variant in esp32dev esp32_release; do
  echo "Flashing $variant to board..."
  pio run -e $variant --target upload --upload-port "$PORT"
  
  # Run smoke test
  bash ../../tools/dev/run_smoke_tests.sh || { echo "FAIL: $variant"; exit 1; }
  
  echo "✓ $variant verified"
done

echo "✓ RC build complete: artifacts/rc-v2.0.0/"
```

**Artifact structure:**

```
artifacts/rc-v2.0.0/
├── story-v2-esp32dev.bin (2.5 MB)
├── story-v2-esp32-release.bin (2.3 MB)
├── story-v2-esp8266-oled.bin (1.8 MB)
├── story-v2-ui-rp2040-ili9488.bin (1.6 MB)
├── story-v2-ui-rp2040-ili9486.bin (1.6 MB)
├── CHECKSUMS.txt
└── BUILD_LOG.txt (compiler output)
```

**Acceptance Criteria:**
- ✅ All 5 firmware variants compile without errors
- ✅ Binaries generated in artifacts/rc-v2.0.0/
- ✅ Checksums computed and verified
- ✅ Hardware verification on 4+ units (smoke tests pass)
- ✅ Zero compiler warnings (or documented)
- ✅ Build log clean (no unexpected messages)

---

#### Task 5.4: Launch Checklist

**What:** Verify client readiness and get sign-off before release.

**Checklist document: `.github/LAUNCH_CHECKLIST_V2.md`**

```markdown
# Story V2 Launch Checklist

Date: [Release date]
Status: [Ready / Pending]

## Development (Dev Team)

- [ ] All code committed to story-V2 branch
- [ ] Code review completed (all PRs merged)
- [ ] CI passes (all tests green)
- [ ] No compiler warnings
- [ ] No memory leaks (stress test 4h clean)
- [ ] Smoke test passing on hardware (4 units)

## Client Review (Product / Client)

- [ ] User Guide reviewed and approved
- [ ] Installation Guide reviewed and approved
- [ ] Release Notes reviewed and approved
- [ ] Feature demo completed (Selector + Orchestrator + Designer)
- [ ] Scenario authoring demo completed
- [ ] Known issues understood and accepted

## Quality Assurance (QA)

- [ ] E2E test suite: 6/6 passing
- [ ] Smoke test: 4/4 scenarios passing
- [ ] Stress test: 4-hour loop completed (≥98% success)
- [ ] No test failures in CI
- [ ] Test coverage: ≥80%

## Release Readiness (Release Manager)

- [ ] RC firmware built for all 5 platforms
- [ ] Binaries checksummed and verified
- [ ] Download links prepared
- [ ] GitHub release draft created
- [ ] CHANGELOG updated
- [ ] Documentation published

## Hardware Validation (Product / Field Testing)

- [ ] 4× ESP32 boards flashed and tested
- [ ] 4× RP2040 UI boards flashed and tested
- [ ] All 4 scenarios tested on each board
- [ ] WebUI accessible from smartphone
- [ ] No hardware failures or anomalies

## Legal / Compliance (if applicable)

- [ ] License file present (MIT / Apache / other)
- [ ] Attribution acknowledgments included
- [ ] Data privacy review completed
- [ ] No sensitive info in code

## Sign-Off

| Role | Name | Date | Signature |
|------|------|------|-----------|
| Dev Lead | [Name] | [Date] | ☑️ |
| QA Lead | [Name] | [Date] | ☑️ |
| Product Manager | [Name] | [Date] | ☑️ |
| Release Manager | [Name] | [Date] | ☑️ |

**Release approved:** [Yes / No]  
**Go-live date:** [Date]
```

**Approval gates:**
1. All dev tasks complete + CI passing
2. Client review + approval
3. QA sign-off
4. Hardware validation passed
5. All checkboxes marked ☑️

**Sign-off process:**
1. Print or email checklist
2. Each stakeholder reviews and initials
3. Release Manager files copy
4. Proceed to GitHub release

**Acceptance Criteria:**
- ✅ Checklist document created
- ✅ All checklist items clearly defined
- ✅ Sign-off section with roles + dates
- ✅ Shared with team (email + GitHub)
- ✅ All stakeholders have signed off before release

---

#### Task 5.5: Public Release

**What:** Publish GitHub release with binaries and documentation.

**GitHub Release (new tag: `v2.0.0`)**

```
Release Title: 🚀 Story V2: Event-Driven Scenarios (Release Candidate)

Release Notes (auto-generated from CHANGELOG.md + Release Notes):

## What's New
- Event-driven story engine with YAML scenarios
- Responsive WebUI (Selector + Orchestrator + Designer)
- REST API + WebSocket for real-time control
- Filesystem-based scenario storage
- Comprehensive test suite (E2E + stress)

## Installation
See [Installation Guide](docs/guides/STORY_V2_INSTALL.md)

## Getting Started
1. Flash firmware: `./tools/dev/cockpit.sh flash`
2. Access WebUI: http://[ESP_IP]:8080/story-ui
3. Select a scenario and click "Play"

## Download Firmware
- [story-v2-esp32dev.bin](releases/download/v2.0.0/story-v2-esp32dev.bin)
- [story-v2-esp32-release.bin](releases/download/v2.0.0/story-v2-esp32-release.bin)
- [story-v2-esp8266-oled.bin](releases/download/v2.0.0/story-v2-esp8266-oled.bin)
- [story-v2-ui-rp2040-ili9488.bin](releases/download/v2.0.0/story-v2-ui-rp2040-ili9488.bin)
- [story-v2-ui-rp2040-ili9486.bin](releases/download/v2.0.0/story-v2-ui-rp2040-ili9486.bin)

[SHA256 checksums](releases/download/v2.0.0/CHECKSUMS.txt)

## Documentation
- [User Guide](docs/guides/STORY_V2_USER_GUIDE.md)
- [Installation Guide](docs/guides/STORY_V2_INSTALL.md)
- [API Reference](docs/protocols/STORY_V2_WEBUI.md)
- [Troubleshooting](docs/guides/STORY_V2_TROUBLESHOOTING.md)

## Testing
- ✅ E2E tests: 6/6 passing
- ✅ Smoke tests: 4/4 scenarios (40 sec)
- ✅ Stress tests: 4-hour loop (1520 iterations, 98% success)
- 🎯 Ready for production use

## Known Issues
See [Release Notes](RELEASE_NOTES_V2.md#known-issues)

## Support
- Issues: [GitHub Issues](issues)
- Discussions: [GitHub Discussions](discussions)

---

Checksums:
```
[SHA256 checksums from CHECKSUMS.txt]
```
```

**Steps:**

1. **Create git tag:**
   ```bash
   git tag -a v2.0.0 -m "Story V2: Event-Driven Scenarios (RC)"
   git push origin v2.0.0
   ```

2. **Upload binaries to GitHub Release:**
   - Go to Releases → New Release → v2.0.0
   - Copy binaries to Release assets
   - Upload CHECKSUMS.txt
   - Paste release notes

3. **Announce release:**
   - Email team + stakeholders
   - Post to #releases Slack channel
   - Update project README with v2.0.0 badge

4. **Monitor post-release:**
   - Track downloads
   - Monitor Issues for bug reports
   - Prepare v2.0.1 patch (if needed)

**Acceptance Criteria:**
- ✅ GitHub release v2.0.0 published
- ✅ All 5 firmware binaries attached
- ✅ CHECKSUMS.txt present
- ✅ Release notes complete + clear
- ✅ Downloads accessible
- ✅ Team + stakeholders notified

---

### 📋 Acceptance Criteria (Phase 5 Complete)

- ✅ **Documentation** complete
  - User Guide (authoring, templates, patterns)
  - Installation Guide (hardware + firmware + WebUI)
  - API Reference (11 endpoints + WebSocket)
  - Troubleshooting Guide (≥5 issues)
  - All reviewed + approved by team
  
- ✅ **Release Notes** published
  - What's new summarized
  - Migration path documented
  - Known issues + workarounds
  - Performance metrics included
  - Testing summary (all phases passed)
  
- ✅ **RC Build** completed
  - All 5 firmware variants compiled
  - Binaries in artifacts/rc-v2.0.0/
  - Checksums computed + verified
  - Hardware verification passed (4 units)
  - Zero compiler warnings
  
- ✅ **Launch Checklist** signed off
  - All development tasks complete
  - Client review + approval
  - QA sign-off
  - Hardware validation passed
  - All stakeholders signed off
  
- ✅ **GitHub Release** published
  - Tag: v2.0.0
  - Binaries attached + checksummed
  - Release notes + documentation linked
  - Team + stakeholders notified
  - Downloads accessible
  
- ✅ **Commit** to main branch (after approval)
  - story-V2 branch merged to main
  - CI passes on main
  - v2.0.0 tag points to release commit

---

### ⏱️ Timeline

- **Depends on:** Phases 2-4 complete (Mar 16-19)
- **Start:** Mar 16-19
- **ETA:** Mar 23 (Sunday) or Mar 26 (Wednesday) EOD
- **Duration:** ~1 week

---

### 📊 Blockers & Escalation

If you encounter blockers:
1. **Phase 4 tests failing:** Don't proceed to release; escalate to QA
2. **Client review incomplete:** Schedule meeting; clarify concerns
3. **Hardware not available:** Skip hardware validation (document waiver)
4. **GitHub Actions failing:** Fix in Phase 2-4; don't release

---

### 🎯 Deliverables

**On completion, provide:**
1. ✅ Commit hash for Phase 5 work (merged to main)
2. ✅ GitHub release URL (v2.0.0)
3. ✅ Release notes artifact
4. ✅ Launch checklist (with all sign-offs)
5. ✅ Documentation links (published)

**Report to Coordination Hub:**
```
**Phase 5 Complete - RELEASE APPROVED ✅**
- ✅ Documentation: User Guide + Install + API + Troubleshooting
- ✅ Release Notes: Features + migration + known issues + metrics
- ✅ RC Build: 5 firmware variants compiled + hardware verified
- ✅ Launch Checklist: All stakeholders signed off
- ✅ GitHub Release: v2.0.0 published with binaries + checksums
- ✅ Code merged: story-V2 → main (CI passing)
- 📁 Artifacts: Release assets + documentation + checklist
- 🎯 STATUS: PRODUCTION READY ✅

Story V2 is now live! 🎉
```
