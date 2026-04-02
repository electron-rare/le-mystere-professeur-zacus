# Zacus NPC Firmware Integration — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development

**Goal:** Wire the NPC engine into scenario_manager, network_manager, and hotline system for end-to-end gameplay.

**Architecture:** NPC engine receives events from scenario_manager, decides responses, routes audio to RTC_PHONE via ESP-NOW or live TTS via WiFi.

**Tech Stack:** ESP-IDF, C, FreeRTOS, ESP-NOW, HTTP client

**Existing code references:**
- NPC engine header: `ESP32_ZACUS/ui_freenove_allinone/include/npc/npc_engine.h`
- NPC engine implementation: `ESP32_ZACUS/ui_freenove_allinone/src/npc/npc_engine.cpp`
- Scenario manager: `ESP32_ZACUS/ui_freenove_allinone/include/app/scenario_manager.h`
- Network manager: `ESP32_ZACUS/ui_freenove_allinone/include/system/network/network_manager.h`
- TTS client: `ESP32_ZACUS/ui_freenove_allinone/include/npc/tts_client.h`
- Audio Kit client (hotline): `ESP32_ZACUS/ui_freenove_allinone/include/npc/audio_kit_client.h`
- QR scan controller: `ESP32_ZACUS/ui_freenove_allinone/include/ui/qr/qr_scan_controller.h`
- Main runtime loop: `ESP32_ZACUS/ui_freenove_allinone/src/app/main.cpp`
- Scenario def types: `ESP32_ZACUS/ui_freenove_allinone/include/core/scenario_def.h`

**Key global instances in main.cpp:**
- `ScenarioManager g_scenario` — scene transitions, step events
- `NetworkManager g_network` — WiFi / ESP-NOW
- `AudioManager g_audio` — local I2S audio
- `CameraManager g_camera` — used by QR scanner

**NPC engine API summary:**
- `npc_init(state)` / `npc_reset(state)` — lifecycle
- `npc_evaluate(state, now_ms, out)` → bool — produces `npc_decision_t` with trigger, audio_source, sd_path
- `npc_on_scene_change(state, scene, expected_ms, now_ms)` — feed from scenario_manager
- `npc_on_qr_scan(state, valid, now_ms)` — feed from QR scanner
- `npc_on_phone_hook(state, off_hook)` — feed from audio_kit_client poll
- `npc_on_hint_request(state, now_ms)` — feed when phone picked up while stuck
- `npc_on_tower_status(state, reachable)` — feed from tts_check_health
- `npc_update_mood(state, now_ms)` — call in tick

**Audio routing decision:**
- `NPC_AUDIO_LIVE_TTS` → `audio_kit_play_tts(text, tts_url, voice)` (tower reachable)
- `NPC_AUDIO_SD_CONTEXTUAL` / `NPC_AUDIO_SD_GENERIC` → `audio_kit_play_sd(sd_path)` (tower down)

---

## Task 1: NPC global state in main.cpp

**Time:** 5 min | **Deps:** None | **Type:** Edit existing file

**File:** `ESP32_ZACUS/ui_freenove_allinone/src/app/main.cpp`

- [ ] 1.1 Add includes at the top of `main.cpp` (after existing npc includes if any, else after the analytics include):

```cpp
#include "npc/npc_engine.h"
#include "npc/tts_client.h"
#include "npc/audio_kit_client.h"
```

- [ ] 1.2 Add global state in the anonymous namespace alongside `g_scenario`, `g_network`, etc.:

```cpp
npc_state_t g_npc_state;
static constexpr const char* kAudioKitBaseUrl = "http://192.168.0.XXX:8300";
static constexpr const char* kTtsPiperUrl    = "http://" TTS_TOWER_IP ":" STRINGIFY(TTS_TOWER_PORT);
static constexpr const char* kTtsVoice       = TTS_VOICE;
```

Note: `STRINGIFY` may not exist — use a raw string literal instead:

```cpp
static constexpr const char* kTtsPiperUrl = "http://192.168.0.120:8001";
```

The Audio Kit IP must be confirmed from the actual hardware config (placeholder `XXX` for now, guarded by a `#warning`).

---

## Task 2: NPC init and Audio Kit init in setup()

**Time:** 5 min | **Deps:** Task 1 | **Type:** Edit existing file

**File:** `ESP32_ZACUS/ui_freenove_allinone/src/app/main.cpp`

- [ ] 2.1 In `setup()`, after `g_scenario` and `g_network` are started, add:

```cpp
  // NPC engine
  npc_init(&g_npc_state);
  tts_init();
  audio_kit_client_init(kAudioKitBaseUrl);
```

Placement: after `g_network.begin(...)` and before the main loop task spawn, to ensure WiFi is up before TTS health check fires.

---

## Task 3: Scene change → NPC notification

**Time:** 10 min | **Deps:** Task 1, 2 | **Type:** Edit existing file

**File:** `ESP32_ZACUS/ui_freenove_allinone/src/app/main.cpp`

**Context:** `ScenarioManager::consumeSceneChanged()` returns true once per transition. The current pattern in `runRuntimeIteration()` already polls `g_scenario.consumeSceneChanged()`.

- [ ] 3.1 Locate the `if (g_scenario.consumeSceneChanged())` block in `runRuntimeIteration()`.

- [ ] 3.2 Inside that block, after the existing scene handling, add NPC notification:

```cpp
    {
      const ScenarioSnapshot snap = g_scenario.snapshot();
      if (snap.step != nullptr) {
        // Map step index to NPC scene index (step position in scenario array).
        // Use the step id hash or a simple sequential counter; for now use
        // the raw step pointer offset from scenario base.
        const ScenarioDef* scen = snap.scenario;
        uint8_t npc_scene = 0U;
        if (scen != nullptr) {
          for (uint8_t i = 0U; i < scen->stepCount; ++i) {
            if (&scen->steps[i] == snap.step) {
              npc_scene = i;
              break;
            }
          }
        }
        // Expected scene duration: default 3 min; override per-scene if known.
        constexpr uint32_t kDefaultSceneDurationMs = 3UL * 60UL * 1000UL;
        npc_on_scene_change(&g_npc_state, npc_scene, kDefaultSceneDurationMs, now_ms);
        npc_on_tower_status(&g_npc_state, tts_is_tower_reachable());
      }
    }
```

---

## Task 4: NPC tick and decision dispatch

**Time:** 15 min | **Deps:** Task 3 | **Type:** Edit existing file

**File:** `ESP32_ZACUS/ui_freenove_allinone/src/app/main.cpp`

- [ ] 4.1 Add a `kNpcTickIntervalMs` constant (evaluate NPC every 5 s, not every loop iteration):

```cpp
constexpr uint32_t kNpcTickIntervalMs = 5000U;
```

- [ ] 4.2 Add a `g_npc_last_tick_ms` variable in the anonymous namespace:

```cpp
uint32_t g_npc_last_tick_ms = 0U;
```

- [ ] 4.3 In `runRuntimeIteration()`, add the NPC tick block after QR scan handling (Task 5) and Tower health update:

```cpp
  // NPC tick
  tts_health_tick(now_ms);
  npc_on_tower_status(&g_npc_state, tts_is_tower_reachable());

  if (now_ms - g_npc_last_tick_ms >= kNpcTickIntervalMs) {
    g_npc_last_tick_ms = now_ms;
    npc_update_mood(&g_npc_state, now_ms);

    npc_decision_t decision;
    if (npc_evaluate(&g_npc_state, now_ms, &decision)) {
      npc_dispatch_decision(&decision);
    }
  }
```

- [ ] 4.4 Implement `npc_dispatch_decision()` as a file-local static function in `main.cpp`, above `runRuntimeIteration()`:

```cpp
static void npc_dispatch_decision(const npc_decision_t* decision) {
  if (decision == nullptr) return;

  switch (decision->audio_source) {
    case NPC_AUDIO_LIVE_TTS:
      if (strlen(decision->phrase_text) > 0U) {
        audio_kit_play_tts(decision->phrase_text, kTtsPiperUrl, kTtsVoice);
      }
      break;

    case NPC_AUDIO_SD_CONTEXTUAL:
    case NPC_AUDIO_SD_GENERIC:
      if (strlen(decision->sd_path) > 0U) {
        audio_kit_play_sd(decision->sd_path);
      }
      break;

    case NPC_AUDIO_NONE:
    default:
      break;
  }
}
```

---

## Task 5: QR scan → NPC notification

**Time:** 10 min | **Deps:** Task 1, 2 | **Type:** Edit existing file

**File:** `ESP32_ZACUS/ui_freenove_allinone/src/app/main.cpp`

**Context:** `QrScanController` exists at `include/ui/qr/qr_scan_controller.h`. Check whether `g_qr_scanner` is already instantiated in main.cpp or needs to be added.

- [ ] 5.1 Search for `QrScanController` or `qr_scan` in `main.cpp`. If not present, add:

```cpp
// In anonymous namespace globals:
ui::QrScanController g_qr_scanner;
```

And in `setup()` after hardware init:

```cpp
  g_qr_scanner.begin();
  g_qr_scanner.setEnabled(true);
```

- [ ] 5.2 In `runRuntimeIteration()`, add QR poll and NPC notification:

```cpp
  // QR scanner → NPC
  {
    ui::QrScanResult qr_result;
    if (g_qr_scanner.poll(&qr_result, 0U)) {
      const bool valid = qr_result.decoder_valid && qr_result.payload_len > 0U;
      npc_on_qr_scan(&g_npc_state, valid, now_ms);

      // Also forward as scenario action event if valid
      if (valid) {
        g_scenario.notifyActionEvent(qr_result.payload, now_ms);
      }
    }
  }
```

---

## Task 6: Phone hook state → NPC notification

**Time:** 10 min | **Deps:** Task 1, 2 | **Type:** Edit existing file

**File:** `ESP32_ZACUS/ui_freenove_allinone/src/app/main.cpp`

**Context:** `audio_kit_poll_status()` returns `audio_kit_status_t` including `phone_off_hook`. Need a periodic poll (every 500 ms as per the comment in `audio_kit_client.h`).

- [ ] 6.1 Add constants and state:

```cpp
constexpr uint32_t kAudioKitPollIntervalMs = 500U;
uint32_t g_audio_kit_last_poll_ms = 0U;
bool g_phone_was_off_hook = false;
```

- [ ] 6.2 In `runRuntimeIteration()`, add the hotline poll block:

```cpp
  // Audio Kit hotline poll
  if (now_ms - g_audio_kit_last_poll_ms >= kAudioKitPollIntervalMs) {
    g_audio_kit_last_poll_ms = now_ms;
    audio_kit_status_t ak_status = {};
    if (audio_kit_poll_status(&ak_status) == ESP_OK) {
      npc_on_phone_hook(&g_npc_state, ak_status.phone_off_hook);

      // Detect rising edge: phone just picked up while stuck → hint request
      if (ak_status.phone_off_hook && !g_phone_was_off_hook) {
        const uint32_t scene_elapsed = now_ms - g_npc_state.scene_start_ms;
        if (scene_elapsed > NPC_STUCK_TIMEOUT_MS) {
          npc_on_hint_request(&g_npc_state, now_ms);
        }
      }
      g_phone_was_off_hook = ak_status.phone_off_hook;
    }
  }
```

---

## Task 7: Game start / game end NPC triggers

**Time:** 5 min | **Deps:** Task 4 | **Type:** Edit existing file

**File:** `ESP32_ZACUS/ui_freenove_allinone/src/app/main.cpp`

**Context:** The scenario uses specific step IDs like `SCENE_U_SON_PROTO` for start and `SCENE_FINAL_WIN` for end. These map to the scene change callback already wired in Task 3. However, we also need to fire `NPC_TRIGGER_GAME_START` and `NPC_TRIGGER_GAME_END` explicitly at the scenario boundaries.

- [ ] 7.1 In the scene change block (Task 3), after `npc_on_scene_change(...)`, detect start/end scenes by step id and directly dispatch a game-start/end audio:

```cpp
        if (snap.step->id != nullptr) {
          if (strcmp(snap.step->id, "SCENE_U_SON_PROTO") == 0) {
            // Game start
            npc_decision_t start_decision = {};
            start_decision.trigger = NPC_TRIGGER_GAME_START;
            start_decision.audio_source = tts_is_tower_reachable()
                ? NPC_AUDIO_LIVE_TTS : NPC_AUDIO_SD_GENERIC;
            npc_build_sd_path(start_decision.sd_path, sizeof(start_decision.sd_path),
                              0U, NPC_TRIGGER_GAME_START, NPC_MOOD_NEUTRAL, 0U);
            npc_dispatch_decision(&start_decision);
          } else if (strcmp(snap.step->id, "SCENE_FINAL_WIN") == 0) {
            // Game end
            npc_decision_t end_decision = {};
            end_decision.trigger = NPC_TRIGGER_GAME_END;
            end_decision.audio_source = tts_is_tower_reachable()
                ? NPC_AUDIO_LIVE_TTS : NPC_AUDIO_SD_GENERIC;
            npc_build_sd_path(end_decision.sd_path, sizeof(end_decision.sd_path),
                              0U, NPC_TRIGGER_GAME_END, NPC_MOOD_NEUTRAL, 0U);
            npc_dispatch_decision(&end_decision);
          }
        }
```

---

## Task 8: NPC reset on game reset

**Time:** 3 min | **Deps:** Task 1, 2 | **Type:** Edit existing file

**File:** `ESP32_ZACUS/ui_freenove_allinone/src/app/main.cpp`

- [ ] 8.1 Find the existing `g_scenario.reset()` call in `main.cpp` (used when game restarts).

- [ ] 8.2 Immediately after `g_scenario.reset()`, add:

```cpp
  npc_reset(&g_npc_state);
  g_phone_was_off_hook = false;
  g_npc_last_tick_ms = 0U;
```

---

## Task 9: Mock scenario integration test (host-side)

**Time:** 15 min | **Deps:** Tasks 1–8 | **Type:** New test file

**File:** `ESP32_ZACUS/test/test_npc_integration/test_npc_integration.cpp`

This test runs on host (Unity framework via PlatformIO `test` environment). It does NOT require hardware — it exercises the pure-C NPC state machine with a mock timeline.

- [ ] 9.1 Create test file with the following test cases:

**Test 1 — scene change propagates correctly:**
- `npc_init(&s)` → `npc_on_scene_change(&s, 0, 180000, 1000)` → verify `s.current_scene == 0`, `s.scene_start_ms == 1000`.

**Test 2 — stuck timer triggers hint decision:**
- Init, set `scene_start_ms = 0`, `expected_scene_duration_ms = 180000`, advance `now_ms` past `NPC_STUCK_TIMEOUT_MS`.
- `npc_evaluate(&s, now_ms, &d)` → must return true, `d.trigger == NPC_TRIGGER_STUCK_TIMER`.

**Test 3 — phone off-hook while stuck triggers hint request:**
- Init, advance past stuck timeout, call `npc_on_phone_hook(&s, true)` then `npc_on_hint_request(&s, now_ms)`.
- `npc_evaluate(&s, now_ms, &d)` → `d.trigger == NPC_TRIGGER_HINT_REQUEST`.

**Test 4 — QR scan increments counter:**
- Init, `npc_on_qr_scan(&s, true, 5000)` → `s.qr_scanned_count == 1`.
- `npc_on_qr_scan(&s, false, 6000)` → `s.failed_attempts == 1`.

**Test 5 — fast progress mood:**
- Init with `expected_scene_duration_ms = 60000`, call `npc_update_mood(&s, 20000)` (33% elapsed < 50%) → `s.mood == NPC_MOOD_IMPRESSED`.

**Test 6 — SD path generation:**
- `npc_build_sd_path(buf, sizeof(buf), 1, NPC_TRIGGER_HINT_REQUEST, NPC_MOOD_WORRIED, 2)` → expect path starts with `/hotline_tts/SCENE_LA_DETECTOR/indice_worried_2`.

**Test 7 — tower down forces SD fallback:**
- Init, `npc_on_tower_status(&s, false)`, advance past stuck timeout.
- `npc_evaluate(&s, now_ms, &d)` → `d.audio_source == NPC_AUDIO_SD_CONTEXTUAL`.

- [ ] 9.2 Add the test environment to `platformio.ini` if not already present:

```ini
[env:test_native]
platform = native
test_framework = unity
build_flags = -std=c++17
```

---

## Integration Checklist

Before claiming done, verify each wiring point:

- [ ] `npc_init` called from `setup()` before main loop
- [ ] `tts_init` + `audio_kit_client_init` called from `setup()`
- [ ] `consumeSceneChanged()` block calls `npc_on_scene_change()`
- [ ] QR poll calls `npc_on_qr_scan()` on every valid/invalid result
- [ ] Audio Kit poll (500 ms) calls `npc_on_phone_hook()` and edge-detects hint request
- [ ] NPC tick (5 s) calls `npc_update_mood()` then `npc_evaluate()` then `npc_dispatch_decision()`
- [ ] `npc_dispatch_decision()` routes to `audio_kit_play_tts` or `audio_kit_play_sd`
- [ ] Game start/end scenes fire immediate NPC decisions
- [ ] `npc_reset()` called alongside `g_scenario.reset()`
- [ ] All 7 Unity tests pass on native target

## Notes for Implementors

- **Audio Kit IP**: The actual IP of the telephone module must be filled in `kAudioKitBaseUrl`. Add a `#warning "Set kAudioKitBaseUrl to actual Audio Kit IP"` guard until confirmed.
- **Phrase text for live TTS**: `npc_decision_t.phrase_text` is populated by `npc_evaluate()` only when the phrase bank is loaded. For now, `phrase_text` will be empty for SD fallback decisions. Live TTS decisions need a separate phrase lookup step — this is tracked in the next sprint (hint routing from `npc_phrases.yaml`).
- **Thread safety**: `g_npc_state` is accessed only from the main FreeRTOS task (`runRuntimeIteration`). No mutex needed unless Audio Kit callbacks are added later.
- **QR debounce**: `NPC_QR_DEBOUNCE_MS = 30000` is in `npc_engine.h` but is not enforced by `npc_on_qr_scan()` — debounce at the call site in Task 5 if needed.
- **Step ID to NPC scene index**: The linear scan in Task 3 is O(n) on step count (≤12 steps). Acceptable for embedded.
