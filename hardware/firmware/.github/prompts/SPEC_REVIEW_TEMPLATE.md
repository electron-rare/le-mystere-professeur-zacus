# Story V2 Spec Review Template

**Purpose:** Validate the Story V2 architecture with client before implementation.

---

## 1. Architecture: Story_n_CodePackApp

### Definition

**Story_n_CodePackApp** is a **complete, autonomous app** (not a wrapper):

- Manages app lifecycle (begin, start, update, handleEvent, stop)
- Coordinates resources (screens, audio, custom bindings)
- Handles event routing (unlock, audio_done, timer, serial, key)
- Configurable via JSON (hold_ms, timeout_ms, key_sequence, etc.)
- Extensible for future AI-generated apps

**Approval:**
- [ ] Definition OK
- [ ] Not a wrapper (autonomous) — APPROVED
- [ ] Extensible for AI — APPROVED
- [ ] Event-driven approach — APPROVED

---

## 2. Filesystem Storage (NEW)

### FS Structure: /story/

```
/story/
├── scenarios/           # JSON scenario definitions
├── apps/                # App config JSON files (reusable)
├── screens/             # Screen metadata JSON (device-specific)
├── actions/             # Binary blobs (optional, custom hardware)
└── audit.log           # Runtime event log (text)
```

### Key Features

- **YAML → JSON conversion** (`story_gen.py deploy`)
- **No C++ recompile** for new scenarios
- **Checksum validation** (detect corruption)
- **Reusable configs** (apps, screens, actions)

**Approval:**
- [ ] FS structure OK
- [ ] YAML→JSON conversion OK
- [ ] Checksum validation OK
- [ ] No C++ recompile — APPROVED

**See:** [STORY_V2_APP_STORAGE.md](../protocols/STORY_V2_APP_STORAGE.md)

---

## 3. WebUI (NEW)

### Features

#### Story Selector
- Browse scenarios on ESP /story/
- Click "Play" to start
- Estimated duration + metadata

#### Live Orchestration
- Pause/Resume/Skip buttons
- Real-time WebSocket stream (step changes + events)
- Audit log viewer (scrollable event history)

#### Story Designer
- YAML editor textarea
- "Validate" button (instant feedback)
- "Deploy" button (write to ESP FS in seconds)
- "Test Run" button (30s preview)

### API

**REST Endpoints (11 total):**
- GET  /api/story/list
- POST /api/story/select/:id
- POST /api/story/start
- GET  /api/story/status
- POST /api/story/pause
- POST /api/story/resume
- POST /api/story/skip
- POST /api/story/validate
- POST /api/story/deploy
- GET  /api/audit/log
- GET  /api/story/fs-info

**WebSocket Stream:**
- ws://esp:8080/api/story/stream (real-time events)

**Approval:**
- [ ] Story Selector OK
- [ ] Live Orchestration OK
- [ ] Story Designer OK
- [ ] 11 REST endpoints OK
- [ ] WebSocket streaming OK

**See:** [STORY_V2_WEBUI.md](../protocols/STORY_V2_WEBUI.md)

---

## 4. Event Types

| Event | Trigger | Use |
|-------|---------|-----|
| `unlock` | LA (60s timeout, >3s cumul) OR key sequence | Transition from locked |
| `audio_done` | Audio pack finished | Next step |
| `timer` | After N milliseconds | Pause/delays |
| `serial` | Command (FORCE_STEP, SKIP) | Testing |
| `key` | Key press (K1, K2, K3) | Puzzles |

**Approval:**
- [ ] All 5 event types — APPROVED
- [ ] LA behavior (60s / >3s) — APPROVED
- [ ] Serial commands OK
- [ ] Key sequences OK

---

## 5. Constraints & Limits

| Constraint | Value | Reason |
|-----------|-------|--------|
| Max steps/scenario | 100 | Memory bloat prevention |
| Max transitions/step | 5 | Branching reasonableness |
| Max audio packs/step | 1 | State simplicity |
| LA hold_ms range | 1s–10s | User intentionality |
| LA timeout_ms range | 30s–5min | Prevent hangs |

**Recommended defaults:** hold_ms=3000, timeout_ms=60000

**Approval:**
- [ ] All constraints OK
- [ ] Defaults suitable
- [ ] REVISIONS (describe below)

---

## 6. Extensibility Model

New apps should follow **Story_n_CodePackApp pattern**:

```cpp
class CustomApp : public StoryApp {
  void begin(context);
  void start(stepContext);
  void update(nowMs, sink);
  void stop(reason);
  void handleEvent(event, sink);
  String snapshot();
};
```

**Configuration via YAML:**
```yaml
app_bindings:
  - id: "APP_CUSTOM"
    app: "CustomPuzzleApp"
    config:
      puzzle_type: "color_sequence"
      difficulty: "medium"
      timeout_ms: 60000
```

**AI can generate:** New apps with custom config + logic, reusing the pattern.

**Approval:**
- [ ] Pattern is extensible — APPROVED
- [ ] AI can generate apps — APPROVED
- [ ] YAML-driven approach — APPROVED

---

## 7. Client Validation Checklist

### Architecture
- [ ] Story_n_CodePackApp is autonomous (not wrapper)
- [ ] Pattern extensible for AI-generated apps
- [ ] Event-driven transitions
- [ ] No C++ changes per scenario

### Filesystem
- [ ] /story/ structure makes sense
- [ ] YAML→JSON conversion acceptable
- [ ] Checksum validation adds safety
- [ ] FS deployment workflow is practical

### WebUI
- [ ] Story Selector useful
- [ ] Live Orchestration covers needs
- [ ] Story Designer enables rapid iteration
- [ ] REST API + WebSocket sufficient

### Events
- [ ] All 5 event types needed
- [ ] LA behavior (60s / >3s) correct
- [ ] Serial commands for testing sufficient
- [ ] Key sequences supported

### Extensibility
- [ ] Future apps can follow pattern
- [ ] AI-generated apps are feasible
- [ ] No blocker to extensibility

---

## 8. Sign-Off

| Role | Name | Date | Signature |
|------|------|------|-----------|
| Client | ______ | __/__/__ | ______ |
| Dev Lead | ______ | __/__/__ | ______ |
| QA | ______ | __/__/__ | ______ |

---

## 9. Next Steps (Post-Approval)

1. [ ] Implement story_gen.py deploy (YAML → JSON)
2. [ ] Build StoryFsManager (ESP loader)
3. [ ] Implement 11 REST API endpoints
4. [ ] Build WebUI React/Vue app
5. [ ] Test WebSocket streaming
6. [ ] Validate with AI-generated scenario
7. [ ] Begin RC firmware build

---

**Reference Documents:**
- [STORY_V2_APP_STORAGE.md](../protocols/STORY_V2_APP_STORAGE.md)
- [STORY_V2_WEBUI.md](../protocols/STORY_V2_WEBUI.md)
- [example_story_n_codepack.yaml](../protocols/story_specs/scenarios/example_story_n_codepack.yaml)
