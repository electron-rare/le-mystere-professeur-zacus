# PHASE 3: Frontend WebUI (Selector + Orchestrator + Designer)

## 📌 Briefing: Frontend_Agent

**Your mission:** Build a responsive React/Vue.js WebUI for Story V2 with 3 main components: Scenario Selector, Live Orchestrator, and Story Designer. This phase depends on Phase 2 (REST API + WebSocket endpoints must be stable).

**Prerequisites for this phase:**
- ✅ Phase 2 complete: 11 REST endpoints + WebSocket stable
- ✅ Phase 2B WiFi/RTOS checks green (or explicit waiver)
- ✅ cURL tests passing
- ✅ API server running on ESP at http://[ESP_IP]:8080

---

### ✅ Required Deliverables (Agent Management)

- Update any WebUI test scripts or E2E notes.
- Update AI generation/deploy scripts if UI depends on them.
- Update docs that describe the WebUI and its API usage.
- Sync changes with the Test & Script Coordinator (cross-team coherence).
- Reference: [docs/TEST_SCRIPT_COORDINATOR.md](docs/TEST_SCRIPT_COORDINATOR.md)

### ⚠️ Watchouts (Audit)

- If UI link is disconnected, surface a clear warning and avoid false “green” runs.
- Treat API timeouts as hard errors, not silent retries.

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
