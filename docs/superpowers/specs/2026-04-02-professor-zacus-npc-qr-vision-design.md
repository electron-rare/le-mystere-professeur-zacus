# Professor Zacus NPC + QR Vision — Design Spec

## Summary

Professor Zacus becomes an autonomous AI-powered NPC that speaks through the RTC_PHONE (retro handset), adapts game difficulty in real-time, and reacts to player actions including QR code scans. Audio is hybrid: live TTS via Piper on Tower with automatic fallback to pre-generated MP3s on SD card.

**Target:** Playable demo by May 1st 2026.

## Goals

1. Professor Zacus speaks autonomously during gameplay — not just when asked for hints
2. Players interact via the RTC_PHONE handset (pick up to listen, hang up to dismiss)
3. QR code scanning unlocks game stages and triggers NPC reactions
4. Game difficulty adapts based on group pace (faster → harder, slower → more hints)
5. System works offline (SD fallback) when Tower is unreachable

## Non-Goals (Sprint 4+)

- Voice recognition from player (ESP-SR wake word, speech-to-text)
- Object recognition (ESP-DL CNN beyond QR)
- XTTS-v2 cloned voice for Professor
- AudioCraft ambient music generation

## Architecture

```
Player picks up RTC_PHONE
        ↓
ESP32-S3 (Freenove) ← ESP-NOW → RTC_BL_PHONE (handset speaker)
        ↓                              ↑
   Game Engine                    audio playback
   (scenario + state)                  ↑
        ↓                              ↑
   NPC Decision Engine ──→ Piper TTS (Tower:8001) ──→ WiFi audio stream
        │                                                    ↓
        │ fallback (Tower down or latency >2s)         RTC_BL_PHONE
        ↓
   SD card MP3s (pre-generated pool)
   via ESP-NOW HOTLINE_TRIGGER

ESP32-S3 Camera ──→ QR decode ──→ SC_EVENT ──→ Game Engine ──→ NPC reaction
```

## Component 1: NPC Decision Engine

### Location
Runs on ESP32-S3 firmware (`hardware/firmware/` or `ESP32_ZACUS/`). Lightweight state machine — no LLM on device.

### Trigger Rules

| Trigger | Condition | NPC Action |
|---------|-----------|------------|
| **Hint request** | Player picks up phone while stuck | Play hint (level 1→2→3 escalation) |
| **Stuck timer** | No progress for >3 min on current scene | Ring phone, offer hint proactively |
| **QR scanned** | Valid QR detected by camera | Congratulations + narrative context |
| **Wrong action** | Invalid QR or repeated failed attempts | Warning or misdirection |
| **Fast progress** | Scene completed in <50% expected time | Add false lead or optional challenge |
| **Slow progress** | Scene taking >150% expected time | Skip optional steps via SCENE_GOTO |
| **Scene transition** | Player completes a scene | Narrative bridge to next scene |
| **Game start** | Session begins | Introduction monologue |
| **Game end** | Final puzzle solved | Victory speech with stats |

### State Tracked

```c
typedef struct {
    uint8_t current_scene;
    uint8_t current_step;
    uint32_t scene_start_ms;
    uint32_t total_elapsed_ms;
    uint8_t hints_given[MAX_SCENES];     // per-scene hint level (0-3)
    uint8_t qr_scanned_count;
    uint8_t failed_attempts;
    bool phone_off_hook;
    bool tower_reachable;                // updated every 30s
    npc_mood_t mood;                     // NEUTRAL, IMPRESSED, WORRIED, AMUSED
} npc_state_t;
```

### Personality

Professor Zacus is eccentric, slightly absent-minded, enthusiastic about science, and speaks in a theatrical French tone. His mood shifts based on player performance:
- **IMPRESSED**: fast progress → fewer hints, more challenges
- **WORRIED**: slow progress → more encouraging, proactive hints
- **AMUSED**: wrong actions → playful teasing, false leads
- **NEUTRAL**: default state

## Component 2: Hybrid Audio (Live TTS + Fallback MP3)

### Live Path (Tower reachable)

1. NPC Engine builds reply text (French, max 200 chars for <5s audio)
2. HTTP POST to `http://192.168.0.120:8001/api/tts`
   - Body: `{"text": "...", "voice": "tom-medium", "format": "wav"}`
   - Response: WAV audio stream
3. ESP32-S3 receives WAV over WiFi
4. Forward to RTC_BL_PHONE via WiFi HTTP endpoint or ESP-NOW chunked audio
5. Phone plays through handset speaker

### Fallback Path (Tower unreachable or latency >2s)

1. NPC Engine selects best-match MP3 from SD card
   - Lookup key: `{scene_id}/{action_type}_{level}.mp3`
   - Example: `SCENE_LA_DETECTOR/commentaire_impressed_1.mp3`
2. ESP-NOW command `HOTLINE_TRIGGER` with file path
3. RTC_BL_PHONE plays from its own SD card (synced at boot)

### Tower Health Check

```c
// Every 30s, ping Tower TTS endpoint
bool check_tower() {
    // HEAD request to http://192.168.0.120:8001/api/tts
    // timeout: 2000ms
    // Updates npc_state.tower_reachable
}
```

### Audio Routing Priority

1. Live TTS (if Tower reachable AND phone off hook)
2. Pre-generated contextual MP3 (if Tower down, matching scene/action)
3. Generic fallback MP3 (if no contextual match)

## Component 3: QR Vision

### Hardware
- ESP32-S3 onboard camera (already wired on Freenove board)
- No additional hardware needed

### Library
- `esp_code_scanner` from ESP-IDF (built-in QR/barcode decoder)
- Runs in a FreeRTOS task at ~5 FPS scan rate
- Low memory footprint (~30KB working buffer)

### QR Format
Each physical QR in the escape room encodes:
```
ZACUS:{scene_id}:{event_id}:{checksum}
```
Example: `ZACUS:LA_DETECTOR:KEY_FOUND:A3F2`

### Integration
1. QR task runs continuously when camera is active
2. On valid decode → publish `SC_EVENT` to game engine
3. Game engine updates scene state
4. NPC Engine receives state change → triggers appropriate reaction
5. Debounce: same QR ignored for 30s after first scan

### Anti-Cheat
- Checksum prevents QR forgery (HMAC-SHA256 truncated to 4 hex chars)
- Each QR only valid for its designated scene (scanning ahead does nothing)
- QR scan logged to analytics

## Component 4: MP3 Pool Generation (Pre-Demo Script)

### Script
`tools/tts/generate_npc_pool.py` — runs before deployment, generates all fallback MP3s.

### Pool Structure

| Category | Count | Template |
|----------|-------|----------|
| Hints (existing) | ~54 | `{scene}/indice_{level}.mp3` |
| Congratulations | 12 | `{scene}/felicitations_{variant}.mp3` |
| Warnings | 10 | `{scene}/attention_{variant}.mp3` |
| Personality comments | 20 | `npc/commentaire_{mood}_{n}.mp3` |
| Difficulty adaptation | 15 | `npc/adaptation_{type}_{n}.mp3` |
| Narrative bridges | 12 | `{scene}/transition_{n}.mp3` |
| False leads | 8 | `{scene}/fausse_piste_{n}.mp3` |
| Ambiance (intro/outro) | 6 | `npc/ambiance_{type}.mp3` |
| **Total** | **~137** | |

### Generation

```bash
python3 tools/tts/generate_npc_pool.py \
    --voice tom-medium \
    --tts-url http://192.168.0.120:8001/api/tts \
    --output hardware/firmware/data/hotline_tts/ \
    --manifest game/scenarios/npc_phrases.yaml
```

Input: `game/scenarios/npc_phrases.yaml` — all Professor Zacus lines organized by category, scene, mood.

Output: MP3 files on SD card + manifest JSON for firmware lookup.

## Data Flow Summary

```
1. Game starts → NPC plays intro via TTS or MP3
2. Players explore → NPC monitors timer + events
3. Player scans QR → SC_EVENT → NPC congratulates
4. Player stuck 3min → NPC rings phone → hint level 1
5. Player still stuck → hint level 2, then 3
6. Player fast → NPC adds false lead or challenge
7. Scene complete → NPC narrative bridge
8. Final puzzle → NPC victory speech + stats
```

## Testing Strategy

- **Unit tests**: NPC trigger logic (Python mock of state machine)
- **Integration**: ESP-NOW command/response with mock phone
- **TTS round-trip**: text → Piper → audio validation (check duration, sample rate)
- **QR decode**: test with printed QR codes at various distances/angles
- **Fallback**: test with Tower deliberately offline
- **Full playthrough**: end-to-end with real hardware

## Files to Create/Modify

| File | Action |
|------|--------|
| `ESP32_ZACUS/.../npc_engine.h` | Create — NPC state machine, trigger rules |
| `ESP32_ZACUS/.../npc_engine.cpp` | Create — implementation |
| `ESP32_ZACUS/.../tts_client.h` | Create — HTTP client for Piper TTS |
| `ESP32_ZACUS/.../tts_client.cpp` | Create — implementation with fallback |
| `ESP32_ZACUS/.../qr_scanner.h` | Create — QR decode task |
| `ESP32_ZACUS/.../qr_scanner.cpp` | Create — camera + esp_code_scanner |
| `game/scenarios/npc_phrases.yaml` | Create — all Professor lines |
| `tools/tts/generate_npc_pool.py` | Create — MP3 batch generator |
| `hardware/firmware/docs/NPC_DESIGN.md` | Create — field reference |
| `ESP32_ZACUS/.../network_manager.cpp` | Modify — add audio streaming |
| `ESP32_ZACUS/.../scenario_manager.cpp` | Modify — wire NPC events |

## Dependencies

- Piper TTS on Tower:8001 (already running, 3 voices)
- ESP-IDF `esp_code_scanner` component
- RTC_BL_PHONE firmware (branch `esp32_RTC_ZACUS`) — needs audio streaming endpoint
- Physical QR codes printed for each puzzle

## Risk Assessment

| Risk | Mitigation |
|------|------------|
| Tower TTS latency >2s in demo venue | Fallback MP3 pool covers all critical paths |
| WiFi unstable at venue | ESP-NOW for phone control is WiFi-independent |
| Camera quality insufficient for QR | Test at demo venue, adjust distance/lighting |
| ESP32-S3 RAM pressure (camera + audio + game) | Profile memory, disable camera when not scanning |
| RTC_BL_PHONE firmware incompatible | Test cross-repo contract early (run_matrix_and_smoke.sh) |
