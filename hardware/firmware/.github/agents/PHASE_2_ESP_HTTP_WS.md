# PHASE 2: ESP HTTP API + WebSocket

## 📌 Briefing: ESP_Agent

**Coordination Hub update (Feb 16, 2026):**
- Phase 2 code landed on `story-V2`, verification pending.
- cURL script + WebSocket stability checks still need to run.
- Latest smoke run failed (exit code 1); see `artifacts/rc_live/smoke_*.log` for details.

**Your mission:** Implement 11 REST API endpoints + WebSocket integration for Story V2 on the ESP32 HTTP server. This phase depends on Phase 1 (StoryFsManager must be working).

**Prerequisites for this phase:**
- ✅ Phase 1 complete: storyGen.py + StoryFsManager working
- ✅ 4 scenarios deployed and validated
- ✅ Serial commands functional

---

### ✅ Required Deliverables (Agent Management)

- Update HTTP/cURL test script and any related tooling.
- Update AI generation scripts if touched (e.g., deploy/validate helpers).
- Update docs that reflect the API/WebSocket changes.
- Sync changes with the Test & Script Coordinator (cross-team coherence).
- Reference: [docs/TEST_SCRIPT_COORDINATOR.md](docs/TEST_SCRIPT_COORDINATOR.md)

### ⚠️ Watchouts (Audit)

- If `/api/story/validate` or `/api/story/deploy` are stubbed, implement the real behavior or return a clear error.
- Keep endpoint count consistent: 11 core endpoints; `/api/story/serial-command` is optional.
- Ensure WebSocket stability evidence (10+ min) before handoff.

### 📋 Tasks

#### Task 2.1: HTTP Server (Port 8080) — 11 REST Endpoints

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
- ✅ All 11 endpoints implemented and responding
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

#### Task 2.3: Serial ↔ HTTP Bridging (Optional)

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

**What:** Write test suite to validate all 11 endpoints + WebSocket stability.

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

# ... (continue for all 11 endpoints)

# WebSocket test (using wscat)
echo "TEST 12: WebSocket /api/story/stream (30 sec)"
timeout 30 wscat -c "ws://192.168.1.100:8080/api/story/stream" --execute 'ping' || echo "FAIL"

# Optional: serial-command endpoint test if enabled
# echo "TEST 13: POST /api/story/serial-command"
# curl -s -X POST "$ESP_URL/api/story/serial-command" -H "Content-Type: application/json" -d '{"command":"STORY_LOAD_SCENARIO DEFAULT"}' | jq . || echo "FAIL"
```

**Acceptance Criteria:**
- ✅ All 11 endpoints respond with 2xx or expected 4xx status
- ✅ JSON responses parse without errors
- ✅ WebSocket connection stable for 30 seconds
- ✅ No dropped frames during stress test (100+ rapid requests)
- ✅ Test script passes locally (can run manually or in CI)

---

### 📋 Acceptance Criteria (Phase 2 Complete)

- ✅ **11 REST endpoints** implemented and responding
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
  
- ✅ **cURL tests pass** (all 11 endpoints)
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
3. ✅ Endpoint count: confirm 11 core endpoints (plus `/api/story/serial-command` if enabled)
4. ✅ WebSocket stability log: 10 min stream with no drops

**Report to Coordination Hub:**
```
**Phase 2 Complete**
- ✅ 11 REST endpoints implemented + tested
- ✅ WebSocket stable (10+ min stream, no drops)
- ✅ CORS enabled
- ✅ cURL test suite passing
- ✅ Code committed: [commit hash]
- 📁 Artifacts: esp32_audio/tests/test_story_http_api_{timestamp}.log
- 🎯 Next: Phase 3 unblocked (Frontend WebUI)
```
