# Voice Pipeline — ESP-SR + MacStudio STT/LLM/TTS

**Date**: 2026-05-03
**Status**: Approved (brainstorm session)
**Author**: L'électron rare (with Claude facilitation)

Depends on: `2026-05-03-tts-stt-llm-macstudio-design.md` (whisper + Piper + Qwen on MacStudio).
Depends on: `2026-05-03-hints-engine-design.md` (voice "indice" command shares the hints path).

---

## Goal

Bring the voice pipeline scaffold (`voice_pipeline.cpp` returns `false` today) to production: a player says "Professeur Zacus, j'ai besoin d'un indice" and the device responds with the appropriate hint audio within ~3 seconds. Pipeline:

```
mic → wake word ("Professeur Zacus") → VAD → captured chunk
   → MacStudio whisper STT
   → keyword fast-path OR Qwen 7B intent classifier
   → action (hints engine, skip, validate, ...)
   → Piper TTS
   → speaker
```

The pipeline lives mostly on the ESP32 (audio capture, wake-word, VAD) and on MacStudio (STT, LLM intent, TTS). The Zacus master orchestrates by HTTP POSTs to MacStudio.

## Non-Goals

- **Conversational AI** (chat-style back-and-forth). The voice is command-driven, not dialog-driven. The player asks for a hint, the NPC delivers; no follow-up turn.
- **Multi-language**. French only.
- **Multi-player isolation**. The mic captures the room, the response is for the room.
- **Privacy / on-device STT**. Audio leaves the ESP32, hits MacStudio. Acceptable since both are on the same LAN, escape room context = no expectation of privacy from the players.
- **Voice for atelier dev tools**. The atelier UI uses keyboard/mouse; voice is firmware-only.
- **PLIP retro phone voice** — separate hardware path (Si3210 SLIC), out of scope here.

## Constraints

- **Framework migration**: requires moving the master firmware from Arduino to ESP-IDF. Only ESP-IDF supports `esp-sr` (wake word + AFE) properly. This is a 2-3 month chantier and **gates** P3 onward.
- **Hardware**: existing Freenove ESP32-S3 WROOM N16R8 board, 2× I2S mics (existing), 1× I2S speaker (existing). 8 MB PSRAM allows large audio buffers.
- **Latency budget**: wake → response audio ≤ 3 s p95.
- **Power**: voice always-listening, but the wake-word detector consumes ~5 mA in detect-only mode. OK on USB-C powered devices.

---

## 1. Architecture

```
┌─────────────────────────────────────────────────┐
│ Zacus master ESP32-S3 (ESP-IDF after P1)         │
│                                                  │
│  audio_in_task (core 0)                          │
│    I2S RX 16 kHz mono                            │
│    → ESP-SR AFE (3-mic array, AEC, NS)           │
│    → ESP-SR wakenet (custom "Professeur Zacus")  │
│       │ (wake fired)                             │
│    → VAD (esp-sr's `vad_t`) → end-of-speech      │
│       │                                          │
│    → POST /voice/transcribe (multipart WAV)      │
│       │                                          │
│  voice_dispatcher_task (core 1)                  │
│    ↓ transcript: "j'ai besoin d'un indice"       │
│    1. keyword fast-path (regex):                 │
│         /indice|hint|aide/   → hints engine      │
│         /skip|passer/        → puzzle_skipped    │
│         /valider|valide/     → puzzle_validate   │
│         /annuler/            → action_cancel     │
│    2. else: POST /voice/intent (LLM Qwen 7B)     │
│       returns {action: "...", params: {...}}     │
│    3. dispatch to game_coordinator:              │
│         → engine.onEvent(...) → decisions[]      │
│         → audio_play_phrase(decisions[].text)    │
│  audio_out_task (core 0)                         │
│    Piper TTS via HTTP /tts                       │
│    → I2S TX speaker                              │
│    During playback: gate audio_in_task           │
│    (Q5b "mute during TTS")                       │
└─────────────────────────────────────────────────┘

       ↑↓ HTTP / WS over LAN  (192.168.0.150)

┌─────────────────────────────────────────────────┐
│ MacStudio M3 Ultra                              │
│   :8200 voice-bridge (FastAPI)                   │
│     POST /voice/transcribe (audio in → text)    │
│       → whisper.cpp /inference                   │
│     POST /voice/intent (text in → action)       │
│       → LiteLLM /v1/chat/completions Qwen 7B     │
│     POST /tts (text in → wav out)                │
│       → Piper                                    │
│   :4100 hints-server (existing per spec 5)       │
│   :4000 LiteLLM proxy                            │
│   :8001 Piper                                    │
│   :11434 ollama                                  │
└─────────────────────────────────────────────────┘
```

The voice-bridge on MacStudio is a thin FastAPI app that fans out to the right backend. Adding it (vs hitting whisper/Piper/LiteLLM directly) keeps the ESP32 client code simple — one host, three intuitive endpoints.

## 2. Decisions

| Decision | Choice | Rationale |
|----------|--------|-----------|
| **Framework** | Full ESP-IDF migration | esp-sr is IDF-native; Arduino wrapper is partial and lags. Migrate the master in one sprint, port modules incrementally |
| **Wake word** | Custom "Professeur Zacus" | Signature of the experience; worth the 1-2 day train (Espressif wakenet trainer + 30 min FR samples) |
| **STT mode** | VAD + batch | whisper-large-v3-turbo on 5-10 s chunks ≈ 400 ms on M3 Ultra; simpler than streaming and equally fast in practice |
| **Intent mapping** | Hybrid keyword + LLM | 4 hard-coded keywords cover 80% of commands at < 100 ms; Qwen 7B fallback for ambiguous transcripts |
| **AEC** | Mute during TTS (start) | Simplest; upgrade to esp-sr software AEC if play-while-listening UX needed later |

## 3. Wake-word training

Custom wakenet model "Professeur Zacus":
1. Record ~50 utterances of "Professeur Zacus" (varied speakers, distances, background noise) — 30 min effort.
2. Augment with synthetic noise (TV, kids playing, ambient escape-room ambient).
3. Run Espressif's WakeNet trainer (Docker image `espressif/wakenet-trainer`).
4. Output: `wn9s_professeur_zacus.bin` (~150 kB).
5. Drop in `partitions/` (LittleFS) ; ESP-IDF loads it via `esp_wn_iface_t`.

False-accept target: < 1 / 24 h of ambient. False-reject target: < 5 % at 2 m distance.

Fallback while training: ship with the pre-trained "hi_esp" wakenet ; player says "Hi ESP, j'ai besoin d'un indice" until the custom model is ready.

## 4. Wire format (voice-bridge HTTP)

### 4.1 `POST /voice/transcribe`

```http
POST /voice/transcribe HTTP/1.1
Host: 192.168.0.150:8200
Content-Type: audio/wav
Content-Length: <bytes>

<16-bit PCM 16 kHz mono WAV bytes, 1-15 s>
```

Response:
```json
{ "transcript": "professeur j'ai besoin d'un indice", "confidence": 0.92, "duration_ms": 380 }
```

### 4.2 `POST /voice/intent`

```http
POST /voice/intent HTTP/1.1
Content-Type: application/json

{
  "transcript": "professeur on a fini, valide pour passer",
  "context": {
    "current_puzzle": "P5_MORSE",
    "phase": "ADAPTIVE",
    "available_actions": ["request_hint", "skip_puzzle", "validate_solution", "cancel"]
  }
}
```

Response (Qwen 7B output, JSON mode):
```json
{ "action": "validate_solution", "params": { "puzzle_id": "P5_MORSE" }, "confidence": 0.88 }
```

If `confidence < 0.6`: server returns `{"action": "noop", "reason": "low_confidence"}` and the firmware plays a "Pardon, je n'ai pas compris" Piper phrase.

### 4.3 `POST /tts` (already exists per spec 4)

`{ "voice": "tom-medium", "text": "..." }` → WAV stream.

## 5. Phased plan

| Phase | Scope | Acceptance | Effort |
|-------|-------|------------|--------|
| **P1** ✅ **livré 2026-05-03** (slices 1–7, commits `8a6f4e3` → `479e92e` dans le submodule `ESP32_ZACUS`) | Master firmware Arduino → ESP-IDF migration. **Gating**. Convert `main.cpp` and the ~30 modules ; preserve features parity. Sous-tâches : **slice 1** IDF migration scaffold ✅ ; **slice 2** Wi-Fi STA + OTA wiring ✅ ; **slice 3** `media_manager` port ✅ ; **slice 4** NPC engine port ✅ ; **slice 5** `voice_pipeline` scaffold + `hints_client` HTTP ✅ ; **slice 6** esp-sr WakeNet9 placeholder `wn9_hiesp` (EN) ✅ — wake custom « Professeur Zacus » = action externe Espressif **PENDING** ; **slice 7** STT WebSocket streaming vers `/voice/ws` MacStudio ✅. | `pio run` succeeds in IDF mode ; flashed device passes the same boot test (SD mount, LittleFS, 30 s monitor no panic). Builds verts livrés sur la plage `8a6f4e3` → `479e92e`. | 100-160 h |
| **P2** | Voice-bridge FastAPI app on MacStudio (`/voice/transcribe`, `/voice/intent`, reuses `/tts`). ✅ couvert par le spec sœur `2026-05-03-tts-stt-llm-macstudio-design.md` (voice-bridge 0.4.0 livré 2026-05-03, part11). | `curl` smoke tests ; transcribe a known FR clip with whisper-large-v3-turbo and get expected text | 4-6 h |
| **P3** | esp-sr integration (AFE + wakenet `wn9_hiesp` placeholder). VAD chunk capture. POST to `/voice/transcribe`. ✅ partiellement livré via slice 6 (placeholder EN actif) ; reste : VAD chunking et POST end-to-end intégrés au game_coordinator. | Saying "hi ESP" wakes the device ; the next ≤10 s of speech is transcribed and printed to serial | 8-12 h |
| **P4** | Custom wakenet "Professeur Zacus" trained + flashed. Replace placeholder. **PENDING** — action externe Espressif (commande payante du modèle custom WakeNet9, délai 2-4 sem). | False-accept < 1/h ambient ; false-reject < 10% at 2 m (hand-tested) | 1-2 day train + 4 h integration |
| **P5** | Voice dispatcher: keyword fast-path (4 commands) → game_coordinator events. Plays acknowledgement TTS phrase. **PENDING** côté firmware hints integration (D_part5 : `puzzle_start`, `attempt_failed`, `group_profile` NVS). | "Professeur Zacus, je veux un indice" triggers the existing hints flow ; latency wake → audio out ≤ 3 s p95 | 6-8 h |
| **P6** | LLM intent fallback via Qwen 7B JSON mode. Confidence threshold + "pardon je n'ai pas compris" handler. | Synthetic ambiguous transcript ("euh, professeur, je sais pas, tu peux faire un truc") routes to LLM, returns `noop` low-confidence | 4-6 h |
| **P7** | Mute-during-TTS gate. Cooldown after wake to avoid double-trigger. **PENDING** (slice 9 firmware : TTS playback inverse + mute-during-TTS). | Speaker plays a 5 s NPC line ; mic doesn't re-trigger wake during that window | 2-3 h |
| **P8** | (optional) Software AEC via esp-sr if mute-only proves disruptive (player wants to interrupt the NPC). | Player can say wake word during NPC playback, NPC stops, listens | 6-10 h |

**Total core (P1-P7)**: ~125-200 h. **P1 (IDF migration) is 80% of the cost** — the rest is bounded once the framework switch is done.

## 6. Risks

| Risk | Probability | Mitigation |
|------|-------------|------------|
| ESP-IDF migration introduces regressions in master firmware (P1) | High by nature | Branch-based migration in `ESP32_ZACUS` ; keep Arduino branch operational ; comprehensive playtest harness (per spec 7) catches regressions |
| Custom wake word fails to converge (training data too thin) | Medium | Fall back to "hi_esp" placeholder ; iterate with more samples |
| Whisper STT mistranscribes accented speech (children, foreign visitors) | Medium-High | Confidence threshold; LLM fallback parses garbled transcripts; "pardon" graceful degradation |
| Speaker echo bypasses mute-during-TTS (room reverb) | Medium | Validate in field ; if issue, jump to AEC P8 |
| MacStudio voice-bridge becomes a single point of failure | Medium | ESP32 retries with exponential backoff ; if voice-bridge is down for > 30 s, firmware logs an analytics event and falls back to button-only hint flow |
| Qwen 7B intent classification leaks game state into hallucinated responses | Low | Strict JSON output mode + schema validation server-side ; reject malformed responses |

## 7. Acceptance gates (whole pipeline)

- [x] **Builds verts livrés sur la plage `8a6f4e3` → `479e92e`** (submodule `ESP32_ZACUS`, slices 1–7 P1, livré 2026-05-03) — `pio run` IDF mode OK, boot test SD/LittleFS/30 s monitor sans panic
- [x] **Voice-bridge MacStudio `/voice/{ws,transcribe,intent}` opérationnel** — couvert par le spec sœur `2026-05-03-tts-stt-llm-macstudio-design.md` (voice-bridge 0.4.0, part11)
- [x] **STT WebSocket streaming end-to-end** (slice 7) — firmware → `/voice/ws` → whisper → intent dispatch
- [ ] Wake → STT → keyword route → audio out ≤ 3 s p95 (hint flow) → **PENDING** (gating sur slice 9 + D_part5)
- [ ] Wake → STT → LLM intent → audio out ≤ 5 s p95 (ambiguous flow) → **PENDING**
- [ ] False-accept rate of "Professeur Zacus" < 1 / hour ambient → **PENDING** (gating sur P4 wake custom Espressif)
- [ ] False-reject rate < 10 % at 2 m distance (3 voice samples per direction) → **PENDING** (gating sur P4)
- [ ] Voice flow degrades gracefully when MacStudio voice-bridge unreachable (button-only mode)
- [ ] Playtest harness has ≥ 1 voice-driven scenario (mocked transcripts replay through voice dispatcher)
- [ ] No false re-triggering during NPC TTS playback (mute gate verified) → **PENDING** (slice 9)
- [ ] ESP-IDF master firmware passes the same playtest set as the Arduino version (no regression vs P1 baseline)

## 8. Out of scope

- True conversational dialog (multi-turn). Future enhancement when needed.
- Hot-word configurable per scenario (different wake words per game pack).
- On-device STT (privacy mode). Could revisit with whisper.cpp tiny-FR if a future deployment needs it offline.
- Voice biometrics (recognizing individual players).
- Visual ASR confirmation on the screen (subtitle of what was understood).
- AEC P8 is optional, only triggered if player UX demands interruption.

## 9. Open questions

None at design stage. All decisions resolved during 2026-05-03 brainstorm:
- Framework: full ESP-IDF migration ✅
- Wake word: custom "Professeur Zacus" ✅
- STT: VAD + batch ≤ 10 s chunks ✅
- Intent: hybrid keyword + Qwen 7B fallback ✅
- AEC: mute-during-TTS first, esp-sr software AEC later if needed ✅
