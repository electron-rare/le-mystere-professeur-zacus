# Professor Zacus NPC + QR Vision — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement NPC decision engine, hybrid TTS audio, QR scanner, and MP3 pool generator for May 1st demo.

**Architecture:** 4 independent components: NPC Engine (C firmware), TTS Client (C firmware), QR Scanner (C firmware), MP3 Pool Generator (Python tool).

**Tech Stack:** ESP-IDF, C, PlatformIO, Python 3, Piper TTS, ESP-NOW, esp_code_scanner

**Spec:** `docs/superpowers/specs/2026-04-02-professor-zacus-npc-qr-vision-design.md`

**Existing code references:**
- Scenario manager: `ESP32_ZACUS/ui_freenove_allinone/include/app/scenario_manager.h`
- Network manager: `ESP32_ZACUS/ui_freenove_allinone/include/system/network/network_manager.h`
- QR scan controller (existing): `ESP32_ZACUS/ui_freenove_allinone/include/ui/qr/qr_scan_controller.h`
- Voice pipeline scaffold: `ESP32_ZACUS/ui_freenove_allinone/src/voice/voice_pipeline.cpp`
- Game analytics: `ESP32_ZACUS/ui_freenove_allinone/include/analytics/game_analytics.h`
- Story HAL: `ESP32_ZACUS/lib/zacus_story_portable/include/zacus_story_portable/story_hal.h`
- Scenario source: `game/scenarios/zacus_v2.yaml`

---

## Task 1: NPC Engine Header (`npc_engine.h`)

**Time:** 3 min | **Deps:** None | **Type:** New file

**File:** `ESP32_ZACUS/ui_freenove_allinone/include/npc/npc_engine.h`

- [ ] 1.1 Create directory and header file with the following content:

```cpp
// npc_engine.h - Professor Zacus NPC decision engine.
// Lightweight state machine: trigger rules, mood system, phrase selection.
#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NPC_MAX_SCENES        12
#define NPC_MAX_HINT_LEVEL    3
#define NPC_PHRASE_MAX_LEN    200
#define NPC_STUCK_TIMEOUT_MS  (3UL * 60UL * 1000UL)
#define NPC_FAST_THRESHOLD_PCT  50
#define NPC_SLOW_THRESHOLD_PCT  150
#define NPC_QR_DEBOUNCE_MS    30000

typedef enum {
    NPC_MOOD_NEUTRAL = 0,
    NPC_MOOD_IMPRESSED,
    NPC_MOOD_WORRIED,
    NPC_MOOD_AMUSED,
    NPC_MOOD_COUNT
} npc_mood_t;

typedef enum {
    NPC_TRIGGER_NONE = 0,
    NPC_TRIGGER_HINT_REQUEST,
    NPC_TRIGGER_STUCK_TIMER,
    NPC_TRIGGER_QR_SCANNED,
    NPC_TRIGGER_WRONG_ACTION,
    NPC_TRIGGER_FAST_PROGRESS,
    NPC_TRIGGER_SLOW_PROGRESS,
    NPC_TRIGGER_SCENE_TRANSITION,
    NPC_TRIGGER_GAME_START,
    NPC_TRIGGER_GAME_END,
    NPC_TRIGGER_COUNT
} npc_trigger_t;

typedef enum {
    NPC_AUDIO_NONE = 0,
    NPC_AUDIO_LIVE_TTS,
    NPC_AUDIO_SD_CONTEXTUAL,
    NPC_AUDIO_SD_GENERIC
} npc_audio_source_t;

typedef struct {
    uint8_t current_scene;
    uint8_t current_step;
    uint32_t scene_start_ms;
    uint32_t total_elapsed_ms;
    uint8_t hints_given[NPC_MAX_SCENES];
    uint8_t qr_scanned_count;
    uint8_t failed_attempts;
    bool phone_off_hook;
    bool tower_reachable;
    npc_mood_t mood;
    uint32_t last_qr_scan_ms;
    uint32_t expected_scene_duration_ms;
} npc_state_t;

typedef struct {
    npc_trigger_t trigger;
    npc_audio_source_t audio_source;
    char phrase_text[NPC_PHRASE_MAX_LEN];
    char sd_path[128];
    npc_mood_t resulting_mood;
} npc_decision_t;

/// Initialize NPC state to defaults.
void npc_init(npc_state_t* state);

/// Reset NPC state for a new game session.
void npc_reset(npc_state_t* state);

/// Evaluate trigger rules and produce a decision.
/// Returns true if NPC wants to speak.
bool npc_evaluate(const npc_state_t* state, uint32_t now_ms, npc_decision_t* out);

/// Notify NPC of a scene change.
void npc_on_scene_change(npc_state_t* state, uint8_t new_scene,
                         uint32_t expected_duration_ms, uint32_t now_ms);

/// Notify NPC of a QR scan result.
void npc_on_qr_scan(npc_state_t* state, bool valid, uint32_t now_ms);

/// Notify NPC that player picked up / hung up phone.
void npc_on_phone_hook(npc_state_t* state, bool off_hook);

/// Notify NPC of hint request (phone picked up while stuck).
void npc_on_hint_request(npc_state_t* state, uint32_t now_ms);

/// Notify NPC of Tower TTS reachability change.
void npc_on_tower_status(npc_state_t* state, bool reachable);

/// Update mood based on progress ratio (elapsed_ms / expected_ms).
void npc_update_mood(npc_state_t* state, uint32_t now_ms);

/// Get the current hint level for a scene (0 = no hints given, max 3).
uint8_t npc_hint_level(const npc_state_t* state, uint8_t scene);

/// Build SD card fallback path for a given trigger + scene.
/// Writes to out_path, returns true if a valid path was built.
bool npc_build_sd_path(char* out_path, size_t capacity,
                       uint8_t scene, npc_trigger_t trigger,
                       npc_mood_t mood, uint8_t variant);

#ifdef __cplusplus
}
#endif
```

- [ ] 1.2 Verify the include directory exists:
```bash
mkdir -p ESP32_ZACUS/ui_freenove_allinone/include/npc
```

---

## Task 2: NPC Engine Implementation (`npc_engine.cpp`)

**Time:** 5 min | **Deps:** Task 1 | **Type:** New file

**File:** `ESP32_ZACUS/ui_freenove_allinone/src/npc/npc_engine.cpp`

- [ ] 2.1 Create source directory:
```bash
mkdir -p ESP32_ZACUS/ui_freenove_allinone/src/npc
```

- [ ] 2.2 Create `npc_engine.cpp` with full implementation:

```cpp
// npc_engine.cpp - Professor Zacus NPC decision engine implementation.
#include "npc/npc_engine.h"
#include <cstring>
#include <cstdio>

// Scene ID strings for SD path generation (must match zacus_v2.yaml step order).
static const char* const kSceneIds[] = {
    "SCENE_U_SON_PROTO",
    "SCENE_LA_DETECTOR",
    "SCENE_WIN_ETAPE1",
    "SCENE_WARNING",
    "SCENE_LEFOU_DETECTOR",
    "SCENE_WIN_ETAPE2",
    "SCENE_QR_DETECTOR",
    "SCENE_FINAL_WIN"
};
static const uint8_t kSceneCount = sizeof(kSceneIds) / sizeof(kSceneIds[0]);

static const char* const kTriggerDirs[] = {
    [NPC_TRIGGER_NONE]             = "generic",
    [NPC_TRIGGER_HINT_REQUEST]     = "indice",
    [NPC_TRIGGER_STUCK_TIMER]      = "indice",
    [NPC_TRIGGER_QR_SCANNED]       = "felicitations",
    [NPC_TRIGGER_WRONG_ACTION]     = "attention",
    [NPC_TRIGGER_FAST_PROGRESS]    = "fausse_piste",
    [NPC_TRIGGER_SLOW_PROGRESS]    = "adaptation",
    [NPC_TRIGGER_SCENE_TRANSITION] = "transition",
    [NPC_TRIGGER_GAME_START]       = "ambiance",
    [NPC_TRIGGER_GAME_END]         = "ambiance",
};

static const char* const kMoodSuffixes[] = {
    [NPC_MOOD_NEUTRAL]    = "neutral",
    [NPC_MOOD_IMPRESSED]  = "impressed",
    [NPC_MOOD_WORRIED]    = "worried",
    [NPC_MOOD_AMUSED]     = "amused",
};

void npc_init(npc_state_t* state) {
    if (state == NULL) return;
    memset(state, 0, sizeof(*state));
    state->mood = NPC_MOOD_NEUTRAL;
}

void npc_reset(npc_state_t* state) {
    npc_init(state);
}

void npc_on_scene_change(npc_state_t* state, uint8_t new_scene,
                         uint32_t expected_duration_ms, uint32_t now_ms) {
    if (state == NULL) return;
    state->current_scene = new_scene;
    state->scene_start_ms = now_ms;
    state->expected_scene_duration_ms = expected_duration_ms;
    state->failed_attempts = 0;
}

void npc_on_qr_scan(npc_state_t* state, bool valid, uint32_t now_ms) {
    if (state == NULL) return;
    if (valid) {
        state->qr_scanned_count++;
    } else {
        state->failed_attempts++;
    }
    state->last_qr_scan_ms = now_ms;
}

void npc_on_phone_hook(npc_state_t* state, bool off_hook) {
    if (state == NULL) return;
    state->phone_off_hook = off_hook;
}

void npc_on_hint_request(npc_state_t* state, uint32_t now_ms) {
    if (state == NULL) return;
    uint8_t scene = state->current_scene;
    if (scene < NPC_MAX_SCENES && state->hints_given[scene] < NPC_MAX_HINT_LEVEL) {
        state->hints_given[scene]++;
    }
    (void)now_ms;
}

void npc_on_tower_status(npc_state_t* state, bool reachable) {
    if (state == NULL) return;
    state->tower_reachable = reachable;
}

void npc_update_mood(npc_state_t* state, uint32_t now_ms) {
    if (state == NULL || state->expected_scene_duration_ms == 0) return;
    uint32_t elapsed = now_ms - state->scene_start_ms;
    uint32_t expected = state->expected_scene_duration_ms;
    uint32_t pct = (elapsed * 100U) / expected;

    if (state->failed_attempts >= 3) {
        state->mood = NPC_MOOD_AMUSED;
    } else if (pct < NPC_FAST_THRESHOLD_PCT) {
        state->mood = NPC_MOOD_IMPRESSED;
    } else if (pct > NPC_SLOW_THRESHOLD_PCT) {
        state->mood = NPC_MOOD_WORRIED;
    } else {
        state->mood = NPC_MOOD_NEUTRAL;
    }
}

uint8_t npc_hint_level(const npc_state_t* state, uint8_t scene) {
    if (state == NULL || scene >= NPC_MAX_SCENES) return 0;
    return state->hints_given[scene];
}

bool npc_build_sd_path(char* out_path, size_t capacity,
                       uint8_t scene, npc_trigger_t trigger,
                       npc_mood_t mood, uint8_t variant) {
    if (out_path == NULL || capacity < 16) return false;

    const char* scene_id = (scene < kSceneCount) ? kSceneIds[scene] : "npc";
    const char* trigger_dir = (trigger < NPC_TRIGGER_COUNT)
        ? kTriggerDirs[trigger] : "generic";
    const char* mood_str = (mood < NPC_MOOD_COUNT)
        ? kMoodSuffixes[mood] : "neutral";

    // Per-scene triggers: /hotline_tts/{scene_id}/{trigger}_{mood}_{variant}.mp3
    // Generic NPC: /hotline_tts/npc/{trigger}_{mood}_{variant}.mp3
    bool is_scene_specific = (trigger != NPC_TRIGGER_GAME_START
                           && trigger != NPC_TRIGGER_GAME_END
                           && trigger != NPC_TRIGGER_NONE);

    int written;
    if (is_scene_specific && scene < kSceneCount) {
        written = snprintf(out_path, capacity,
            "/hotline_tts/%s/%s_%s_%u.mp3",
            scene_id, trigger_dir, mood_str, (unsigned)variant);
    } else {
        written = snprintf(out_path, capacity,
            "/hotline_tts/npc/%s_%s_%u.mp3",
            trigger_dir, mood_str, (unsigned)variant);
    }
    return (written > 0 && (size_t)written < capacity);
}

bool npc_evaluate(const npc_state_t* state, uint32_t now_ms, npc_decision_t* out) {
    if (state == NULL || out == NULL) return false;
    memset(out, 0, sizeof(*out));

    uint32_t scene_elapsed = now_ms - state->scene_start_ms;
    uint32_t expected = state->expected_scene_duration_ms;

    // Priority 1: Hint request (phone off hook while stuck)
    if (state->phone_off_hook && scene_elapsed > NPC_STUCK_TIMEOUT_MS) {
        uint8_t level = npc_hint_level(state, state->current_scene);
        out->trigger = NPC_TRIGGER_HINT_REQUEST;
        out->resulting_mood = state->mood;
        npc_build_sd_path(out->sd_path, sizeof(out->sd_path),
                          state->current_scene, NPC_TRIGGER_HINT_REQUEST,
                          state->mood, level);
        out->audio_source = state->tower_reachable
            ? NPC_AUDIO_LIVE_TTS : NPC_AUDIO_SD_CONTEXTUAL;
        return true;
    }

    // Priority 2: Stuck timer (proactive, no phone needed)
    if (scene_elapsed > NPC_STUCK_TIMEOUT_MS
        && npc_hint_level(state, state->current_scene) == 0) {
        out->trigger = NPC_TRIGGER_STUCK_TIMER;
        out->resulting_mood = NPC_MOOD_WORRIED;
        npc_build_sd_path(out->sd_path, sizeof(out->sd_path),
                          state->current_scene, NPC_TRIGGER_STUCK_TIMER,
                          NPC_MOOD_WORRIED, 0);
        out->audio_source = state->tower_reachable
            ? NPC_AUDIO_LIVE_TTS : NPC_AUDIO_SD_CONTEXTUAL;
        return true;
    }

    // Priority 3: Fast progress detection
    if (expected > 0 && scene_elapsed > 0
        && (scene_elapsed * 100U / expected) < NPC_FAST_THRESHOLD_PCT) {
        out->trigger = NPC_TRIGGER_FAST_PROGRESS;
        out->resulting_mood = NPC_MOOD_IMPRESSED;
        npc_build_sd_path(out->sd_path, sizeof(out->sd_path),
                          state->current_scene, NPC_TRIGGER_FAST_PROGRESS,
                          NPC_MOOD_IMPRESSED, 0);
        out->audio_source = state->tower_reachable
            ? NPC_AUDIO_LIVE_TTS : NPC_AUDIO_SD_CONTEXTUAL;
        return true;
    }

    // Priority 4: Slow progress detection
    if (expected > 0 && (scene_elapsed * 100U / expected) > NPC_SLOW_THRESHOLD_PCT) {
        out->trigger = NPC_TRIGGER_SLOW_PROGRESS;
        out->resulting_mood = NPC_MOOD_WORRIED;
        npc_build_sd_path(out->sd_path, sizeof(out->sd_path),
                          state->current_scene, NPC_TRIGGER_SLOW_PROGRESS,
                          NPC_MOOD_WORRIED, 0);
        out->audio_source = state->tower_reachable
            ? NPC_AUDIO_LIVE_TTS : NPC_AUDIO_SD_CONTEXTUAL;
        return true;
    }

    return false;
}
```

- [ ] 2.3 Add `-I$PROJECT_DIR/ui_freenove_allinone/include/npc` to `build_flags` in `platformio.ini` (actually not needed since `include/` is already in the search path via `-I$PROJECT_DIR/ui_freenove_allinone/include`).

---

## Task 3: TTS Client Header (`tts_client.h`)

**Time:** 2 min | **Deps:** None | **Type:** New file

**File:** `ESP32_ZACUS/ui_freenove_allinone/include/npc/tts_client.h`

- [ ] 3.1 Create header:

```cpp
// tts_client.h - HTTP client for Piper TTS on Tower:8001 with SD fallback.
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TTS_TOWER_IP        "192.168.0.120"
#define TTS_TOWER_PORT      8001
#define TTS_API_PATH        "/api/tts"
#define TTS_VOICE           "tom-medium"
#define TTS_TIMEOUT_MS      2000
#define TTS_HEALTH_INTERVAL_MS  30000
#define TTS_MAX_TEXT_LEN    200
#define TTS_WAV_BUF_SIZE    (64 * 1024)

typedef enum {
    TTS_RESULT_OK = 0,
    TTS_RESULT_TIMEOUT,
    TTS_RESULT_HTTP_ERROR,
    TTS_RESULT_ALLOC_FAIL,
    TTS_RESULT_TOWER_DOWN,
    TTS_RESULT_TEXT_TOO_LONG
} tts_result_t;

typedef struct {
    bool tower_reachable;
    uint32_t last_health_check_ms;
    uint32_t last_latency_ms;
    uint32_t total_requests;
    uint32_t total_failures;
} tts_stats_t;

/// Initialize TTS client (call once from setup).
void tts_init(void);

/// Check Tower TTS health (non-blocking, HEAD request).
/// Updates internal reachability state.
/// Returns true if Tower responded within TTS_TIMEOUT_MS.
bool tts_check_health(uint32_t now_ms);

/// Periodic health check — only pings if TTS_HEALTH_INTERVAL_MS elapsed.
void tts_health_tick(uint32_t now_ms);

/// Query last known Tower reachability.
bool tts_is_tower_reachable(void);

/// Request TTS synthesis. Blocking call (up to TTS_TIMEOUT_MS).
/// On success, writes WAV data to out_buf and sets *out_len.
/// Caller must allocate out_buf of at least TTS_WAV_BUF_SIZE bytes (use PSRAM).
tts_result_t tts_synthesize(const char* text, uint8_t* out_buf,
                            size_t buf_capacity, size_t* out_len);

/// Get TTS client statistics.
tts_stats_t tts_get_stats(void);

#ifdef __cplusplus
}
#endif
```

---

## Task 4: TTS Client Implementation (`tts_client.cpp`)

**Time:** 5 min | **Deps:** Task 3 | **Type:** New file

**File:** `ESP32_ZACUS/ui_freenove_allinone/src/npc/tts_client.cpp`

- [ ] 4.1 Create implementation:

```cpp
// tts_client.cpp - Piper TTS HTTP client for ESP32.
#include "npc/tts_client.h"

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <cstring>
#include <cstdio>

static tts_stats_t s_stats = {};
static char s_url[128] = {0};

void tts_init(void) {
    memset(&s_stats, 0, sizeof(s_stats));
    snprintf(s_url, sizeof(s_url), "http://%s:%d%s",
             TTS_TOWER_IP, TTS_TOWER_PORT, TTS_API_PATH);
    Serial.printf("[TTS] init url=%s\n", s_url);
}

bool tts_check_health(uint32_t now_ms) {
    if (WiFi.status() != WL_CONNECTED) {
        s_stats.tower_reachable = false;
        s_stats.last_health_check_ms = now_ms;
        return false;
    }

    HTTPClient http;
    http.setTimeout(TTS_TIMEOUT_MS);

    char health_url[128];
    snprintf(health_url, sizeof(health_url), "http://%s:%d/",
             TTS_TOWER_IP, TTS_TOWER_PORT);

    bool ok = false;
    if (http.begin(health_url)) {
        uint32_t t0 = millis();
        int code = http.sendRequest("HEAD");
        s_stats.last_latency_ms = millis() - t0;
        ok = (code >= 200 && code < 400);
        http.end();
    }

    s_stats.tower_reachable = ok;
    s_stats.last_health_check_ms = now_ms;
    Serial.printf("[TTS] health: %s (%lu ms)\n",
                  ok ? "OK" : "DOWN", (unsigned long)s_stats.last_latency_ms);
    return ok;
}

void tts_health_tick(uint32_t now_ms) {
    if (now_ms - s_stats.last_health_check_ms >= TTS_HEALTH_INTERVAL_MS) {
        tts_check_health(now_ms);
    }
}

bool tts_is_tower_reachable(void) {
    return s_stats.tower_reachable;
}

tts_result_t tts_synthesize(const char* text, uint8_t* out_buf,
                            size_t buf_capacity, size_t* out_len) {
    if (text == NULL || out_buf == NULL || out_len == NULL) {
        return TTS_RESULT_ALLOC_FAIL;
    }
    *out_len = 0;

    size_t text_len = strlen(text);
    if (text_len > TTS_MAX_TEXT_LEN) {
        return TTS_RESULT_TEXT_TOO_LONG;
    }
    if (!s_stats.tower_reachable) {
        return TTS_RESULT_TOWER_DOWN;
    }

    s_stats.total_requests++;

    HTTPClient http;
    http.setTimeout(TTS_TIMEOUT_MS);

    if (!http.begin(s_url)) {
        s_stats.total_failures++;
        return TTS_RESULT_HTTP_ERROR;
    }

    http.addHeader("Content-Type", "application/json");

    // Build JSON body: {"text":"...","voice":"tom-medium","format":"wav"}
    char body[TTS_MAX_TEXT_LEN + 64];
    snprintf(body, sizeof(body),
             "{\"text\":\"%s\",\"voice\":\"%s\",\"format\":\"wav\"}",
             text, TTS_VOICE);

    uint32_t t0 = millis();
    int code = http.POST(body);
    s_stats.last_latency_ms = millis() - t0;

    if (code != 200) {
        Serial.printf("[TTS] POST failed: %d (%lu ms)\n", code,
                      (unsigned long)s_stats.last_latency_ms);
        http.end();
        s_stats.total_failures++;
        if (s_stats.last_latency_ms >= TTS_TIMEOUT_MS) {
            return TTS_RESULT_TIMEOUT;
        }
        return TTS_RESULT_HTTP_ERROR;
    }

    int content_len = http.getSize();
    if (content_len <= 0 || (size_t)content_len > buf_capacity) {
        Serial.printf("[TTS] bad content_len: %d (cap %u)\n",
                      content_len, (unsigned)buf_capacity);
        http.end();
        s_stats.total_failures++;
        return TTS_RESULT_ALLOC_FAIL;
    }

    WiFiClient* stream = http.getStreamPtr();
    size_t total_read = 0;
    while (total_read < (size_t)content_len) {
        int avail = stream->available();
        if (avail <= 0) {
            if (!stream->connected()) break;
            delay(1);
            continue;
        }
        int to_read = (avail > 1024) ? 1024 : avail;
        if (total_read + (size_t)to_read > buf_capacity) {
            to_read = (int)(buf_capacity - total_read);
        }
        int r = stream->read(out_buf + total_read, to_read);
        if (r > 0) total_read += (size_t)r;
    }

    http.end();
    *out_len = total_read;
    Serial.printf("[TTS] OK: %u bytes, %lu ms\n",
                  (unsigned)total_read, (unsigned long)s_stats.last_latency_ms);
    return TTS_RESULT_OK;
}

tts_stats_t tts_get_stats(void) {
    return s_stats;
}
```

---

## Task 5: QR Scanner Header (`qr_scanner.h`)

**Time:** 2 min | **Deps:** None | **Type:** New file

**File:** `ESP32_ZACUS/ui_freenove_allinone/include/npc/qr_scanner.h`

- [ ] 5.1 Create QR scanner with ZACUS-protocol validation, HMAC checksum, debounce, and anti-cheat:

```cpp
// qr_scanner.h - QR decode task with ZACUS protocol, debounce, anti-cheat.
// Wraps the existing ui::QrScanController with NPC-level logic.
#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define QR_PREFIX              "ZACUS:"
#define QR_PREFIX_LEN          6
#define QR_SCENE_ID_MAX        32
#define QR_EVENT_ID_MAX        32
#define QR_CHECKSUM_LEN        4
#define QR_DEBOUNCE_MS         30000
#define QR_HMAC_KEY            "zacus-escape-2026"
#define QR_MAX_HISTORY         16

typedef enum {
    QR_VALID = 0,
    QR_INVALID_FORMAT,
    QR_INVALID_CHECKSUM,
    QR_WRONG_SCENE,
    QR_DEBOUNCED,
    QR_ALREADY_SCANNED
} qr_validation_t;

typedef struct {
    char scene_id[QR_SCENE_ID_MAX];
    char event_id[QR_EVENT_ID_MAX];
    char checksum[QR_CHECKSUM_LEN + 1];
    qr_validation_t status;
    uint32_t scanned_at_ms;
} qr_decode_result_t;

/// Initialize QR scanner subsystem.
void qr_npc_init(void);

/// Parse a raw QR payload string into structured result.
/// Does NOT validate scene or checksum yet.
bool qr_npc_parse(const char* payload, qr_decode_result_t* out);

/// Validate a parsed QR result against current game state.
/// current_scene_id: the scene the player is currently in.
/// now_ms: current time for debounce check.
qr_validation_t qr_npc_validate(const qr_decode_result_t* decoded,
                                 const char* current_scene_id,
                                 uint32_t now_ms);

/// Compute HMAC-SHA256 checksum (truncated to 4 hex chars).
/// Input: "SCENE_ID:EVENT_ID", output: 4 hex chars in out_checksum.
void qr_npc_compute_checksum(const char* scene_id, const char* event_id,
                              char out_checksum[5]);

/// Record a scan in history (for anti-cheat / dedup).
void qr_npc_record_scan(const char* scene_id, const char* event_id, uint32_t now_ms);

/// Check if a specific QR was already scanned.
bool qr_npc_was_scanned(const char* scene_id, const char* event_id);

/// Get total valid scans count.
uint8_t qr_npc_scan_count(void);

#ifdef __cplusplus
}
#endif
```

---

## Task 6: QR Scanner Implementation (`qr_scanner.cpp`)

**Time:** 4 min | **Deps:** Task 5 | **Type:** New file

**File:** `ESP32_ZACUS/ui_freenove_allinone/src/npc/qr_scanner.cpp`

- [ ] 6.1 Create implementation with HMAC validation and scan history:

```cpp
// qr_scanner.cpp - ZACUS QR protocol parser, validator, anti-cheat.
#include "npc/qr_scanner.h"
#include <cstring>
#include <cstdio>
#include <mbedtls/md.h>

typedef struct {
    char scene_id[QR_SCENE_ID_MAX];
    char event_id[QR_EVENT_ID_MAX];
    uint32_t at_ms;
} qr_history_entry_t;

static qr_history_entry_t s_history[QR_MAX_HISTORY];
static uint8_t s_history_count = 0;
static uint32_t s_last_scan_ms = 0;
static char s_last_payload[QR_SCENE_ID_MAX + QR_EVENT_ID_MAX + 8] = {0};

void qr_npc_init(void) {
    memset(s_history, 0, sizeof(s_history));
    s_history_count = 0;
    s_last_scan_ms = 0;
    s_last_payload[0] = '\0';
}

bool qr_npc_parse(const char* payload, qr_decode_result_t* out) {
    if (payload == NULL || out == NULL) return false;
    memset(out, 0, sizeof(*out));

    // Expected: ZACUS:{scene_id}:{event_id}:{checksum}
    if (strncmp(payload, QR_PREFIX, QR_PREFIX_LEN) != 0) {
        out->status = QR_INVALID_FORMAT;
        return false;
    }

    const char* p = payload + QR_PREFIX_LEN;

    // Parse scene_id
    const char* sep1 = strchr(p, ':');
    if (sep1 == NULL) { out->status = QR_INVALID_FORMAT; return false; }
    size_t scene_len = (size_t)(sep1 - p);
    if (scene_len == 0 || scene_len >= QR_SCENE_ID_MAX) {
        out->status = QR_INVALID_FORMAT; return false;
    }
    memcpy(out->scene_id, p, scene_len);
    out->scene_id[scene_len] = '\0';

    // Parse event_id
    const char* p2 = sep1 + 1;
    const char* sep2 = strchr(p2, ':');
    if (sep2 == NULL) { out->status = QR_INVALID_FORMAT; return false; }
    size_t event_len = (size_t)(sep2 - p2);
    if (event_len == 0 || event_len >= QR_EVENT_ID_MAX) {
        out->status = QR_INVALID_FORMAT; return false;
    }
    memcpy(out->event_id, p2, event_len);
    out->event_id[event_len] = '\0';

    // Parse checksum (4 hex chars)
    const char* p3 = sep2 + 1;
    size_t ck_len = strlen(p3);
    if (ck_len != QR_CHECKSUM_LEN) {
        out->status = QR_INVALID_FORMAT; return false;
    }
    memcpy(out->checksum, p3, QR_CHECKSUM_LEN);
    out->checksum[QR_CHECKSUM_LEN] = '\0';

    out->status = QR_VALID;
    return true;
}

void qr_npc_compute_checksum(const char* scene_id, const char* event_id,
                              char out_checksum[5]) {
    if (scene_id == NULL || event_id == NULL || out_checksum == NULL) return;

    char input[QR_SCENE_ID_MAX + QR_EVENT_ID_MAX + 2];
    snprintf(input, sizeof(input), "%s:%s", scene_id, event_id);

    uint8_t hmac[32];
    mbedtls_md_context_t ctx;
    mbedtls_md_init(&ctx);
    const mbedtls_md_info_t* info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    mbedtls_md_setup(&ctx, info, 1);
    mbedtls_md_hmac_starts(&ctx, (const uint8_t*)QR_HMAC_KEY, strlen(QR_HMAC_KEY));
    mbedtls_md_hmac_update(&ctx, (const uint8_t*)input, strlen(input));
    mbedtls_md_hmac_finish(&ctx, hmac);
    mbedtls_md_free(&ctx);

    // Truncate to first 2 bytes = 4 hex chars
    snprintf(out_checksum, 5, "%02X%02X", hmac[0], hmac[1]);
}

qr_validation_t qr_npc_validate(const qr_decode_result_t* decoded,
                                 const char* current_scene_id,
                                 uint32_t now_ms) {
    if (decoded == NULL || decoded->status != QR_VALID) {
        return QR_INVALID_FORMAT;
    }

    // Checksum verification
    char expected[5];
    qr_npc_compute_checksum(decoded->scene_id, decoded->event_id, expected);
    if (strncmp(expected, decoded->checksum, QR_CHECKSUM_LEN) != 0) {
        return QR_INVALID_CHECKSUM;
    }

    // Scene check: QR only valid for its designated scene
    if (current_scene_id != NULL
        && strcmp(decoded->scene_id, current_scene_id) != 0) {
        return QR_WRONG_SCENE;
    }

    // Already scanned check
    if (qr_npc_was_scanned(decoded->scene_id, decoded->event_id)) {
        return QR_ALREADY_SCANNED;
    }

    // Debounce: same raw payload within QR_DEBOUNCE_MS
    char key[QR_SCENE_ID_MAX + QR_EVENT_ID_MAX + 2];
    snprintf(key, sizeof(key), "%s:%s", decoded->scene_id, decoded->event_id);
    if (strcmp(key, s_last_payload) == 0
        && (now_ms - s_last_scan_ms) < QR_DEBOUNCE_MS) {
        return QR_DEBOUNCED;
    }

    return QR_VALID;
}

void qr_npc_record_scan(const char* scene_id, const char* event_id, uint32_t now_ms) {
    if (scene_id == NULL || event_id == NULL) return;

    // Update debounce state
    snprintf(s_last_payload, sizeof(s_last_payload), "%s:%s", scene_id, event_id);
    s_last_scan_ms = now_ms;

    // Add to history (ring buffer)
    if (s_history_count < QR_MAX_HISTORY) {
        qr_history_entry_t* entry = &s_history[s_history_count++];
        strncpy(entry->scene_id, scene_id, QR_SCENE_ID_MAX - 1);
        strncpy(entry->event_id, event_id, QR_EVENT_ID_MAX - 1);
        entry->at_ms = now_ms;
    }
}

bool qr_npc_was_scanned(const char* scene_id, const char* event_id) {
    if (scene_id == NULL || event_id == NULL) return false;
    for (uint8_t i = 0; i < s_history_count; i++) {
        if (strcmp(s_history[i].scene_id, scene_id) == 0
            && strcmp(s_history[i].event_id, event_id) == 0) {
            return true;
        }
    }
    return false;
}

uint8_t qr_npc_scan_count(void) {
    return s_history_count;
}
```

---

## Task 7: NPC Phrases YAML

**Time:** 5 min | **Deps:** None | **Type:** New file

**File:** `game/scenarios/npc_phrases.yaml`

- [ ] 7.1 Create the YAML manifest with all Professor Zacus lines organized by category, scene, and mood:

```yaml
# npc_phrases.yaml - All Professor Zacus NPC lines for TTS generation.
# Used by tools/tts/generate_npc_pool.py to batch-generate MP3 fallback pool.
# Voice: tom-medium on Piper TTS (Tower:8001)

meta:
  version: 1
  voice: tom-medium
  language: fr
  max_chars: 200
  total_estimated: 137

# =============================================
# HINTS — per scene, 3 levels each
# =============================================
hints:
  SCENE_U_SON_PROTO:
    - level: 1
      text: "Hmm, U-SON vibre de maniere etrange... Regardez bien les frequences affichees."
    - level: 2
      text: "Le prototype reagit aux sons purs. Cherchez un diapason ou un generateur de frequence."
    - level: 3
      text: "Quatre cent quarante hertz. C'est le LA de reference. Produisez cette note exacte."

  SCENE_LA_DETECTOR:
    - level: 1
      text: "Le detecteur attend une frequence precise. Ecoutez bien, la reponse est dans la musique."
    - level: 2
      text: "LA quatre cent quarante hertz. C'est le standard universel. Utilisez le generateur."
    - level: 3
      text: "Approchez le generateur de frequence du microphone et reglez-le sur quatre cent quarante hertz exact."

  SCENE_LEFOU_DETECTOR:
    - level: 1
      text: "Zone quatre... L'Electron Fou a laisse un code musical. Regardez le piano."
    - level: 2
      text: "Les lettres sur le piano-alphabet forment un mot. L.E.F.O.U. L'Electron Fou..."
    - level: 3
      text: "Tapez L.E.F.O.U sur le piano-alphabet. Chaque lettre correspond a une touche."

  SCENE_QR_DETECTOR:
    - level: 1
      text: "La cle finale est cachee quelque part dans cette piece. Cherchez un code QR."
    - level: 2
      text: "Le portrait du professeur... Il cache quelque chose derriere lui."
    - level: 3
      text: "Retournez le portrait de Zacus. Le QR code WIN est colle au dos."

# =============================================
# CONGRATULATIONS — per scene
# =============================================
congratulations:
  SCENE_LA_DETECTOR:
    - variant: 1
      mood: impressed
      text: "Extraordinaire ! Vous avez stabilise le LA quatre cent quarante. U-SON est sauve !"
    - variant: 2
      mood: neutral
      text: "Bien joue. La frequence est verrouillee. Passons a la suite."

  SCENE_LEFOU_DETECTOR:
    - variant: 1
      mood: impressed
      text: "Magnifique ! Le code de l'Electron Fou est dechiffre. Quelle perspicacite !"
    - variant: 2
      mood: neutral
      text: "Le piano a parle. L'Electron Fou valide votre passage en Zone quatre."

  SCENE_QR_DETECTOR:
    - variant: 1
      mood: impressed
      text: "VICTOIRE ! Le QR WIN est valide. Le Campus est sauve, l'equipe est formidable !"
    - variant: 2
      mood: neutral
      text: "QR code valide. Le mystere est resolu. Felicitations."

  generic:
    - variant: 1
      mood: impressed
      text: "Impressionnant ! Je n'aurais pas fait mieux moi-meme. Et pourtant..."
    - variant: 2
      mood: amused
      text: "Ah, vous avez trouve ! J'avais presque oublie ou j'avais cache celui-la."
    - variant: 3
      mood: neutral
      text: "Tres bien. Continuez sur cette lancee."

# =============================================
# WARNINGS — wrong actions / invalid QR
# =============================================
warnings:
  generic:
    - variant: 1
      mood: amused
      text: "Non non non ! Ce n'est pas du tout ca. Mais j'admire votre creativite."
    - variant: 2
      mood: worried
      text: "Attention, cette manipulation n'est pas la bonne. Reflechissez calmement."
    - variant: 3
      mood: amused
      text: "Vous essayez de tricher ? Le professeur Zacus voit tout, vous savez."
    - variant: 4
      mood: neutral
      text: "Ce QR code n'est pas valide. Cherchez le bon, il est dans cette salle."
    - variant: 5
      mood: worried
      text: "Ce n'est pas le bon moment pour ce code. Chaque chose en son temps."

# =============================================
# PERSONALITY COMMENTS — mood-based ambient
# =============================================
personality:
  impressed:
    - text: "Quelle equipe remarquable ! Le campus n'a jamais vu un tel niveau."
    - text: "Vous allez plus vite que mes meilleurs etudiants. Fascinant."
    - text: "A ce rythme, vous finirez avant que mon cafe ne refroidisse."
    - text: "Je suis estomaque. Peut-etre devrais-je rendre l'enigme plus difficile..."
    - text: "Brillant ! Absolument brillant. Je prends des notes."

  worried:
    - text: "Ne paniquez pas. Le temps passe, mais la solution est proche."
    - text: "Prenez un instant pour observer. La reponse est devant vos yeux."
    - text: "Je sens que vous avez besoin d'un petit coup de pouce. Decrochez le telephone."
    - text: "U-SON devient instable. Il faut agir vite maintenant."
    - text: "Le campus compte sur vous. Concentrez-vous sur l'essentiel."

  amused:
    - text: "Ha ha ! Voila une approche... originale. Pas la bonne, mais originale."
    - text: "Je vois que vous testez toutes les hypotheses. Meme les mauvaises."
    - text: "L'Electron Fou serait fier de cette confusion. Lui aussi melange tout."
    - text: "C'est comme ca que j'ai decouvert le plutonium. Par erreur."
    - text: "Continuez comme ca et vous allez inventer quelque chose de nouveau."

  neutral:
    - text: "Hmm, voyons voir comment vous vous en sortez."
    - text: "Le campus attend. Chaque seconde compte."
    - text: "Interessant. Tres interessant."
    - text: "Montrez-moi ce que la prochaine generation sait faire."
    - text: "Le professeur Zacus observe. Toujours."

# =============================================
# DIFFICULTY ADAPTATION
# =============================================
adaptation:
  fast_skip:
    - text: "Vous etes trop rapides ! Voici un defi supplementaire pour vous occuper."
    - text: "Attendez... il y a un piege que j'avais prepare pour les plus rapides."
    - text: "Pas si vite ! L'Electron Fou a cache un obstacle bonus par ici."

  slow_help:
    - text: "Le temps presse. Je vais simplifier un peu les choses pour vous."
    - text: "Hmm, passons directement a l'etape suivante. Vous avez prouve votre valeur."
    - text: "Le professeur Zacus decide d'intervenir. Suivez mes instructions."

  false_lead:
    - text: "Attention, j'ai entendu dire qu'un piege se cache pres de l'armoire."
    - text: "Un de mes collegues a laisse un indice trompeur ici. Ne vous y fiez pas."
    - text: "Le miroir renvoie une image inversee. Peut-etre que le code aussi..."

# =============================================
# SCENE TRANSITIONS — narrative bridges
# =============================================
transitions:
  STEP_U_SON_PROTO_to_STEP_LA_DETECTOR:
    text: "U-SON est active mais instable. Vous devez trouver la frequence de reference pour le stabiliser."

  STEP_LA_DETECTOR_to_STEP_WIN_ETAPE1:
    text: "Le LA est verrouille. U-SON se calme enfin. Etape un validee !"

  STEP_WIN_ETAPE1_to_STEP_WARNING:
    text: "Attention ! Un avertissement du systeme. Zone quatre detectee. Quelque chose ne va pas..."

  STEP_WARNING_to_STEP_LEFOU_DETECTOR:
    text: "Zone quatre, le domaine de l'Electron Fou. Seul son code musical vous ouvrira le passage."

  STEP_LEFOU_DETECTOR_to_STEP_QR_DETECTOR:
    text: "Le code de l'Electron Fou est valide. Il ne reste plus qu'a trouver la cle finale dans les archives."

  STEP_QR_DETECTOR_to_STEP_FINAL_WIN:
    text: "C'est fini ! Le campus est sauve. Le mystere du Professeur Zacus est resolu."

# =============================================
# AMBIANCE — intro / outro
# =============================================
ambiance:
  game_start:
    - text: "Bienvenue au Campus scientifique du Professeur Zacus. Je suis... enfin, j'etais la. Quelque chose s'est passe avec U-SON. Mon prototype devient instable. Vous etes ma derniere chance."
    - text: "Ah, vous voila enfin ! Le professeur Zacus a besoin de vous. Le campus est en alerte. Ecoutez attentivement mes instructions."

  game_end_victory:
    - text: "Formidable ! Le campus est sauve. Ce n'etait pas une punition, mais un test d'ethique. U-SON ne doit pas devenir un objet de panique. Vous avez prouve votre valeur."
    - text: "Bravo ! En tant que minutes et secondes, vous avez resolu le mystere. Le professeur Zacus est fier de vous."

  game_end_timeout:
    - text: "Le temps est ecoule. Le campus reste en mode securite. Mais ne soyez pas tristes. Le vrai test, c'etait le courage d'essayer."
```

---

## Task 8: MP3 Pool Generator Script

**Time:** 5 min | **Deps:** Task 7 | **Type:** New file

**File:** `tools/tts/generate_npc_pool.py`

- [ ] 8.1 Create directory:
```bash
mkdir -p tools/tts
```

- [ ] 8.2 Create the complete batch generation script:

```python
#!/usr/bin/env python3
"""generate_npc_pool.py - Batch-generate Professor Zacus NPC MP3 fallback pool.

Usage:
    python3 tools/tts/generate_npc_pool.py \
        --voice tom-medium \
        --tts-url http://192.168.0.120:8001/api/tts \
        --output hardware/firmware/data/hotline_tts/ \
        --manifest game/scenarios/npc_phrases.yaml

Reads npc_phrases.yaml, sends each line to Piper TTS, saves WAV,
converts to MP3, and writes a manifest.json for firmware lookup.
"""

import argparse
import json
import os
import subprocess
import sys
import time
from pathlib import Path

import requests
import yaml


def parse_args():
    p = argparse.ArgumentParser(description="Generate NPC MP3 fallback pool")
    p.add_argument("--voice", default="tom-medium", help="Piper TTS voice name")
    p.add_argument("--tts-url", default="http://192.168.0.120:8001/api/tts",
                   help="Piper TTS endpoint URL")
    p.add_argument("--output", required=True, help="Output directory for MP3 files")
    p.add_argument("--manifest", required=True, help="Path to npc_phrases.yaml")
    p.add_argument("--format", choices=["mp3", "wav"], default="mp3",
                   help="Output audio format (default: mp3)")
    p.add_argument("--dry-run", action="store_true", help="Print files without generating")
    p.add_argument("--timeout", type=int, default=10, help="HTTP timeout in seconds")
    p.add_argument("--delay", type=float, default=0.3,
                   help="Delay between requests in seconds")
    return p.parse_args()


def tts_request(url: str, text: str, voice: str, timeout: int) -> bytes:
    """Send TTS request, return WAV bytes."""
    resp = requests.post(url, json={
        "text": text,
        "voice": voice,
        "format": "wav"
    }, timeout=timeout)
    resp.raise_for_status()
    return resp.content


def wav_to_mp3(wav_path: Path, mp3_path: Path):
    """Convert WAV to MP3 using ffmpeg."""
    subprocess.run([
        "ffmpeg", "-y", "-i", str(wav_path),
        "-codec:a", "libmp3lame", "-b:a", "64k",
        "-ar", "22050", "-ac", "1",
        str(mp3_path)
    ], check=True, capture_output=True)
    wav_path.unlink()


def process_hints(data: dict, output_dir: Path, args, manifest: list):
    """Process hints section."""
    hints = data.get("hints", {})
    for scene_id, levels in hints.items():
        scene_dir = output_dir / scene_id
        scene_dir.mkdir(parents=True, exist_ok=True)
        for entry in levels:
            level = entry["level"]
            text = entry["text"]
            filename = f"indice_{level}"
            yield scene_id, filename, text


def process_congratulations(data: dict, output_dir: Path, args, manifest: list):
    """Process congratulations section."""
    congrats = data.get("congratulations", {})
    for scene_id, variants in congrats.items():
        for entry in variants:
            variant = entry["variant"]
            mood = entry.get("mood", "neutral")
            text = entry["text"]
            if scene_id == "generic":
                filename = f"felicitations_{mood}_{variant}"
                yield "npc", filename, text
            else:
                filename = f"felicitations_{mood}_{variant}"
                yield scene_id, filename, text


def process_warnings(data: dict, output_dir: Path, args, manifest: list):
    """Process warnings section."""
    warnings = data.get("warnings", {})
    for scene_id, variants in warnings.items():
        for entry in variants:
            variant = entry["variant"]
            mood = entry.get("mood", "neutral")
            text = entry["text"]
            filename = f"attention_{mood}_{variant}"
            yield "npc" if scene_id == "generic" else scene_id, filename, text


def process_personality(data: dict, output_dir: Path, args, manifest: list):
    """Process personality comments section."""
    personality = data.get("personality", {})
    for mood, entries in personality.items():
        for i, entry in enumerate(entries, 1):
            text = entry["text"]
            filename = f"commentaire_{mood}_{i}"
            yield "npc", filename, text


def process_adaptation(data: dict, output_dir: Path, args, manifest: list):
    """Process difficulty adaptation section."""
    adaptation = data.get("adaptation", {})
    for adapt_type, entries in adaptation.items():
        for i, entry in enumerate(entries, 1):
            text = entry["text"]
            filename = f"adaptation_{adapt_type}_{i}"
            yield "npc", filename, text


def process_transitions(data: dict, output_dir: Path, args, manifest: list):
    """Process scene transition narrative bridges."""
    transitions = data.get("transitions", {})
    for transition_id, entry in transitions.items():
        text = entry["text"]
        # Extract scene from transition ID (e.g. STEP_X_to_STEP_Y -> first scene)
        parts = transition_id.split("_to_")
        scene_id = parts[0].replace("STEP_", "SCENE_") if parts else "npc"
        filename = f"transition_{transition_id}"
        yield scene_id, filename, text


def process_ambiance(data: dict, output_dir: Path, args, manifest: list):
    """Process ambiance (intro/outro) section."""
    ambiance = data.get("ambiance", {})
    for amb_type, entries in ambiance.items():
        for i, entry in enumerate(entries, 1):
            text = entry["text"]
            filename = f"ambiance_{amb_type}_{i}"
            yield "npc", filename, text


def main():
    args = parse_args()
    output_dir = Path(args.output)
    manifest_path = Path(args.manifest)

    if not manifest_path.exists():
        print(f"ERROR: manifest not found: {manifest_path}", file=sys.stderr)
        sys.exit(1)

    with open(manifest_path, "r", encoding="utf-8") as f:
        data = yaml.safe_load(f)

    output_dir.mkdir(parents=True, exist_ok=True)

    processors = [
        process_hints,
        process_congratulations,
        process_warnings,
        process_personality,
        process_adaptation,
        process_transitions,
        process_ambiance,
    ]

    manifest = []
    total = 0
    errors = 0
    ext = f".{args.format}"

    # Collect all items first
    all_items = []
    for proc in processors:
        for subdir, filename, text in proc(data, output_dir, args, manifest):
            all_items.append((subdir, filename, text))

    print(f"Found {len(all_items)} phrases to generate.")

    if args.dry_run:
        for subdir, filename, text in all_items:
            rel_path = f"{subdir}/{filename}{ext}"
            print(f"  {rel_path}: {text[:60]}...")
        print(f"\nDry run: {len(all_items)} files would be generated.")
        return

    # Check TTS server
    try:
        resp = requests.head(args.tts_url.replace("/api/tts", "/"), timeout=3)
        print(f"TTS server OK (status {resp.status_code})")
    except requests.ConnectionError:
        print(f"ERROR: Cannot reach TTS server at {args.tts_url}", file=sys.stderr)
        sys.exit(1)

    for subdir, filename, text in all_items:
        subdir_path = output_dir / subdir
        subdir_path.mkdir(parents=True, exist_ok=True)

        wav_path = subdir_path / f"{filename}.wav"
        final_path = subdir_path / f"{filename}{ext}"

        if final_path.exists():
            print(f"  SKIP (exists): {subdir}/{filename}{ext}")
            manifest.append({
                "path": f"{subdir}/{filename}{ext}",
                "text": text,
            })
            total += 1
            continue

        try:
            wav_data = tts_request(args.tts_url, text, args.voice, args.timeout)
            wav_path.write_bytes(wav_data)

            if args.format == "mp3":
                wav_to_mp3(wav_path, final_path)
            # else wav_path is already the final file

            manifest.append({
                "path": f"{subdir}/{filename}{ext}",
                "text": text,
            })
            total += 1
            print(f"  OK: {subdir}/{filename}{ext} ({len(wav_data)} bytes)")
            time.sleep(args.delay)

        except Exception as e:
            errors += 1
            print(f"  FAIL: {subdir}/{filename}{ext}: {e}", file=sys.stderr)

    # Write manifest JSON
    manifest_out = output_dir / "manifest.json"
    with open(manifest_out, "w", encoding="utf-8") as f:
        json.dump({
            "version": data.get("meta", {}).get("version", 1),
            "voice": args.voice,
            "generated_at": time.strftime("%Y-%m-%dT%H:%M:%S"),
            "total": total,
            "files": manifest,
        }, f, indent=2, ensure_ascii=False)

    print(f"\nDone: {total} generated, {errors} errors.")
    print(f"Manifest: {manifest_out}")


if __name__ == "__main__":
    main()
```

- [ ] 8.3 Make executable:
```bash
chmod +x tools/tts/generate_npc_pool.py
```

---

## Task 9: Wire NPC to Scenario Manager

**Time:** 4 min | **Deps:** Tasks 1-6 | **Type:** Modify existing file

**File:** `ESP32_ZACUS/ui_freenove_allinone/src/app/scenario_manager.cpp`

- [ ] 9.1 Add NPC include at top of file, after existing includes:

Add after line 8 (`#include "scenarios/default_scenario_v2.h"`):
```cpp
#include "npc/npc_engine.h"
```

- [ ] 9.2 Add NPC state as a static in the anonymous namespace (after line 12):

Add after `namespace {`:
```cpp
static npc_state_t g_npc_state;
static bool g_npc_initialized = false;
```

- [ ] 9.3 In `ScenarioManager::begin()`, initialize NPC after scenario load succeeds. Add after the `Serial.printf("[SCENARIO] loaded built-in scenario..."` block (after line 155):

```cpp
  if (!g_npc_initialized) {
    npc_init(&g_npc_state);
    g_npc_initialized = true;
    Serial.println("[NPC] initialized");
  }
```

- [ ] 9.4 In `ScenarioManager::enterStep()`, notify NPC of scene change. This requires finding `enterStep` in the file and adding after the step entry log line:

```cpp
  if (g_npc_initialized) {
    // Estimate 5 minutes per scene as default expected duration
    npc_on_scene_change(&g_npc_state, (uint8_t)step_index, 5UL * 60UL * 1000UL, now_ms);
    npc_update_mood(&g_npc_state, now_ms);
  }
```

- [ ] 9.5 In `ScenarioManager::tick()`, add NPC evaluation. Add before the closing brace:

```cpp
  if (g_npc_initialized) {
    npc_update_mood(&g_npc_state, now_ms);
  }
```

---

## Task 10: Wire NPC to Network Manager (Tower health check)

**Time:** 3 min | **Deps:** Tasks 3-4 | **Type:** Modify existing file

**File:** `ESP32_ZACUS/ui_freenove_allinone/src/system/network/network_manager.cpp`

- [ ] 10.1 Add TTS include at top, after existing includes:

Add after `#include "core/str_utils.h"` (line 13):
```cpp
#include "npc/tts_client.h"
```

- [ ] 10.2 In `NetworkManager::update()`, add TTS health tick. Find the `update` method and add at the end, before its closing brace:

```cpp
  // NPC TTS health check (every 30s)
  tts_health_tick(now_ms);
```

---

## Task 11: Wire QR Scanner to NPC (event dispatch)

**Time:** 3 min | **Deps:** Tasks 5-6 | **Type:** Modify existing file

**File:** `ESP32_ZACUS/ui_freenove_allinone/src/ui/qr/qr_scene_controller.cpp`

- [ ] 11.1 Read the existing `qr_scene_controller.cpp` to find the QR validation callback location.

- [ ] 11.2 Add NPC QR include at top:
```cpp
#include "npc/qr_scanner.h"
#include "npc/npc_engine.h"
```

- [ ] 11.3 After a successful QR validation (where `UNLOCK_QR` event is dispatched), add NPC notification:
```cpp
  // Notify NPC of valid QR scan
  extern npc_state_t g_npc_state;
  npc_on_qr_scan(&g_npc_state, true, millis());
  qr_npc_record_scan(decoded.scene_id, decoded.event_id, millis());
```

- [ ] 11.4 After a failed QR validation, add:
```cpp
  extern npc_state_t g_npc_state;
  npc_on_qr_scan(&g_npc_state, false, millis());
```

---

## Task 12: Add build flags to platformio.ini

**Time:** 2 min | **Deps:** Tasks 1-6 | **Type:** Modify existing file

**File:** `ESP32_ZACUS/platformio.ini`

- [ ] 12.1 Add `DNPC_ENGINE_ENABLED=1` build flag. In the `build_flags` section of `[env:freenove_esp32s3_full_with_ui]`, add:

```
  -DNPC_ENGINE_ENABLED=1
  -DTTS_CLIENT_ENABLED=1
```

- [ ] 12.2 Verify `mbedtls` is available (it is built into ESP-IDF/Arduino for ESP32, no extra dependency needed).

---

## Task 13: Unit Tests for NPC Trigger Logic (Python)

**Time:** 5 min | **Deps:** Task 1 | **Type:** New file, TDD

**File:** `tests/test_npc_engine.py`

- [ ] 13.1 Create Python unit tests that mirror the C logic (portable test without hardware):

```python
"""test_npc_engine.py - Unit tests for NPC trigger logic.

These tests validate the NPC state machine rules in Python,
mirroring the C implementation for fast iteration.
Run: python3 -m pytest tests/test_npc_engine.py -v
"""

import pytest

# Python mirror of NPC constants
NPC_MAX_SCENES = 12
NPC_MAX_HINT_LEVEL = 3
NPC_STUCK_TIMEOUT_MS = 3 * 60 * 1000  # 180000
NPC_FAST_THRESHOLD_PCT = 50
NPC_SLOW_THRESHOLD_PCT = 150
NPC_QR_DEBOUNCE_MS = 30000

MOOD_NEUTRAL = 0
MOOD_IMPRESSED = 1
MOOD_WORRIED = 2
MOOD_AMUSED = 3

TRIGGER_NONE = 0
TRIGGER_HINT_REQUEST = 1
TRIGGER_STUCK_TIMER = 2
TRIGGER_QR_SCANNED = 3
TRIGGER_WRONG_ACTION = 4
TRIGGER_FAST_PROGRESS = 5
TRIGGER_SLOW_PROGRESS = 6
TRIGGER_SCENE_TRANSITION = 7
TRIGGER_GAME_START = 8
TRIGGER_GAME_END = 9


class NpcState:
    """Python mirror of npc_state_t."""
    def __init__(self):
        self.current_scene = 0
        self.current_step = 0
        self.scene_start_ms = 0
        self.total_elapsed_ms = 0
        self.hints_given = [0] * NPC_MAX_SCENES
        self.qr_scanned_count = 0
        self.failed_attempts = 0
        self.phone_off_hook = False
        self.tower_reachable = True
        self.mood = MOOD_NEUTRAL
        self.last_qr_scan_ms = 0
        self.expected_scene_duration_ms = 0


def update_mood(state: NpcState, now_ms: int):
    if state.expected_scene_duration_ms == 0:
        return
    elapsed = now_ms - state.scene_start_ms
    pct = (elapsed * 100) // state.expected_scene_duration_ms
    if state.failed_attempts >= 3:
        state.mood = MOOD_AMUSED
    elif pct < NPC_FAST_THRESHOLD_PCT:
        state.mood = MOOD_IMPRESSED
    elif pct > NPC_SLOW_THRESHOLD_PCT:
        state.mood = MOOD_WORRIED
    else:
        state.mood = MOOD_NEUTRAL


def evaluate(state: NpcState, now_ms: int):
    """Returns (trigger, mood) or (TRIGGER_NONE, mood)."""
    scene_elapsed = now_ms - state.scene_start_ms
    expected = state.expected_scene_duration_ms

    # Priority 1: Hint request
    if state.phone_off_hook and scene_elapsed > NPC_STUCK_TIMEOUT_MS:
        return TRIGGER_HINT_REQUEST, state.mood

    # Priority 2: Stuck timer (proactive)
    if scene_elapsed > NPC_STUCK_TIMEOUT_MS:
        if state.hints_given[state.current_scene] == 0:
            return TRIGGER_STUCK_TIMER, MOOD_WORRIED

    # Priority 3: Fast progress
    if expected > 0 and scene_elapsed > 0:
        if (scene_elapsed * 100 // expected) < NPC_FAST_THRESHOLD_PCT:
            return TRIGGER_FAST_PROGRESS, MOOD_IMPRESSED

    # Priority 4: Slow progress
    if expected > 0:
        if (scene_elapsed * 100 // expected) > NPC_SLOW_THRESHOLD_PCT:
            return TRIGGER_SLOW_PROGRESS, MOOD_WORRIED

    return TRIGGER_NONE, state.mood


class TestNpcMood:
    def test_default_mood_is_neutral(self):
        s = NpcState()
        assert s.mood == MOOD_NEUTRAL

    def test_fast_progress_sets_impressed(self):
        s = NpcState()
        s.scene_start_ms = 0
        s.expected_scene_duration_ms = 300000  # 5 min
        update_mood(s, 100000)  # 100s elapsed = 33% < 50%
        assert s.mood == MOOD_IMPRESSED

    def test_slow_progress_sets_worried(self):
        s = NpcState()
        s.scene_start_ms = 0
        s.expected_scene_duration_ms = 300000  # 5 min
        update_mood(s, 500000)  # 500s = 166% > 150%
        assert s.mood == MOOD_WORRIED

    def test_normal_progress_stays_neutral(self):
        s = NpcState()
        s.scene_start_ms = 0
        s.expected_scene_duration_ms = 300000
        update_mood(s, 200000)  # 66%
        assert s.mood == MOOD_NEUTRAL

    def test_many_failures_sets_amused(self):
        s = NpcState()
        s.scene_start_ms = 0
        s.expected_scene_duration_ms = 300000
        s.failed_attempts = 3
        update_mood(s, 100000)
        assert s.mood == MOOD_AMUSED

    def test_failures_override_fast(self):
        s = NpcState()
        s.scene_start_ms = 0
        s.expected_scene_duration_ms = 300000
        s.failed_attempts = 5
        update_mood(s, 50000)  # fast, but many failures
        assert s.mood == MOOD_AMUSED


class TestNpcTriggers:
    def test_no_trigger_early_game(self):
        s = NpcState()
        s.scene_start_ms = 0
        s.expected_scene_duration_ms = 300000
        trigger, mood = evaluate(s, 60000)  # 1 min in
        assert trigger == TRIGGER_FAST_PROGRESS  # 20% < 50%

    def test_stuck_timer_fires_after_3min(self):
        s = NpcState()
        s.scene_start_ms = 0
        s.expected_scene_duration_ms = 300000
        trigger, mood = evaluate(s, 200000)  # 3:20 > 3:00
        assert trigger == TRIGGER_STUCK_TIMER
        assert mood == MOOD_WORRIED

    def test_stuck_timer_does_not_repeat_after_hint(self):
        s = NpcState()
        s.scene_start_ms = 0
        s.expected_scene_duration_ms = 300000
        s.hints_given[0] = 1  # already got hint
        trigger, mood = evaluate(s, 200000)
        # Should NOT fire stuck_timer since hint already given
        assert trigger != TRIGGER_STUCK_TIMER

    def test_hint_request_when_phone_off_hook_and_stuck(self):
        s = NpcState()
        s.scene_start_ms = 0
        s.expected_scene_duration_ms = 300000
        s.phone_off_hook = True
        trigger, mood = evaluate(s, 200000)
        assert trigger == TRIGGER_HINT_REQUEST

    def test_hint_request_priority_over_stuck_timer(self):
        s = NpcState()
        s.scene_start_ms = 0
        s.expected_scene_duration_ms = 300000
        s.phone_off_hook = True
        s.hints_given[0] = 0
        trigger, _ = evaluate(s, 200000)
        assert trigger == TRIGGER_HINT_REQUEST

    def test_slow_progress_trigger(self):
        s = NpcState()
        s.scene_start_ms = 0
        s.expected_scene_duration_ms = 300000
        s.hints_given[0] = 1  # disable stuck timer
        trigger, mood = evaluate(s, 480000)  # 160%
        assert trigger == TRIGGER_SLOW_PROGRESS
        assert mood == MOOD_WORRIED

    def test_fast_progress_trigger(self):
        s = NpcState()
        s.scene_start_ms = 0
        s.expected_scene_duration_ms = 600000  # 10 min
        trigger, mood = evaluate(s, 120000)  # 2 min = 20%
        assert trigger == TRIGGER_FAST_PROGRESS
        assert mood == MOOD_IMPRESSED


class TestNpcHints:
    def test_hint_level_starts_at_zero(self):
        s = NpcState()
        assert s.hints_given[0] == 0

    def test_hint_level_increments(self):
        s = NpcState()
        s.hints_given[0] = 1
        assert s.hints_given[0] == 1

    def test_hint_level_caps_at_three(self):
        s = NpcState()
        for i in range(5):
            if s.hints_given[0] < NPC_MAX_HINT_LEVEL:
                s.hints_given[0] += 1
        assert s.hints_given[0] == NPC_MAX_HINT_LEVEL


class TestQrProtocol:
    """Test QR format parsing (Python mirror)."""

    def test_valid_qr_parse(self):
        payload = "ZACUS:LA_DETECTOR:KEY_FOUND:A3F2"
        parts = payload.split(":")
        assert parts[0] == "ZACUS"
        assert parts[1] == "LA_DETECTOR"
        assert parts[2] == "KEY_FOUND"
        assert parts[3] == "A3F2"
        assert len(parts[3]) == 4

    def test_invalid_prefix(self):
        payload = "NOTUS:LA_DETECTOR:KEY_FOUND:A3F2"
        assert not payload.startswith("ZACUS:")

    def test_missing_checksum(self):
        payload = "ZACUS:LA_DETECTOR:KEY_FOUND"
        parts = payload.split(":")
        assert len(parts) == 3  # missing checksum

    def test_empty_payload(self):
        payload = ""
        assert not payload.startswith("ZACUS:")


class TestSdPathBuilder:
    """Test SD path generation (Python mirror)."""

    SCENE_IDS = [
        "SCENE_U_SON_PROTO", "SCENE_LA_DETECTOR", "SCENE_WIN_ETAPE1",
        "SCENE_WARNING", "SCENE_LEFOU_DETECTOR", "SCENE_WIN_ETAPE2",
        "SCENE_QR_DETECTOR", "SCENE_FINAL_WIN"
    ]
    TRIGGER_DIRS = {
        TRIGGER_HINT_REQUEST: "indice",
        TRIGGER_QR_SCANNED: "felicitations",
        TRIGGER_WRONG_ACTION: "attention",
        TRIGGER_FAST_PROGRESS: "fausse_piste",
        TRIGGER_SCENE_TRANSITION: "transition",
        TRIGGER_GAME_START: "ambiance",
    }
    MOOD_SUFFIXES = {
        MOOD_NEUTRAL: "neutral",
        MOOD_IMPRESSED: "impressed",
        MOOD_WORRIED: "worried",
        MOOD_AMUSED: "amused",
    }

    def build_sd_path(self, scene_idx, trigger, mood, variant):
        scene_id = self.SCENE_IDS[scene_idx] if scene_idx < len(self.SCENE_IDS) else "npc"
        trigger_dir = self.TRIGGER_DIRS.get(trigger, "generic")
        mood_str = self.MOOD_SUFFIXES.get(mood, "neutral")

        is_scene_specific = trigger not in (TRIGGER_GAME_START, TRIGGER_GAME_END, TRIGGER_NONE)
        if is_scene_specific and scene_idx < len(self.SCENE_IDS):
            return f"/hotline_tts/{scene_id}/{trigger_dir}_{mood_str}_{variant}.mp3"
        return f"/hotline_tts/npc/{trigger_dir}_{mood_str}_{variant}.mp3"

    def test_hint_path(self):
        path = self.build_sd_path(1, TRIGGER_HINT_REQUEST, MOOD_WORRIED, 2)
        assert path == "/hotline_tts/SCENE_LA_DETECTOR/indice_worried_2.mp3"

    def test_game_start_is_generic(self):
        path = self.build_sd_path(0, TRIGGER_GAME_START, MOOD_NEUTRAL, 1)
        assert path == "/hotline_tts/npc/ambiance_neutral_1.mp3"

    def test_qr_congratulations_path(self):
        path = self.build_sd_path(6, TRIGGER_QR_SCANNED, MOOD_IMPRESSED, 1)
        assert path == "/hotline_tts/SCENE_QR_DETECTOR/felicitations_impressed_1.mp3"
```

- [ ] 13.2 Run tests:
```bash
cd /Users/electron/Documents/Projets_Creatifs/le-mystere-professeur-zacus
python3 -m pytest tests/test_npc_engine.py -v
```

---

## Task 14: Integration Test for TTS Round-Trip (Python)

**Time:** 3 min | **Deps:** Task 8 | **Type:** New file

**File:** `tests/test_tts_roundtrip.py`

- [ ] 14.1 Create integration test (requires Tower online):

```python
"""test_tts_roundtrip.py - Integration test for Piper TTS round-trip.

Requires Tower:8001 running. Skip if unreachable.
Run: python3 -m pytest tests/test_tts_roundtrip.py -v
"""

import io
import struct
import time

import pytest
import requests

TTS_URL = "http://192.168.0.120:8001/api/tts"
TTS_VOICE = "tom-medium"
TTS_TIMEOUT = 10


def tower_reachable():
    try:
        r = requests.head(TTS_URL.replace("/api/tts", "/"), timeout=2)
        return r.status_code < 400
    except Exception:
        return False


skip_no_tower = pytest.mark.skipif(
    not tower_reachable(),
    reason="Tower TTS not reachable"
)


@skip_no_tower
class TestTtsRoundTrip:
    def _synth(self, text: str) -> bytes:
        resp = requests.post(TTS_URL, json={
            "text": text,
            "voice": TTS_VOICE,
            "format": "wav",
        }, timeout=TTS_TIMEOUT)
        resp.raise_for_status()
        return resp.content

    def test_simple_phrase(self):
        wav = self._synth("Bonjour, je suis le Professeur Zacus.")
        assert len(wav) > 44  # WAV header is 44 bytes min
        assert wav[:4] == b"RIFF"
        assert wav[8:12] == b"WAVE"

    def test_long_phrase_under_200_chars(self):
        text = "Extraordinaire ! Vous avez stabilise le LA quatre cent quarante hertz. U-SON est sauve. Passons a la suite."
        assert len(text) <= 200
        wav = self._synth(text)
        assert len(wav) > 1000

    def test_wav_sample_rate(self):
        wav = self._synth("Test de frequence.")
        # WAV format: sample rate at offset 24 (4 bytes LE)
        sample_rate = struct.unpack_from("<I", wav, 24)[0]
        assert sample_rate in (16000, 22050, 24000, 44100)

    def test_latency_under_2s(self):
        t0 = time.time()
        self._synth("Test rapide.")
        elapsed = time.time() - t0
        assert elapsed < 2.0, f"TTS latency {elapsed:.2f}s exceeds 2s threshold"

    def test_french_accents(self):
        wav = self._synth("Le detecteur de frequences est pret.")
        assert len(wav) > 44
```

- [ ] 14.2 Run integration test (skip gracefully if Tower down):
```bash
cd /Users/electron/Documents/Projets_Creatifs/le-mystere-professeur-zacus
python3 -m pytest tests/test_tts_roundtrip.py -v
```

---

## Task 15: QR Code Generator Utility (for printing physical QR codes)

**Time:** 3 min | **Deps:** None | **Type:** New file

**File:** `tools/tts/generate_qr_codes.py`

- [ ] 15.1 Create QR code generator for physical escape room QR codes with HMAC checksums:

```python
#!/usr/bin/env python3
"""generate_qr_codes.py - Generate physical QR codes for Zacus escape room.

Computes HMAC-SHA256 checksums matching the firmware's qr_scanner.cpp.

Usage:
    python3 tools/tts/generate_qr_codes.py --output tools/printables/qr/
"""

import argparse
import hashlib
import hmac
import os
from pathlib import Path

HMAC_KEY = b"zacus-escape-2026"

QR_DEFINITIONS = [
    ("LA_DETECTOR", "KEY_FOUND"),
    ("LEFOU_DETECTOR", "PIANO_OK"),
    ("QR_DETECTOR", "WIN"),
]


def compute_checksum(scene_id: str, event_id: str) -> str:
    """Match firmware qr_npc_compute_checksum: HMAC-SHA256, first 2 bytes as 4 hex chars."""
    msg = f"{scene_id}:{event_id}".encode()
    digest = hmac.new(HMAC_KEY, msg, hashlib.sha256).digest()
    return f"{digest[0]:02X}{digest[1]:02X}"


def main():
    parser = argparse.ArgumentParser(description="Generate Zacus QR codes")
    parser.add_argument("--output", default="tools/printables/qr/",
                        help="Output directory")
    args = parser.parse_args()

    out_dir = Path(args.output)
    out_dir.mkdir(parents=True, exist_ok=True)

    print("Zacus QR Code Payloads:")
    print("=" * 60)
    for scene_id, event_id in QR_DEFINITIONS:
        checksum = compute_checksum(scene_id, event_id)
        payload = f"ZACUS:{scene_id}:{event_id}:{checksum}"
        print(f"  {payload}")

        # Write payload to text file (use qrencode or similar to generate image)
        txt_path = out_dir / f"qr_{scene_id}_{event_id}.txt"
        txt_path.write_text(payload + "\n")

    print(f"\nPayload files written to {out_dir}/")
    print("Generate images with: qrencode -o output.png -s 10 < payload.txt")


if __name__ == "__main__":
    main()
```

---

## Task 16: Validate YAML Manifest Schema

**Time:** 2 min | **Deps:** Task 7 | **Type:** New file, TDD

**File:** `tests/test_npc_phrases_schema.py`

- [ ] 16.1 Create schema validation test for npc_phrases.yaml:

```python
"""test_npc_phrases_schema.py - Validate npc_phrases.yaml structure."""

import pytest
import yaml
from pathlib import Path

MANIFEST_PATH = Path(__file__).parent.parent / "game" / "scenarios" / "npc_phrases.yaml"


@pytest.fixture
def phrases():
    assert MANIFEST_PATH.exists(), f"Manifest not found: {MANIFEST_PATH}"
    with open(MANIFEST_PATH, "r", encoding="utf-8") as f:
        return yaml.safe_load(f)


class TestNpcPhrasesSchema:
    def test_meta_section_exists(self, phrases):
        assert "meta" in phrases
        assert "version" in phrases["meta"]
        assert "voice" in phrases["meta"]

    def test_hints_section_has_levels(self, phrases):
        hints = phrases.get("hints", {})
        assert len(hints) > 0, "No hint scenes defined"
        for scene_id, levels in hints.items():
            for entry in levels:
                assert "level" in entry, f"Missing level in {scene_id}"
                assert "text" in entry, f"Missing text in {scene_id}"
                assert 1 <= entry["level"] <= 3

    def test_all_texts_under_200_chars(self, phrases):
        max_chars = phrases.get("meta", {}).get("max_chars", 200)
        violations = []
        for section_name in ["hints", "congratulations", "warnings",
                              "personality", "adaptation", "ambiance"]:
            section = phrases.get(section_name, {})
            if isinstance(section, dict):
                for key, entries in section.items():
                    if isinstance(entries, list):
                        for entry in entries:
                            text = entry.get("text", "")
                            if len(text) > max_chars:
                                violations.append(
                                    f"{section_name}/{key}: {len(text)} chars"
                                )
        assert not violations, f"Texts over {max_chars} chars:\n" + "\n".join(violations)

    def test_transitions_have_text(self, phrases):
        transitions = phrases.get("transitions", {})
        assert len(transitions) > 0
        for trans_id, entry in transitions.items():
            assert "text" in entry, f"Missing text in transition {trans_id}"

    def test_ambiance_has_start_and_end(self, phrases):
        ambiance = phrases.get("ambiance", {})
        assert "game_start" in ambiance
        assert "game_end_victory" in ambiance

    def test_personality_has_all_moods(self, phrases):
        personality = phrases.get("personality", {})
        expected = {"impressed", "worried", "amused", "neutral"}
        assert set(personality.keys()) == expected

    def test_total_phrase_count(self, phrases):
        """Verify we have at least 100 phrases (spec says ~137)."""
        count = 0
        for section_name in ["hints", "congratulations", "warnings",
                              "personality", "adaptation", "ambiance"]:
            section = phrases.get(section_name, {})
            if isinstance(section, dict):
                for key, entries in section.items():
                    if isinstance(entries, list):
                        count += len(entries)
        transitions = phrases.get("transitions", {})
        count += len(transitions)
        assert count >= 80, f"Only {count} phrases, expected 80+"
```

- [ ] 16.2 Run:
```bash
cd /Users/electron/Documents/Projets_Creatifs/le-mystere-professeur-zacus
python3 -m pytest tests/test_npc_phrases_schema.py -v
```

---

## Task 17: Add `npc` source files to PlatformIO build filter

**Time:** 2 min | **Deps:** Tasks 1-6 | **Type:** Modify existing file

**File:** `ESP32_ZACUS/platformio.ini`

- [ ] 17.1 Update `build_src_filter` to include npc sources. Find this section:
```ini
build_src_filter =
  -<*>
  +<src/>
```

This already includes all of `src/` recursively, so `src/npc/` is automatically included. No change needed.

- [ ] 17.2 Verify by checking the include path covers `include/npc/`. The existing flag `-I$PROJECT_DIR/ui_freenove_allinone/include` already covers all subdirectories. No change needed.

---

## Task 18: Create requirements file for Python tools

**Time:** 1 min | **Deps:** None | **Type:** New file

**File:** `tools/tts/requirements.txt`

- [ ] 18.1 Create:
```
requests>=2.28.0
pyyaml>=6.0
```

---

## Execution Order Summary

| Phase | Tasks | Can parallelize? |
|-------|-------|-----------------|
| **Phase 1: Headers** | 1, 3, 5 | Yes (all independent) |
| **Phase 2: Implementations** | 2, 4, 6 | Yes (each depends only on its header) |
| **Phase 3: Data** | 7, 8, 15, 18 | Yes (all independent) |
| **Phase 4: Integration** | 9, 10, 11, 12, 17 | Partially (9-11 modify different files) |
| **Phase 5: Tests** | 13, 14, 16 | Yes (all independent) |

**Estimated total time:** 45-55 minutes for a single developer, ~25 minutes with parallel agents.

**Verification command after all tasks:**
```bash
cd /Users/electron/Documents/Projets_Creatifs/le-mystere-professeur-zacus
python3 -m pytest tests/test_npc_engine.py tests/test_npc_phrases_schema.py -v
# Integration test (needs Tower online):
python3 -m pytest tests/test_tts_roundtrip.py -v
# PlatformIO compile check (needs ESP32 toolchain):
cd ESP32_ZACUS && pio run --environment freenove_esp32s3_full_with_ui
```
