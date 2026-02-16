# PHASE 1: Backend (story_gen.py + StoryFsManager)

## 📌 Briefing: Backend_Agent

**Your mission:** Implement `story_gen.py` deploy utilities and the `StoryFsManager` class for Story V2 filesystem storage. This phase unblocks all downstream work (Phases 2-5).

---

### ✅ Required Deliverables (Agent Management)

- Update test scripts relevant to the phase.
- Update AI generation scripts (`story_gen` and related tooling).
- Update docs that reflect the change (README/tests/protocols).

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
