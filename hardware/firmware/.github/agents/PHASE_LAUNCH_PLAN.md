# Plan: Launch 5 Agent Phases (Hybrid Model)

## Execution Model: Hybrid (1→2-5 Parallel)

## Operational Prereqs (All Phases)

- Bootstrap environment: `./tools/dev/bootstrap_local.sh`
- Python tooling: activate `.venv` (local `pyserial`)
- Entry point (TUI/CLI): `tools/dev/cockpit.sh`
- Build gates: `./build_all.sh` or `pio run -e esp32dev esp32_release esp8266_oled ui_rp2040_ili9488 ui_rp2040_ili9486`
- Smoke gates: `./tools/dev/run_matrix_and_smoke.sh`
- Logs: `hardware/firmware/logs/`
- Artifacts: `hardware/firmware/artifacts/`

**Architecture:**
```
┌─────────────────────────────────────────────────────────────┐
│ COORDINATION HUB (This Conversation)                        │
│ Tracks all phases, approves handoffs, manages blockers      │
└─────────────────────────────────────────────────────────────┘

Phase 1 (SEQUENTIAL - BLOCKING)
├─ Conversation 2: Backend_Agent
│  ├─ Task 1.1: story_gen.py deploy (YAML → JSON)
│  ├─ Task 1.2: StoryFsManager implementation
│  ├─ Task 1.3: Serial commands support
│  └─ Task 1.4: Acceptance tests
│  🏁 Deliverable: story_gen.py working + StoryFsManager class
│  ⏱️  ETA: ~5 days (by Friday)

                        ↓ HANDOFF

Phases 2-5 (PARALLEL - INTERDEPENDENT)
├─ Conversation 3: ESP_Agent (needs Phase 1 output)
│  ├─ Task 2.1-2.5: REST API + WebSocket + Serial integration
│  └─ 🏁 Deliverable: 11 API endpoints working
│
├─ Conversation 4: Frontend_Agent (needs Phase 2 endpoints)
│  ├─ Task 3.1-3.6: WebUI React/Vue components
│  └─ 🏁 Deliverable: Selector + Orchestrator + Designer
│
├─ Conversation 5: QA_Agent (needs Phases 2-3)
│  ├─ Task 4.1-4.5: E2E + smoke + stress tests
│  └─ 🏁 Deliverable: Full test suite + CI integration
│
└─ Conversation 6: Release_Agent (needs Phases 2-5)
   ├─ Task 5.1-5.5: Docs + RC build + release notes
   └─ 🏁 Deliverable: RC firmware + launch guide
```

---

## Critical Dependencies

```
Phase 1 (Backend)
   ↓ (story_gen.py output required)
Phase 2 (ESP HTTP API)
   ↓ (REST endpoints + WebSocket required)
   ├→ Phase 3 (Frontend WebUI)
   ├→ Phase 4 (QA tests)
   └→ Phase 5 (Release)
```

**Key Constraint:** Phase 2 cannot be fully completed until Phase 1 deliverables are ready.

---

## Launch Instructions

### 🎯 PHASE 1: Backend_Agent (START IMMEDIATELY)

**Open:** Conversation 2 (new chat)
**Duration:** ~5 days (Feb 16-20)

**Briefing:**
```
You are Backend_Agent. Your mission is to implement story_gen.py deploy
utilities and the StoryFsManager class for Story V2 filesystem storage.

📋 Tasks:
  1.1. story_gen.py: YAML → JSON conversion
       - Validates YAML against story_spec_v1.yaml schema
       - Generates JSON files for /story/scenarios/, /story/apps/, etc.
       - Checksum validation (detect corruption)
       - Deploy to ESP /story/ filesystem via serial

  1.2. StoryFsManager: C++ implementation
       - Load scenarios from /story/ JSON
       - Manage app configs, screen metadata, actions
       - Reusable configs (no C++ recompilation for new scenarios)
       - Async operations (non-blocking)

  1.3. Serial commands for testing
       - STORY_LOAD_SCENARIO {id}
       - STORY_ARM
       - STORY_FORCE_STEP {id}
       - STORY_TEST_ON / STORY_TEST_OFF

  1.4. Unit + integration tests
       - Verify YAML→JSON conversion
       - Test FS loading and state management
       - Acceptance: 4/4 scenarios load + arm + transition

✅ Definition of Done:
   - story_gen.py deployed and working
   - StoryFsManager class complete (header + implementation)
   - 4 test scenarios verified (DEFAULT, EXPRESS, EXPRESS_DONE, SPECTRE)
   - Serial commands functional
   - No C++ engine changes required

📚 Reference:
   - docs/protocols/STORY_V2_APP_STORAGE.md (FS architecture)
   - docs/protocols/story_specs/scenarios/README.md (scenario guide)
   - .github/issues/01-backend-story-gen.md (detailed tasks)

⏱️  Target: Friday Feb 20 EOD
```

**Expected Deliverables:**
- ✅ `esp32_audio/tools/story_gen/story_gen.py` (refined)
- ✅ `esp32_audio/src/story/fs/story_fs_manager.h`
- ✅ `esp32_audio/src/story/fs/story_fs_manager.cpp`
- ✅ Unit test results (all 4 scenarios passing)
- ✅ Commit to `story-V2` branch

---

### 🎯 PHASES 2-5: Parallel Setup (PREPARE NOW, EXECUTE AFTER Phase 1)

**Status:** Ready to launch, waiting for Phase 1 deliverables before implementation can advance significantly.

#### Conversation 3: ESP_Agent (Phase 2)

**Briefing:**
```
You are ESP_Agent. Implement 11 REST API endpoints + WebSocket + Serial 
integration for Story V2 on ESP32 HTTP server.

📋 Tasks:
  2.1. HTTP server: port 8080
       - GET  /api/story/list → list scenarios from /story/
       - POST /api/story/select/:id → select scenario
       - POST /api/story/start → arm and start
       - GET  /api/story/status → current state + step
       - POST /api/story/pause → pause execution
       - POST /api/story/resume → resume
       - POST /api/story/skip → skip to next step
       - POST /api/story/validate → validate YAML
       - POST /api/story/deploy → write JSON to /story/ FS
       - GET  /api/audit/log → event history
       - GET  /api/story/fs-info → /story/ directory info

  2.2. WebSocket: ws://esp:8080/api/story/stream
       - Real-time events (step changes + transitions)
       - Audit log streaming

  2.3. Serial integration
       - Forward STORY_V2 serial commands
       - bidirectional: WebUI ↔ Serial

  2.4. CORS + error handling
       - Allow requests from smartphone browser
       - Clear error messages + retry logic

  2.5. Testing
       - cURL tests for all 11 endpoints
       - WebSocket stability (10 min stream)

✅ Definition of Done:
   - All 11 endpoints implemented and tested
   - WebSocket streaming working
   - CORS headers correct
   - No dropped frames/messages
   - 4 scenario integration tests pass

📚 Reference:
   - docs/protocols/STORY_V2_WEBUI.md (API spec + WebSocket formats)
   - .github/issues/02-esp-http-rest-api.md (detailed tasks)

⏱️  Target: March 2 EOD (after Phase 1 complete)
```

---

#### Conversation 4: Frontend_Agent (Phase 3)

**Briefing:**
```
You are Frontend_Agent. Build the WebUI React/Vue app for Story V2
(Selector + Live Orchestration + Designer).

📋 Tasks:
  3.1. Selector component
       - Browse /api/story/list
       - Display scenario metadata + estimated duration
       - "Play" button → POST /api/story/start

  3.2. Live Orchestration component
       - Real-time step display (from WebSocket)
       - Buttons: Pause, Resume, Skip
       - Event audit log (scrollable history)

  3.3. Story Designer component
       - YAML editor (textarea)
       - "Validate" button → POST /api/story/validate
       - "Deploy" button → POST /api/story/deploy
       - "Test Run" button → 30s preview

  3.4. Responsive design
       - Mobile-friendly (portrait + landscape)
       - Works on smartphone browser (connect to ESP over WiFi)

  3.5. WebSocket integration
       - Auto-reconnect on disconnect
       - Real-time updates (no polling)

  3.6. Error handling + UX
       - Clear error messages
       - Loading states
       - Offline fallback

✅ Definition of Done:
   - All 3 components functional
   - Deploys to ESP /static/ (or external server)
   - WebSocket connection stable
   - 4 scenario E2E tests pass (select, play, observe)
   - Responsive design verified

📚 Reference:
   - docs/protocols/STORY_V2_WEBUI.md (UI design + API integration)
   - .github/issues/03-frontend-webui.md (detailed tasks)

⏱️  Target: March 9 EOD (after Phase 2 endpoints stable)
```

---

#### Conversation 5: QA_Agent (Phase 4)

**Briefing:**
```
You are QA_Agent. Build comprehensive test suite for Story V2
(E2E + smoke + stress + stability).

📋 Tasks:
  4.1. E2E tests: Cypress/Playwright
       - Launch story → observe screen transition
       - Select story → deploy → run → verify completion
       - WebUI → API → Story Engine flow

  4.2. Smoke tests: bash/Python
       - 4 scenario quick tests (40 sec)
       - UI link stability check
       - Serial command execution

  4.3. Stress tests: Python
       - 4-hour continuous loop (1520 scenario executions)
       - Disconnect/reconnect resilience
       - Memory leak detection (heap growth)

  4.4. CI integration
       - GitHub Actions workflow (.github/workflows/firmware-story-v2.yml)
       - Run on every commit (PR gates)
       - Artifact collection (logs, screenshots)

  4.5. Test documentation
       - Test procedures (manual + automated)
       - How to run locally + in CI
       - Troubleshooting guide

✅ Definition of Done:
   - All E2E tests passing
   - 4-hour stress test completing successfully
   - CI pipeline operational
   - Test coverage ≥80%
   - Zero flaky tests (pass rate ≥95%)

📚 Reference:
   - .github/issues/04-qa-testing.md (detailed tasks)
   - tools/dev/run_matrix_and_smoke.sh (existing framework)

⏱️  Target: March 16 EOD (after Phases 2-3 stable)
```

---

#### Conversation 6: Release_Agent (Phase 5)

**Briefing:**
```
You are Release_Agent. Document, build, and release Story V2 RC firmware.

📋 Tasks:
  5.1. Documentation
       - Story V2 user guide (authoring scenarios)
       - Installation guide (ES32 + RP2040 + WebUI)
       - API reference (endpoints + WebSocket formats)
       - Troubleshooting guide (common issues)

  5.2. Release notes
       - What's new in Story V2
       - Migration path from V1 → V2
       - Known issues + workarounds
       - Performance metrics

  5.3. RC build
       - PlatformIO environments: esp32dev + esp8266_oled + ui_rp2040_*
       - Binary artifacts: .bin files
       - Verification: smoke test all 4 scenarios pass

  5.4. Launch checklist
       - Client checklist (SPEC_REVIEW_TEMPLATE.md)
       - Team sign-off (Dev, QA, Release Manager)
       - Board availability (4x hardware validated)

  5.5. Public release
       - GitHub release tag: v2.0.0
       - Download links (binaries)
       - CHANGELOG.md updated

✅ Definition of Done:
   - All docs complete + reviewed
   - RC binaries built + tested on hardware
   - Smoke tests passing (all 4 scenarios)
   - Client approval confirmed
   - GitHub release published

📚 Reference:
   - .github/issues/05-release-docs-rc.md (detailed tasks)
   - docs/protocols/STORY_V2_APP_STORAGE.md + WEBUI.md (reference)
   - CHANGELOG.md (template)

⏱️  Target: March 23 EOD (after all phases complete)
```

---

## Handoff Protocol

### Status Update Template

Use this exact format to keep handoffs consistent:

```
**Status Update: [PHASE N] [Agent Name]**
- ✅ Completed: [task list]
- 🔄 In Progress: [current task] ([% complete])
- ⏸️ Blocked: [blocker description] (requires: [dependency])
- 📋 Next 3 days: [planned work]
- 📈 Health: [GREEN/YELLOW/RED]
```

### Phase 1 → Phases 2-5

**When:** Backend_Agent reports "Phase 1 complete" in Coordination Hub

**Checklist:**
- [ ] story_gen.py deployed and working
- [ ] StoryFsManager class compiled and tested
- [ ] 4/4 scenarios load + arm + transition successfully
- [ ] Serial commands functional
- [ ] Code committed to `story-V2` branch
- [ ] Logs saved under `hardware/firmware/logs/`
- [ ] Artifacts saved under `hardware/firmware/artifacts/`

**Action:** Coordination Hub approves → Phases 2-5 teams begin implementation

---

### Phases 2 → 3

**When:** ESP_Agent reports "Phase 2 endpoints stable" in Coordination Hub

**Checklist:**
- [ ] All 11 REST endpoints implemented
- [ ] WebSocket connection stable (no drops)
- [ ] cURL tests passing for all endpoints
- [ ] Error handling working
- [ ] Logs saved under `hardware/firmware/logs/`
- [ ] Artifacts saved under `hardware/firmware/artifacts/`

**Action:** Coordination Hub approves → Frontend_Agent begins WebUI implementation

---

### Phases 2-3 → 4

**When:** ESP_Agent + Frontend_Agent report "Phase 2-3 stable" in Coordination Hub

**Checklist:**
- [ ] Phase 2: 11 endpoints + WebSocket stable
- [ ] Phase 3: Selector + Orchestrator + Designer working
- [ ] E2E: Story select → play → completion verified
- [ ] Logs saved under `hardware/firmware/logs/`
- [ ] Artifacts saved under `hardware/firmware/artifacts/`

**Action:** Coordination Hub approves → QA_Agent begins E2E/stress test suite

---

### Phases 2-5 → 5

**When:** ESP_Agent + Frontend_Agent + QA_Agent report "All systems green" in Coordination Hub

**Checklist:**
- [ ] Phase 2: REST API + WebSocket stable
- [ ] Phase 3: WebUI responsive + working
- [ ] Phase 4: E2E tests ≥95% pass rate, 4h stress test complete
- [ ] Zero blockers
- [ ] Logs saved under `hardware/firmware/logs/`
- [ ] Artifacts saved under `hardware/firmware/artifacts/`

**Action:** Coordination Hub approves → Release_Agent begins documentation + RC build

---

## Communication Protocol

### Daily Standup (Coordination Hub)

Each agent reports to Coordination Hub **every 3 days** (or daily if blocked):

```
**Status Update: [PHASE N] [Agent Name]**
- ✅ Completed: [task list]
- 🔄 In Progress: [current task] ([% complete])
- ⏸️ Blocked: [blocker description] (requires: [dependency])
- 📋 Next 3 days: [planned work]
- 📈 Health: [GREEN/YELLOW/RED]
```

### Blocker Resolution

If any agent reports **RED**, Coordination Hub:
1. Investigates root cause
2. Escalates to relevant dependencies
3. Proposes solution (code fix, design change, etc.)
4. Approves continuation

### Handoff Approval

When agent reports phase complete:
1. Coordination Hub verifies checklist
2. Reviews code quality + testing
3. Approves handoff to next phase(s)
4. Documents approval in PHASE_STATUS.md

---

## Critical Path Timeline

```
Timeline (Weeks):
──────────────────────────────────────────────────────────────

Week 1 (Feb 16-20)
├─ Phase 1: Backend (5 days)
│  └─ story_gen.py + StoryFsManager + tests
└─ Phases 2-5: Preparation

                    ↓ HANDOFF (Feb 21)

Weeks 2-3 (Feb 21 - Mar 5)
├─ Phase 2: ESP (11 REST + WebSocket) [weeks 2-3]
├─ Phase 3: Frontend (WebUI) [weeks 2-3, starts week 2.5]
└─ Phase 4: QA setup [week 3]

                    ↓ HANDOFF (Week 3)

Weeks 3-4 (Mar 2 - Mar 9)
├─ Phase 4: QA (E2E + stress tests) [weeks 3-4]
└─ Phase 5: Documentation [week 4]

                    ↓ HANDOFF (Week 4)

Week 4+ (Mar 10+)
├─ Phase 5: Release (RC build + launch)
└─ Client sign-off + public release
```

**Critical Milestones:**
- Feb 20: Phase 1 complete → Phase 2 unblocked
- Mar 2: Phase 2 complete → Phase 3 unblocked
- Mar 9: Phase 3 complete + Phase 4 ready
- Mar 16: Phase 4 complete → Phase 5 unblocked
- Mar 23: Phase 5 complete → RC release

---

## Success Criteria

✅ **All Phases Complete When:**
1. Phase 1: story_gen.py + StoryFsManager working, 4/4 scenarios pass
2. Phase 2: 11 REST endpoints + WebSocket stable, cURL tests pass
3. Phase 3: WebUI Selector + Orchestrator + Designer functional, E2E tests pass
4. Phase 4: E2E tests ≥95% pass, 4-hour stress test clean, CI integrated
5. Phase 5: Full docs complete, RC build verified, client approved

**Launch Decision:**
- ✅ All phases complete
- ✅ Zero critical blockers
- ✅ 4/4 hardware units validated
- ✅ Client sign-off documented
- → **RELEASE APPROVED**

---

## FAQ

**Q: Can Phase 2 start before Phase 1 is complete?**  
A: Yes, prepare and plan. Implementation can start Week 2 once Phase 1 deliverables are ready.

**Q: What if Phase 1 hits blockers?**  
A: Phases 2-5 wait (no forward progress). Coordination Hub escalates to unblock Phase 1.

**Q: Can phases overlap?**  
A: Phase 2-5 can run in parallel (independent tasks). Phase 3 depends on Phase 2 API stability.

**Q: How long if all goes well?**  
A: 4 weeks (Feb 16 - Mar 16) for Phases 1-4. Phase 5 (release) takes 1 more week.

**Q: What if a phase misses deadline?**  
A: Shift downstream phases by same delay. Document in PHASE_STATUS.md.

---

## Next Action

✅ **Plan ready for execution**

**You now have 3 options:**

1. **🚀 Launch immediately**
   - Open Conversation 2 (Backend_Agent)
   - Paste the "Phase 1 Briefing" above
   - Backend_Agent begins work

2. **📋 Review & refine first**
   - File any questions about tasks, timeline, or acceptance criteria
   - I'll clarify before agents launch

3. **🔍 Deep-dive on specific phase**
   - Ask about Phase 2, 3, 4, or 5 details
   - I'll expand briefings with code examples, architectures, etc.

**Ready to proceed?**
