# Agent Briefings (All Phases)

Version: 1.1  
Date: Feb 16, 2026  
Status: Phase 1 complete; Phases 2-5 queued

---

## 📋 How to Use This Document

**For each phase:**
1. Open a **new conversation** (GitHub Copilot Chat)
2. Copy the **full briefing** for that phase (below)
3. Paste it as the initial user message
4. Agent begins work according to briefing + acceptance criteria

**For coordination:**
- All agents report to Coordination Hub (current conversation) **every 3 days**
- Status format: ✅ Completed / 🔄 In Progress / ⏸️ Blocked / 📋 Next 3 days / 📈 Health
- Blockers escalated immediately

**Approval workflow:**
- Agent reports phase complete
- Coordination Hub verifies checklist
- Coordination Hub approves handoff to next phase(s)

---

## Repo Constraints (All Agents)

- Scope: all files under `hardware/firmware/**`
- TUI/CLI entry: `tools/dev/cockpit.sh` (single entry point)
- Tooling rules: see `AGENTS.md` and `tools/dev/AGENTS.md`
- Logs must go to `hardware/firmware/logs/`
- Artifacts must go to `hardware/firmware/artifacts/`
- Avoid machine-specific serial paths in committed scripts

## Verification By Phase (Minimum Expectations)

- Phase 1 (Backend): run local unit checks; log scenario validation under `artifacts/`
- Phase 2 (ESP API): cURL/WebSocket checks; save output under `artifacts/`
- Phase 3 (WebUI): manual E2E run; capture notes/logs under `logs/` or `artifacts/`
- Phase 4 (QA): smoke + stress runs; store logs under `artifacts/rc_live/`
- Phase 5 (Release): build artifacts + checksums under `artifacts/`

---

# PHASE 1: Backend (story_gen.py + StoryFsManager)

## 📌 Briefing: Backend_Agent

**Your mission:** Implement `story_gen.py` deploy utilities and the `StoryFsManager` class for Story V2 filesystem storage. This phase unblocks all downstream work (Phases 2-5).

---

### 📋 Tasks

#### Task 1.1: story_gen.py — YAML → JSON Conversion

**What:** Refine `esp32_audio/tools/story_gen/story_gen.py` to generate JSON binaries for deployment.

**The process:**
1. **Validate YAML** against `docs/protocols/story_specs/schema/story_spec_v1.yaml`
   - Detect schema mismatches early
   - Warn on optional fields missing (e.g., estimated_duration_s)
   
2. **Generate JSON files** per spec:
   ```
   /story/scenarios/{scenario_id}.json
   /story/apps/{app_id}.json
   /story/screens/{screen_id}.json
   /story/audio/{pack_id}.json
   /story/actions/{action_id}.json
   ```
   - One file per resource (atomic updates)
   - Deterministic ordering (sorted keys)
   - Compact encoding (no whitespace)

3. **Checksum validation**
   - Compute SHA256(JSON content)
   - Write alongside JSON: `/story/{type}/{id}.sha256`
   - On ESP load: verify checksum to detect corruption
   - Warn if mismatch

4. **Deploy to ESP via Serial**
   - Serial command: `STORY_DEPLOY {scenario_id} {archive.tar.gz}`
   - ESP receives tar, extracts to `/story/`
   - Validates checksums post-extract
   - Returns `STORY_DEPLOY_OK` or error

**Reference:**
- Source: `esp32_audio/tools/story_gen/story_gen.py`
- Spec: `docs/protocols/story_specs/schema/story_spec_v1.yaml`
- ESP storage: `esp32_audio/src/story/fs/` (read-only for now)

**Acceptance Criteria:**
- ✅ `story_gen.py validate` exits 0 for valid YAML, non-zero for invalid
- ✅ JSON generation produces deterministic output (same input → same JSON)
- ✅ Checksums computed correctly (verify with `sha256sum`)
- ✅ Deploy tar archive contains all resource files
- ✅ `STORY_DEPLOY` command executes on ESP and logs success/failure

---

#### Task 1.2: StoryFsManager — C++ Filesystem Manager

**What:** Implement `StoryFsManager` class to load and manage Story V2 resources from ESP `/story/` filesystem.

**The class (header + implementation):**
- File: `esp32_audio/src/story/fs/story_fs_manager.h` + `.cpp`

**Interface:**
```cpp
class StoryFsManager {
 public:
  // Lifecycle
  StoryFsManager(const char* story_root = "/story");
  bool init();
  void cleanup();
  
  // Load scenario + resources atomically
  bool loadScenario(const char* scenario_id);
  
  // Resource access (read-only snapshots)
  const StepDef* getStep(const char* step_id);
  const ResourceBindings* getResources(const char* step_id);
  const AppConfig* getAppConfig(const char* app_id);
  
  // Diagnostics
  bool validateChecksum(const char* resource_type, const char* resource_id);
  void listResources(const char* resource_type);  // logs to Serial
  
 private:
  // Filesystem operations (atomic, cached)
  bool loadJson(const char* path, JsonDocument& doc);
  bool verifyChecksum(const char* resource_path);
};

// Per-scenario snapshot
struct StepDef {
  char id[64];
  uint32_t screen_scene_id;
  uint32_t audio_pack_id;
  const char* action_ids[8];
  int action_count;
  const char* app_ids[4];
  int app_count;
  bool mp3_gate_open;
};

struct ResourceBindings {
  uint32_t screen_scene_id;
  uint32_t audio_pack_id;
  const char* action_ids[8];
  int action_count;
};

struct AppConfig {
  char app_id[64];
  char app_type[32];  // "LaDetector", "AudioPack", etc.
  JsonObject params;  // app-specific config
};
```

**Behavior:**
- **Lazy loading:** Load resources on-demand (only when requested)
- **Caching:** Cache loaded JSON in RAM (configurable max size)
- **Atomic:** No partial loads; verify checksum before caching
- **Non-blocking async:** Use `ArduinoJson` streaming for large files
- **Error logging:** Serial logs for all failures (checksum, parse, etc.)

**Reference:**
- JSON schema: `docs/protocols/story_specs/schema/story_spec_v1.yaml`
- Storage layout: `docs/protocols/STORY_V2_APP_STORAGE.md`
- ESP32 filesystem: `SPIFFS` or `LittleFS` (existing config in `platformio.ini`)

**Acceptance Criteria:**
- ✅ `StoryFsManager::init()` succeeds if `/story/` exists with valid checksums
- ✅ `loadScenario(id)` returns true for valid scenarios, false for missing
- ✅ `getStep()` and `getResources()` return correct data from cached JSON
- ✅ `validateChecksum()` detects corrupted files
- ✅ Compiles without warnings or errors
- ✅ Memory footprint ≤50KB for 4 scenarios loaded

---

#### Task 1.3: Serial Commands for Testing

**What:** Implement serial commands for manual testing and validation of story_gen.py + StoryFsManager.

**Commands to implement:**

```
STORY_LOAD_SCENARIO {scenario_id}
  → Calls StoryFsManager::loadScenario(id)
  → Logs: "STORY_LOAD_SCENARIO DEFAULT" → "STORY_LOAD_SCENARIO_OK"

STORY_ARM
  → Prepares story for execution (arms Story V2 engine)
  → Logs: "STORY_ARM_OK" or error

STORY_FORCE_STEP {step_id}
  → Jumps to step (bypasses transitions, for testing)
  → Logs: "STORY_FORCE_STEP unlock_event" → "STORY_FORCE_STEP_OK"

STORY_V2_STATUS
  → Prints Story V2 runtime snapshot
  → Logs: "[STORY_V2] ..." + "STORY_V2_STATUS_OK"

STORY_V2_ENABLE {STATUS|ON|OFF}
  → Enables/disables Story V2 controller
  → Logs: "STORY_V2_ENABLE ..." → "STORY_V2_OK"

STORY_V2_TRACE {STATUS|ON|OFF}
  → Toggles V2 trace logging
  → Logs: "STORY_V2_TRACE ..." → "STORY_V2_OK"

STORY_V2_TRACE_LEVEL {OFF|ERR|INFO|DEBUG}
  → Sets V2 trace verbosity
  → Logs: "STORY_V2_TRACE_LEVEL ..." → "STORY_V2_OK"

STORY_V2_HEALTH
  → Prints health summary snapshot
  → Logs: "[STORY_V2] HEALTH ..." + "STORY_V2_OK"

STORY_FS_LIST {resource_type}
  → Lists resources from /story/ (e.g., "scenarios", "apps", "screens")
  → Logs: [scenario_id, ...] one per line

STORY_FS_VALIDATE {resource_type} {resource_id}
  → Validates checksum for a single resource
  → Logs: "STORY_FS_VALIDATE scenarios DEFAULT" → "OK" or "CHECKSUM_MISMATCH"
```

**Reference:**
- Serial protocol: `esp32_audio/docs/protocols/SERIAL_PROTOCOL.md`
- Story engine: `esp32_audio/src/controllers/story/story_controller_v2.cpp`

**Acceptance Criteria:**
- ✅ All listed commands parse correctly from serial input
- ✅ Commands execute and log results
- ✅ Error messages are clear (e.g., "STORY_LOAD_SCENARIO DEFAULT: NOT_FOUND")
- ✅ No blocking operations (all async or fast sync-only)

---

#### Task 1.4: Unit + Integration Tests

**What:** Write and execute tests for story_gen.py, StoryFsManager, and serial commands.

**Test suite:**

1. **Unit tests: story_gen.py** (Python, `esp32_audio/tests/test_story_gen.py`)
   ```
   test_validate_yaml_valid() → passes for valid YAML
   test_validate_yaml_invalid() → fails for schema mismatch
   test_generate_json_deterministic() → same input → same output
   test_checksum_mismatch() → detect corruption
   test_deploy_tar_creation() → tar contains all files
   ```

2. **Unit tests: StoryFsManager** (C++, `esp32_audio/tests/test_story_fs_manager.cpp`)
  ```
  test_init_creates_cache() → init() succeeds
  test_load_scenario_missing() → returns false for unknown id
  test_validate_checksum_corrupted() → detects mismatch
  ```

3. **Integration tests: 4 scenarios** (Serial test loop, `tools/dev/test_story_4scenarios.py`)
   ```
   For each scenario in (DEFAULT, EXPRESS, EXPRESS_DONE, SPECTRE):
     - Compile firmware with StoryFsManager
     - Flash ESP32
     - Deploy scenario via STORY_DEPLOY
     - Run: STORY_LOAD_SCENARIO {id} → OK
     - Run: STORY_ARM → OK
     - Run: STORY_FORCE_STEP unlock_event → OK
     - Run: STORY_V2_STATUS → OK
     - Verify step transitions (2 steps each, ~10 sec total per scenario)
     - Log success to artifacts/rc_live/test_4scenarios_{date}.log
   ```

**Test data:**
- Scenarios: `docs/protocols/story_specs/scenarios/` (DEFAULT, EXPRESS, EXPRESS_DONE, SPECTRE)
- Prompts: `docs/protocols/story_specs/prompts/` (for reference)

**Acceptance Criteria:**
- ✅ All unit tests pass (Python + C++)
- ✅ All 4 scenarios load + arm + transition successfully
- ✅ Integration test log clean (no errors, no warnings)
- ✅ Test execution time ≤ 5 minutes (per scenario ~40 sec)
- ✅ Artifact logs committed to `artifacts/rc_live/test_4scenarios_{timestamp}.log`

---

### 📋 Acceptance Criteria (Phase 1 Complete)

- ✅ **story_gen.py** deployed and working
  - Validates YAML correctly
  - Generates JSON with correct checksums
  - Deploys tar archive to ESP via serial
  
- ✅ **StoryFsManager** class complete (header + implementation)
  - Loads scenarios and resources from `/story/` FS
  - Caches JSON in RAM
  - Validates checksums on load
  - Compiles without warnings
  
- ✅ **Serial commands** functional
  - All listed commands parse and execute
  - Logging clear and correct
  
- ✅ **4/4 scenarios pass integration tests**
  - DEFAULT, EXPRESS, EXPRESS_DONE, SPECTRE
  - Each: load + arm + transition + cleanup
  - Zero errors
  
- ✅ **Code committed** to `story-V2` branch
  - No merge conflicts
  - All tests passing in CI
  
- ✅ **Documentation updated**
  - Code comments for StoryFsManager public interface
  - README for story_gen.py (usage + examples)

---

### 📚 Reference Documentation

- **Specs:** `docs/protocols/story_specs/schema/story_spec_v1.yaml`
- **Storage:** `docs/protocols/STORY_V2_APP_STORAGE.md`
- **Pipeline:** `docs/protocols/STORY_V2_PIPELINE.md`
- **Scenarios:** `docs/protocols/story_specs/scenarios/`
- **Serial protocol:** `esp32_audio/docs/protocols/SERIAL_PROTOCOL.md`
- **Source files:**
  - `esp32_audio/tools/story_gen/story_gen.py`
  - `esp32_audio/src/story/fs/story_fs_manager.{h,cpp}`
  - `esp32_audio/src/controllers/story/story_controller_v2.cpp`

---

### ⏱️ Timeline

- **Start:** Feb 16 (Monday)
- **ETA:** Feb 20 (Friday) EOD
- **Duration:** ~5 days

**Daily milestones:**
- **Day 1-2:** story_gen.py refinement + testing
- **Day 2-3:** StoryFsManager implementation
- **Day 3-4:** Serial commands + unit tests
- **Day 4-5:** Integration tests (4 scenarios) + docs

---

### 📊 Blockers & Escalation

If you encounter blockers:
1. **Unclear spec:** Escalate to Coordination Hub (current conversation)
2. **Build failures:** Check PlatformIO config in `platformio.ini`
3. **Serial issues:** Verify port + baud (115200 for ESP32)
4. **Filesystem issues:** Check `/story/` mount point on ESP

---

### 🎯 Deliverables

**On completion, provide:**
1. ✅ Commit hash for all Phase 1 work
2. ✅ Test results artifact: `artifacts/rc_live/test_4scenarios_{timestamp}.log`
3. ✅ Integration test summary (scenarios passing)
4. ✅ Line count: story_gen.py (refined), StoryFsManager (.h + .cpp), tests

**Report to Coordination Hub:**
```
**Phase 1 Complete**
- ✅ story_gen.py: YAML → JSON conversion working
- ✅ StoryFsManager: 4/4 scenarios load + arm + transition
- ✅ Serial commands: all listed commands implemented + tested
- ✅ Integration tests: 4 scenarios passed
- ✅ Code committed: [commit hash]
- 📁 Artifacts: artifacts/rc_live/test_4scenarios_[timestamp].log
- 🎯 Next: Phase 2 unblocked (ESP HTTP API)
```

---

---

# PHASE 2: ESP HTTP API + WebSocket

## 📌 Briefing: ESP_Agent

**Your mission:** Implement 12 REST API endpoints + WebSocket integration for Story V2 on the ESP32 HTTP server. This phase depends on Phase 1 (StoryFsManager must be working).

**Prerequisites for this phase:**
- ✅ Phase 1 complete: storyGen.py + StoryFsManager working
- ✅ 4 scenarios deployed and validated
- ✅ Serial commands functional

---

### 📋 Tasks

#### Task 2.1: HTTP Server (Port 8080) — 12 REST Endpoints

**What:** Extend ESP32 HTTP server to expose Story V2 resource + control endpoints.

**Endpoints:**

```
GET /api/story/list
  → Lists all scenarios from StoryFsManager
  → Response: {"scenarios": [{"id": "DEFAULT", "estimated_duration_s": 120, ...}, ...]}
  → Status: 200 OK | 500 Internal Error

POST /api/story/select/{scenario_id}
  → Selects a scenario (loads into engine)
  → Body: {} (empty)
  → Response: {"selected": "DEFAULT", "status": "ready"}
  → Status: 200 OK | 404 Not Found | 400 Bad Request

POST /api/story/start
  → Arms engine and begins execution
  → Body: {}
  → Response: {"status": "running", "current_step": "unlock_event", "started_at_ms": 1234567}
  → Status: 200 OK | 409 Conflict (already running) | 412 Precondition Failed (not selected)

GET /api/story/status
  → Returns current execution state
  → Response: {"status": "running|paused|done|idle", "current_step": "...", "progress_pct": 45, ...}
  → Status: 200 OK

POST /api/story/pause
  → Pauses execution
  → Body: {}
  → Response: {"status": "paused", "paused_at_step": "..."}
  → Status: 200 OK | 409 Conflict (not running)

POST /api/story/resume
  → Resumes from pause
  → Body: {}
  → Response: {"status": "running"}
  → Status: 200 OK | 409 Conflict (not paused)

POST /api/story/skip
  → Skip to next step
  → Body: {}
  → Response: {"current_step": "...", "previous_step": "..."}
  → Status: 200 OK | 409 Conflict (not running)

POST /api/story/validate
  → Validate YAML against schema (optional, for WebUI Designer)
  → Body: {"yaml": "---\nversion: 1\n..."}
  → Response: {"valid": true} | {"valid": false, "errors": ["..."]}
  → Status: 200 OK | 400 Bad Request

POST /api/story/deploy
  → Deploy scenario archive to /story/ FS
  → Body: multipart/form-data or raw binary tar.gz
  → Response: {"deployed": "SCENARIO_ID", "status": "ok"}
  → Status: 200 OK | 400 Bad Request | 507 Insufficient Storage

GET /api/audit/log
  → Returns event history (last N events)
  → Query: ?limit=50 (default 50, max 500)
  → Response: {"events": [{"timestamp": 1234567, "type": "step_change", "data": {...}}, ...]}
  → Status: 200 OK

GET /api/story/fs-info
  → Returns /story/ filesystem info
  → Response: {"total_bytes": 1048576, "used_bytes": 512000, "free_bytes": 536576, "scenarios": 4}
  → Status: 200 OK

POST /api/story/serial-command
  → Runs a Story serial command and returns the response
  → Body: {"command": "STORY_V2_STATUS"}
  → Response: {"command": "...", "response": "...", "latency_ms": 45}
  → Status: 200 OK | 400 Bad Request
```

**Implementation notes:**
- HTTP server library: existing ESP32 framework (likely `AsyncWebServer` or IDF HTTP)
- Port: 8080 (hardcoded or configurable in `platformio.ini`)
- Response format: JSON (use `ArduinoJson` or built-in serialization)
- Status codes: semantic (200, 404, 409, 412, 507)
- Error handling: clear messages (e.g., "Scenario not found: DEFAULT")

**Reference:**
- ESP32 HTTP server: `esp32_audio/src/web/` (existing code)
- Story controller: `esp32_audio/src/controllers/story/story_controller_v2.cpp`
- StoryFsManager: `esp32_audio/src/story/fs/story_fs_manager.h`

**Acceptance Criteria:**
- ✅ All 12 endpoints implemented and responding
- ✅ Correct HTTP status codes (200, 404, 409, etc.)
- ✅ JSON responses valid and schema-compliant
- ✅ Error messages clear and helpful
- ✅ No memory leaks (verified with heap inspector)
- ✅ cURL tests pass for all endpoints (see Task 2.5)

---

#### Task 2.2: WebSocket Integration (ws://esp:8080/api/story/stream)

**What:** Real-time event streaming via WebSocket for story step changes, transitions, and audit log.

**WebSocket contract:**

```
Connection: ws://[ESP_IP]:8080/api/story/stream

Server → Client messages (JSON):
  {
    "type": "step_change",
    "timestamp": 1234567,
    "data": {
      "previous_step": "unlock_event",
      "current_step": "action_1",
      "progress_pct": 25
    }
  }

  {
    "type": "transition",
    "timestamp": 1234567,
    "data": {
      "event": "unlock",
      "transition_id": "unlock_to_action_1"
    }
  }

  {
    "type": "audit_log",
    "timestamp": 1234567,
    "data": {
      "event_type": "step_execute",
      "step_id": "action_1"
    }
  }

  {
    "type": "status",
    "timestamp": 1234567,
    "data": {
      "status": "running|paused|done|idle",
      "memory_free": 51234,
      "heap_pct": 62
    }
  }

  {
    "type": "error",
    "timestamp": 1234567,
    "data": {
      "error_code": 500,
      "message": "StoryFsManager load failed"
    }
  }
```

**Behavior:**
- Broadcast all events to all connected clients (multi-client support)
- Send status ping every 5 seconds (keep-alive)
- Auto-reconnect on client disconnect (no server-side action needed)
- Buffer last N events (configurable, default 50) for late joiners
- No message loss (queue events if buffer full)

**Implementation:**
- WebSocket library: `AsyncWebSocket` or built-in ESP32 IDF
- Event source: Story V2 engine (StoryEngineV2 event queue)
- Bridging: Pump engine events → WebSocket broadcasts

**Reference:**
- WebSocket protocol: `docs/protocols/STORY_V2_WEBUI.md`
- Story event queue: `esp32_audio/src/story/core/story_engine_v2.cpp`

**Acceptance Criteria:**
- ✅ WebSocket endpoint `/api/story/stream` opens and accepts connections
- ✅ Server broadcasts step changes + transitions to all connected clients
- ✅ Ping every 5 seconds keeps connection alive
- ✅ No message drops (stress test: 100+ messages in 10 sec)
- ✅ WebSocket stability verified with `wscat` or Postman (10 min stream)

---

#### Task 2.3: Serial ↔ HTTP Bridging

**What:** Forward STORY_V2 serial commands and responses through HTTP endpoints (optional but useful for testing).

**Contract:**

```
POST /api/story/serial-command
  → Accepts serial command string
  → Body: {"command": "STORY_LOAD_SCENARIO DEFAULT"}
  → Executes on serial layer
  → Response: {"command": "...", "response": "STORY_LOAD_SCENARIO_OK", "latency_ms": 45}
  → Status: 200 OK | 400 Bad Request
```

**Behavior:**
- Parses command string
- Routes to serial handler (same as physical serial input)
- Captures response from serial output
- Returns response + latency to HTTP client
- Timeout: 2 seconds (return error if no response)

**Reference:**
- Serial protocol: `esp32_audio/docs/protocols/SERIAL_PROTOCOL.md`
- Serial handler: `esp32_audio/src/serial_handler.cpp`

**Acceptance Criteria:**
- ✅ Endpoint accepts valid commands
- ✅ Response returned to HTTP client
- ✅ Error handling for invalid commands
- ✅ Timeout works (2 sec max wait)

---

#### Task 2.4: CORS + Error Handling

**What:** Enable CORS for cross-origin requests (smartphone browser to ESP) and provide clear error messages.

**CORS headers:**

```
Access-Control-Allow-Origin: *
Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS
Access-Control-Allow-Headers: Content-Type, Authorization
Access-Control-Max-Age: 3600
```

**Error response format:**

```json
{
  "error": {
    "code": 400,
    "message": "Invalid scenario ID",
    "details": "Scenario 'UNKNOWN' not found in /story/"
  }
}
```

**HTTP status codes:**
- `200 OK`: Success
- `400 Bad Request`: Invalid input
- `404 Not Found`: Resource not found
- `409 Conflict`: State conflict (e.g., already running)
- `412 Precondition Failed`: Missing prerequisite (e.g., scenario not selected)
- `507 Insufficient Storage`: Filesystem full
- `500 Internal Server Error`: Unexpected error

**Reference:**
- CORS spec: https://developer.mozilla.org/en-US/docs/Web/HTTP/CORS
- Story V2 API spec: `docs/protocols/STORY_V2_WEBUI.md`

**Acceptance Criteria:**
- ✅ CORS headers present on all endpoints
- ✅ Preflight requests (OPTIONS) handled correctly
- ✅ Error responses include error code + message + details
- ✅ No sensitive info leaked in error messages

---

#### Task 2.5: Testing — cURL + WebSocket Validation

**What:** Write test suite to validate all 12 endpoints + WebSocket stability.

**Test suite: `esp32_audio/tests/test_story_http_api.sh`**

```bash
#!/bin/bash
# Assumes ESP at 192.168.1.100:8080

ESP_URL="http://192.168.1.100:8080"

# Test 1: GET /api/story/list
echo "TEST 1: GET /api/story/list"
curl -s "$ESP_URL/api/story/list" | jq . || echo "FAIL"

# Test 2: POST /api/story/select
echo "TEST 2: POST /api/story/select/DEFAULT"
curl -s -X POST "$ESP_URL/api/story/select/DEFAULT" -H "Content-Type: application/json" -d '{}' | jq . || echo "FAIL"

# Test 3: POST /api/story/start
echo "TEST 3: POST /api/story/start"
curl -s -X POST "$ESP_URL/api/story/start" -H "Content-Type: application/json" -d '{}' | jq . || echo "FAIL"

# Test 4: GET /api/story/status
echo "TEST 4: GET /api/story/status"
curl -s "$ESP_URL/api/story/status" | jq . || echo "FAIL"

# ... (continue for all 12 endpoints)

# WebSocket test (using wscat)
echo "TEST 12: WebSocket /api/story/stream (30 sec)"
timeout 30 wscat -c "ws://192.168.1.100:8080/api/story/stream" --execute 'ping' || echo "FAIL"
```

**Acceptance Criteria:**
- ✅ All 12 endpoints respond with 2xx or expected 4xx status
- ✅ JSON responses parse without errors
- ✅ WebSocket connection stable for 30 seconds
- ✅ No dropped frames during stress test (100+ rapid requests)
- ✅ Test script passes locally (can run manually or in CI)

---

### 📋 Acceptance Criteria (Phase 2 Complete)

- ✅ **12 REST endpoints** implemented and responding
  - All respond with correct HTTP status codes
  - JSON responses valid and schema-compliant
  - Error messages clear
  
- ✅ **WebSocket** streaming functional
  - `/api/story/stream` accepts connections
  - Broadcasts step changes + transitions
  - Stable for ≥10 minutes (no drops)
  
- ✅ **CORS enabled** for cross-origin requests
  - Headers present on all endpoints
  - OPTIONS preflight handled
  
- ✅ **cURL tests pass** (all 12 endpoints)
  - Test script: `esp32_audio/tests/test_story_http_api.sh`
  - Zero failures
  
- ✅ **Code committed** to `story-V2` branch
  - No merge conflicts
  - CI passes
  
- ✅ **Documentation updated**
  - API reference in `docs/protocols/STORY_V2_WEBUI.md` (if not already)

---

### ⏱️ Timeline

- **Depends on:** Phase 1 complete (Feb 20)
- **Start:** Feb 21 (Saturday) or Feb 24 (Monday)
- **ETA:** Mar 2 (Sunday) or Mar 5 (Wednesday) EOD
- **Duration:** ~2 weeks (parallel with Phase 3)

---

### 📊 Blockers & Escalation

If you encounter blockers, escalate to Coordination Hub:
1. **HTTP server missing:** Check `esp32_audio/src/web/` for existing framework
2. **StoryFsManager not ready:** Phase 1 not complete; wait for handoff
3. **WebSocket library not available:** Use built-in `AsyncWebSocket`
4. **CORS issues:** Debug with browser dev tools + network tab

---

### 🎯 Deliverables

**On completion, provide:**
1. ✅ Commit hash for Phase 2 work
2. ✅ Test results: `esp32_audio/tests/test_story_http_api_{timestamp}.log`
3. ✅ Endpoint count: confirm all 12 implemented
4. ✅ WebSocket stability log: 10 min stream with no drops

**Report to Coordination Hub:**
```
**Phase 2 Complete**
- ✅ 12 REST endpoints implemented + tested
- ✅ WebSocket stable (10+ min stream, no drops)
- ✅ CORS enabled
- ✅ cURL test suite passing
- ✅ Code committed: [commit hash]
- 📁 Artifacts: esp32_audio/tests/test_story_http_api_{timestamp}.log
- 🎯 Next: Phase 3 unblocked (Frontend WebUI)
```

---

---

# PHASE 3: Frontend WebUI (Selector + Orchestrator + Designer)

## 📌 Briefing: Frontend_Agent

**Your mission:** Build a responsive React WebUI for Story V2 with 3 main components: Scenario Selector, Live Orchestrator, and Story Designer. This phase depends on Phase 2 (REST API + WebSocket endpoints must be stable).

**Prerequisites for this phase:**
- ✅ Phase 2 complete: 12 REST endpoints + WebSocket stable
- ✅ cURL tests passing
- ✅ API server running on ESP at http://[ESP_IP]:8080

**Implementation note:**
- Use the existing Vite app under `fronted dev web UI/` (do not re-scaffold).

---

### 📋 Tasks

#### Task 3.1: Scenario Selector Component

**What:** Browse and select scenarios from `/api/story/list`, display metadata, and launch.

**Component spec:**

```
Component: ScenarioSelector
Props:
  - scenarios: [{id, duration_s, description}, ...]
  - onSelect: (scenario_id) => void

UI:
  - Grid or list of scenario cards
  - Each card shows:
    - Scenario ID (large title)
    - Duration (estimated_duration_s)
    - Description (if available)
    - "Play" button
  - On "Play" click:
    1. POST /api/story/select/{id}
    2. POST /api/story/start
    3. Transition to Orchestrator component
    
Responsive:
  - Desktop: 4 columns
  - Tablet: 2 columns
  - Mobile: 1 column (portrait), 2 columns (landscape)

Error handling:
  - Loading spinner while fetching /api/story/list
  - Error message if API fails
  - Retry button
```

**Reference:**
- API: `GET /api/story/list`
- Spec: `docs/protocols/STORY_V2_WEBUI.md`

**Acceptance Criteria:**
- ✅ Component renders scenario list from API
- ✅ Card layout responsive (desktop/tablet/mobile)
- ✅ "Play" button calls select + start endpoints
- ✅ Handles API errors gracefully
- ✅ No lag in responsive transitions (≤100ms)

---

#### Task 3.2: Live Orchestration Component

**What:** Real-time step display, event log, and playback controls.

**Component spec:**

```
Component: LiveOrchestrator
Props:
  - scenario: {id, steps: [...]}
  - onSkip: () => void
  - onPause: () => void
  - onResume: () => void
  - onBack: () => void (return to Selector)

UI Layout:
  - [Top] Current step display
    - Step ID (large, centered)
    - Progress bar (% complete)
    - Status badge (running, paused, done)
  
  - [Middle] Control buttons
    - Pause (if running)
    - Resume (if paused)
    - Skip (advance to next step)
    - Back (return to Selector)
  
  - [Bottom] Event audit log (scrollable history)
    - Timestamp | Event Type | Data (JSON)
    - Auto-scroll to bottom (new events)
    - Max 100 events in view (old events pruned)

WebSocket integration:
  - Connect to ws://[ESP_IP]:8080/api/story/stream
  - Listen for "step_change", "transition", "audit_log" messages
  - Update step display in real-time
  - Append to event log

Responsive:
  - Desktop: status on left, buttons center, log on right
  - Mobile: status full-width, buttons below, log below buttons
  - Landscape: compress buttons into single row

Error handling:
  - Reconnect WebSocket on disconnect
  - Show "Disconnected" alert
  - Retry auto after 3 sec
```

**Reference:**
- WebSocket: `ws://[ESP_IP]:8080/api/story/stream`
- API: `POST /api/story/pause`, `POST /api/story/resume`, `POST /api/story/skip`
- Spec: `docs/protocols/STORY_V2_WEBUI.md`

**Acceptance Criteria:**
- ✅ Step display updates in real-time from WebSocket
- ✅ Control buttons (pause/resume/skip) functional
- ✅ Audit log accumulates and auto-scrolls
- ✅ WebSocket reconnect works on disconnect
- ✅ Responsive layout works (desktop/tablet/mobile)
- ✅ E2E: Select scenario → observe step transitions → skip → back

---

#### Task 3.3: Story Designer Component

**What:** YAML editor for authoring scenarios, with validate and deploy buttons.

**Component spec:**

```
Component: StoryDesigner
Props:
  - onValidate: (yaml) => {valid: bool, errors?: []}
  - onDeploy: (yaml) => {deployed: id, status: 'ok'|'error'}

UI Layout:
  - [Left] YAML editor (textarea or Monaco editor)
    - Syntax highlighting for YAML
    - Line numbers
    - Editable
    - Auto-save to localStorage (draft)
  
  - [Right] Info panel
    - "Validate" button
      → Calls POST /api/story/validate
      → Shows errors or "Valid ✓"
    - "Deploy" button
      → Calls POST /api/story/deploy
      → Shows success or error message
    - "Test Run" button (optional)
      → Deploy + auto-select + start
      → Run for 30 sec preview
      → Return to Selector
    - "Load template" dropdown
      → Load DEFAULT, EXPRESS, EXPRESS_DONE, SPECTRE templates
      → Populate editor

Responsive:
  - Desktop: editor left, panel right (50/50 split)
  - Mobile: editor top, panel bottom
  
Error handling:
  - Validation errors displayed with line numbers
  - Deploy errors show clear message
  - Unsaved changes warning before navigate
```

**Reference:**
- API: `POST /api/story/validate`, `POST /api/story/deploy`
- Spec: `docs/protocols/story_specs/schema/story_spec_v1.yaml` (user reference)
- Templates: `docs/protocols/story_specs/scenarios/`

**Acceptance Criteria:**
- ✅ YAML editor renders and is editable
- ✅ Validate button calls API correctly
- ✅ Deploy button calls API and shows status
- ✅ Template dropdown loads valid YAML
- ✅ Responsive layout works
- ✅ E2E: Load template → validate → deploy → appears in Selector

---

#### Task 3.4: Responsive Design

**What:** Ensure all 3 components are mobile-first and work on smartphone browsers.

**Testing matrix:**

```
Devices:
  - Desktop (1920x1080, landscape)
  - Tablet (768x1024, portrait + landscape)
  - Smartphone (375x667, portrait + landscape)

Scenarios:
  - Landscape → portrait transition (no layout break)
  - Touch vs mouse (buttons sized for touch, ≥44px)
  - Network latency (loading states visible)
  - Offline (WebSocket disconnect handling)

Accessibility:
  - Keyboard navigation (Tab, Enter, Esc)
  - Screen reader support (ARIA labels)
  - Color contrast (WCAG AA)
  - Font size ≥14px

Tools:
  - Chrome DevTools device emulation
  - BrowserStack or similar (optional, for real devices)
```

**Reference:**
- Material Design: https://material.io/design/platform-guidance/android-bars.html
- Bootstrap responsive grid (if using Bootstrap)

**Acceptance Criteria:**
- ✅ All components render on mobile (375px width)
- ✅ Buttons touch-friendly (≥44px)
- ✅ Landscape → portrait transitions smooth
- ✅ No horizontal scrolling on mobile
- ✅ Loading states visible
- ✅ Keyboard navigation works

---

#### Task 3.5: WebSocket Integration

**What:** Establish and maintain WebSocket connection for real-time updates.

**Library:** Use `Socket.io` (easier reconnect) or native `WebSocket` API (simpler, no extra dep).

**Contract:**

```javascript
const ws = new WebSocket('ws://[ESP_IP]:8080/api/story/stream');

ws.onopen = () => console.log('Connected');
ws.onmessage = (event) => {
  const msg = JSON.parse(event.data);
  
  switch(msg.type) {
    case 'step_change':
      updateStepDisplay(msg.data.current_step);
      updateProgress(msg.data.progress_pct);
      break;
    case 'transition':
      logEvent(msg);
      break;
    case 'status':
      updateHealthIndicator(msg.data.memory_free);
      break;
    case 'error':
      showAlert(`Error: ${msg.data.message}`);
      break;
  }
};

ws.onclose = () => {
  showAlert('Disconnected. Retrying...');
  // Auto-reconnect after 3 sec
  setTimeout(() => reconnectWebSocket(), 3000);
};
```

**Behavior:**
- Auto-reconnect: exponential backoff (1s, 2s, 4s, 8s, max 30s)
- Buffer messages during disconnect (last 50 events)
- Show connection status indicator
- Clean up on component unmount

**Acceptance Criteria:**
- ✅ WebSocket connects on component mount
- ✅ Messages parsed and handled correctly
- ✅ Auto-reconnect works on disconnect
- ✅ No memory leaks (unsubscribe on unmount)
- ✅ Stability test: 10 min stream with 500+ message exchanges

---

#### Task 3.6: Error Handling + UX

**What:** Handle edge cases and provide clear feedback to user.

**Error scenarios:**

```
1. ESP offline (API not responding)
   → Show: "Cannot reach device at [IP]. Check connection."
   → Action: Retry button
   
2. Scenario not found
   → Show: "Scenario 'UNKNOWN' not found"
   → Suggest: Browse available scenarios
   
3. Deployment full (507 Insufficient Storage)
   → Show: "Device storage full. Delete old scenarios?"
   → Action: Offer cleanup or abort
   
4. WebSocket disconnected
   → Show: "Live stream disconnected. Retrying..."
   → Auto-reconnect with visual indicator
   
5. Validator error (invalid YAML)
   → Show: "Line 5: Missing field 'steps'"
   → Action: Highlight line in editor

Loading states:
  - Spinner during API calls
  - Skeleton loader for scenario list
  - Progress bar during deployment

Success messages:
  - "Scenario deployed successfully!"
  - "Started running [scenario name]"
  - "Validation passed ✓"
```

**Reference:**
- Material Design error handling: https://material.io/design/communication/messages.html
- HTTP status code meanings: `400 Bad Request`, `404 Not Found`, `409 Conflict`, `507 Insufficient Storage`

**Acceptance Criteria:**
- ✅ All error codes (400, 404, 409, 507) handled gracefully
- ✅ Clear error messages displayed (no tech jargon)
- ✅ Loading states visible during API calls
- ✅ Retry logic for transient failures
- ✅ No unhandled promise rejections (browser console clean)
- ✅ User can recover from any error state

---

### 📋 Acceptance Criteria (Phase 3 Complete)

- ✅ **Scenario Selector** component functional
  - Fetches scenarios from `/api/story/list`
  - Displays cards with metadata
  - "Play" button selects + starts
  
- ✅ **Live Orchestrator** component functional
  - Displays current step in real-time
  - Accepts pause/resume/skip commands
  - Shows audit log
  
- ✅ **Story Designer** component functional
  - YAML editor with syntax highlighting
  - Validate button works (calls API)
  - Deploy button works (calls API)
  - Template dropdown loads samples
  
- ✅ **Responsive design** verified
  - Desktop (1920x1080), Tablet (768x1024), Mobile (375x667)
  - All layouts work in portrait + landscape
  - Touch-friendly buttons
  
- ✅ **WebSocket** stable and auto-reconnecting
  - Real-time step updates
  - Auto-reconnect on disconnect
  - 10+ min stream, no drops
  
- ✅ **Error handling** comprehensive
  - All HTTP error codes handled
  - Clear error messages
  - Loading states + retry logic
  
- ✅ **Code committed** to `story-V2` branch
  - No merge conflicts
  - CI passes (linting, unit tests)
  
- ✅ **Documentation updated**
  - README for WebUI (how to deploy + access)
  - API integration guide (endpoints used)

---

### ⏱️ Timeline

- **Depends on:** Phase 2 complete (Mar 2-5)
- **Start:** Mar 2 (Sunday) or Mar 5 (Wednesday)
- **Parallel with:** Phase 2 (last 1-2 weeks of Phase 2)
- **ETA:** Mar 9 (Sunday) or Mar 12 (Wednesday) EOD
- **Duration:** ~2 weeks

---

### 📊 Blockers & Escalation

If you encounter blockers:
1. **API not responding:** Phase 2 not complete; wait for handoff
2. **WebSocket not connecting:** Check firewall (esp WiFi router)
3. **Deployment fails:** Check device storage; may need cleanup
4. **Responsive layout breaks:** Use DevTools device emulation; test early and often

---

### 🎯 Deliverables

**On completion, provide:**
1. ✅ Commit hash for Phase 3 work
2. ✅ WebUI URL (where deployed: ESP static/, external server, etc.)
3. ✅ E2E test results: select → observe → skip → deploy workflow
4. ✅ Responsive test log (devices tested)

**Report to Coordination Hub:**
```
**Phase 3 Complete**
- ✅ Scenario Selector component working
- ✅ Live Orchestrator component working
- ✅ Story Designer component working
- ✅ Responsive design verified (desktop/tablet/mobile)
- ✅ WebSocket stable (10+ min, no drops)
- ✅ Error handling comprehensive
- ✅ Code committed: [commit hash]
- 📁 Artifacts: [WebUI URL, test results]
- 🎯 Next: Phase 4 unblocked (QA testing)
```

---

---

# PHASE 4: QA Testing (E2E + Smoke + Stress)

## 📌 Briefing: QA_Agent

**Your mission:** Build comprehensive test suite for Story V2 covering E2E scenarios, smoke tests, and 4-hour stress tests. Integrate with CI (GitHub Actions). This phase depends on Phases 2-3 (API + WebUI stable).

**Prerequisites for this phase:**
- ✅ Phase 2 complete: 12 REST endpoints + WebSocket stable
- ✅ Phase 3 complete: WebUI Selector/Orchestrator/Designer functional
- ✅ Both layers integrated and ready for end-to-end testing

---

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

for scenario in DEFAULT EXPRESS EXPRESS_DONE SPECTRE; do
  echo "Smoke test: $scenario"
  
  # 1. Load scenario via serial
  echo "STORY_LOAD_SCENARIO $scenario" > /dev/cu.SLAB_USBtoUART7
  sleep 0.5
  
  # 2. Verify loaded in serial output
  response=$(timeout 2 cat /dev/cu.SLAB_USBtoUART7)
  grep -q "STORY_LOAD_SCENARIO_OK" "$response" || { echo "FAIL: $scenario load"; exit 1; }
  
  # 3. Arm scenario
  echo "STORY_ARM" > /dev/cu.SLAB_USBtoUART7
  sleep 0.5
  
  # 4. Verify armed
  response=$(timeout 2 cat /dev/cu.SLAB_USBtoUART7)
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
import serial
import time
import sys
from pathlib import Path

PORT = '/dev/cu.SLAB_USBtoUART7'
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
\`\`\`bash
cd hardware/firmware
bash tools/dev/run_smoke_tests.sh
\`\`\`

Expected output:
\`\`\`
✓ DEFAULT passed
✓ EXPRESS passed
✓ EXPRESS_DONE passed
✓ SPECTRE passed
✓ All smoke tests passed (40 sec)
\`\`\`

## Unit Tests

C++ tests:
\`\`\`bash
pio run -e esp32dev --target test
\`\`\`

Python tests:
\`\`\`bash
python3 -m pytest esp32_audio/tests/test_story_*.py -v
\`\`\`

## E2E Tests

Prerequisites:
- Firmware flashed and running
- WebUI deployed
- ESP at http://[IP]:8080

Run:
\`\`\`bash
npx cypress run --spec "esp32_audio/tests/e2e/**/*.cy.js"
\`\`\`

## Stress Test (4 hours)

Prerequisites:
- Serial connection to ESP
- Compile latest firmware

Run:
\`\`\`bash
python3 tools/dev/run_stress_tests.py
\`\`\`

Expected: ≥98% success rate, zero panics/reboots

## Troubleshooting

### Test fails: "PANIC: assertion failed"
- Check free heap (may be out of RAM)
- Recompile with optimizations enabled

### Smoke test times out
- Check serial port: `ls /dev/cu.*`
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

---

---

# PHASE 5: Release (Docs + RC Build + Launch)

## 📌 Briefing: Release_Agent

**Your mission:** Document Story V2, build RC firmware, prepare client launch materials, and execute the release. This is the final phase; all prior phases must be stable.

**Prerequisites for this phase:**
- ✅ Phases 2-4 complete and stable
- ✅ All tests passing (E2E, smoke, stress)
- ✅ Zero critical blockers
- ✅ Client review + approval in progress

---

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
   \`\`\`json
   {
     "scenarios": [
       {
         "id": "DEFAULT",
         "estimated_duration_s": 120,
         "description": "Unlock the mystery"
       }
     ]
   }
   \`\`\`
   
  ... (continue for all 12 endpoints)
   
   ## WebSocket
   
   ### ws://[ESP]:8080/api/story/stream
   Real-time events.
   \`\`\`
   Server -> Client:
   {"type": "step_change", "data": {"current_step": "..."}}
   \`\`\`
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
- ✅ API Reference complete (all 12 endpoints + WebSocket)
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
- 🔌 REST API (12 endpoints)
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
for variant in esp32dev esp32_release; do
  echo "Flashing $variant to board..."
  pio run -e $variant --target upload --upload-port /dev/cu.SLAB_USBtoUART7
  
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
\`\`\`
[SHA256 checksums from CHECKSUMS.txt]
\`\`\`
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
  - API Reference (12 endpoints + WebSocket)
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

---

---

## Summary Table

| Phase | Agent | Duration | Key Deliverables | Status |
|-------|-------|----------|------------------|--------|
| 1 | Backend_Agent | 5 days (Feb 16-20) | story_gen.py + StoryFsManager | ⏳ START |
| 2 | ESP_Agent | 2 weeks (Feb 21 - Mar 5) | 12 REST endpoints + WebSocket | ⏳ AFTER 1 |
| 3 | Frontend_Agent | 2 weeks (Feb 21 - Mar 9) | WebUI Selector/Orchestrator/Designer | ⏳ PARALLEL 2 |
| 4 | QA_Agent | 2 weeks (Mar 5-19) | E2E + Smoke + Stress tests + CI | ⏳ AFTER 2-3 |
| 5 | Release_Agent | 1 week (Mar 16-23) | Docs + RC build + GitHub release | ⏳ AFTER 4 |

**Critical path:** Phase 1 → Phase 2 → Phase 3 + Phase 4 → Phase 5  
**Timeline:** 4-5 weeks (Feb 16 - Mar 26)  
**Launch date:** ~Mar 26, 2026 🎉

---

## Next Steps for Coordination Hub

1. ✅ **Plan created** (PHASE_LAUNCH_PLAN.md)
2. ✅ **Briefings drafted** (this document - AGENT_BRIEFINGS.md)
3. 🔄 **Ready to launch**
   - Open Conversation 2 (Backend_Agent)
   - Paste Phase 1 Briefing (above)
   - Backend_Agent begins work (Feb 16)
4. 🔄 **Phases 2-5 queued**
   - Awaiting Phase 1 completion
   - Ready to kick off on Feb 21

---

**Plan status:** ✅ **READY FOR LAUNCH**

**You are cleared to open Conversation 2 and begin Phase 1 with Backend_Agent.** 🚀
