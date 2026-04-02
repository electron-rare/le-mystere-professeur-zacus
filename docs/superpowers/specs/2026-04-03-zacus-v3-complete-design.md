# Le Mystère du Professeur Zacus — V3 Complete Design Spec

## Summary

Mobile escape room kit in 2-3 suitcases. Professor Zacus is an eccentric AI NPC who "tests" players, adapts difficulty in real-time, profiles groups (tech/non-tech), and controls game duration (30-90 min). Hub central (RTC_PHONE + BOX-3) + 7 autonomous puzzle objects communicating via ESP-NOW. XTTS-v2 cloned voice for live speech, Piper TTS fallback. Three commercial tiers: animation (800-1500€), rental (300-500€/weekend), kit sale (3000-5000€, BOM ~491€).

## Game Design

### Narrative

Professor Zacus is a meta-character — he knows the players are in an escape room. He tests them, observes, comments, teases. The game IS the test. The Professor watches through cameras (BOX-3), listens through the phone, and adapts everything.

"Bienvenue dans mon laboratoire... portable. Aujourd'hui, c'est VOUS qui êtes l'expérience."

### Structure

```
PHASE 1: INTRO (5 min)
  → Phone rings. Professor introduces himself.
  → Players learn the rules through the Professor's monologue.

PHASE 2: PROFILING (10 min)
  → Puzzle A: logic/pattern (non-tech) — measures speed
  → Puzzle B: mini LED circuit (tech) — measures technical comfort
  → NPC classifies group: TECH / NON_TECH / MIXED
  → NPC sets target_duration based on game master config

PHASE 3: ADAPTIVE PARCOURS (20-60 min)
  → 4-8 puzzles selected from pool based on group profile
  → TECH group: circuits, frequencies, morse code, components
  → NON_TECH group: symbols, sequences, colors, sounds
  → MIXED: alternates both types
  → NPC dynamically adds/removes puzzles based on pace
  → Fast group → bonus puzzles, false leads
  → Slow group → proactive hints, skip optional puzzles

PHASE 4: CLIMAX (10 min)
  → Final collaborative puzzle requiring all players
  → Code assembled from all previous puzzle solutions
  → Opens the final chest

PHASE 5: OUTRO (5 min)
  → Professor's verdict: score, personal comments
  → "Diplôme du Professeur Zacus" (printed or digital)
  → Stats: time, hints used, puzzles solved, team rating
```

### Puzzle Pool (7 puzzles)

| ID | Name | Object | Tech | Interaction | Profile |
|----|------|--------|------|-------------|---------|
| P1 | Séquence Sonore | Box with speaker + 4 buttons | ESP32 + I2S + buttons | Listen to melody, reproduce sequence | BOTH |
| P2 | Circuit LED | Giant magnetic breadboard (60x40cm folding) | LED strips + magnetic sensors | Place components correctly to light the LED | TECH |
| P3 | QR Treasure | 6 laminated QR codes hidden in room | ESP32-S3 camera (BOX-3) | Scan in correct order | BOTH |
| P4 | Fréquence Radio | Retro radio enclosure (3D printed) | ESP32 + DAC + rotary encoder | Find the correct frequency | TECH |
| P5 | Code Morse | Telegraph key (wood + metal) | ESP32 + buzzer + button | Decode the Professor's morse message | MIXED |
| P6 | Symboles Alchimiques | Laser-cut wooden tablet with symbols | NFC reader + 12 NFC tags | Place symbols in correct order on reader | NON_TECH |
| P7 | Coffre Final | Wooden box with electronic lock | ESP32 + servo + 4x3 keypad | Enter code assembled from all puzzles | BOTH |

### Adaptive Difficulty Rules

```c
// In npc_engine.cpp
if (scene_elapsed < expected_time * FAST_THRESHOLD) {
    // Group is fast → add challenge
    if (puzzles_solved < total_puzzles - 1) {
        add_bonus_puzzle();  // or add_false_lead();
    }
    npc_mood = NPC_MOOD_IMPRESSED;
}

if (scene_elapsed > expected_time * SLOW_THRESHOLD) {
    // Group is slow → help
    if (hints_given[current_scene] < MAX_HINT_LEVEL) {
        proactive_hint();
    } else {
        skip_to_next_puzzle();
    }
    npc_mood = NPC_MOOD_WORRIED;
}

// Duration targeting
if (total_elapsed > target_duration * 0.8 && puzzles_remaining > 2) {
    skip_optional_puzzles();
}
```

### Group Profiling

After Phase 2 (profiling puzzles), the NPC sets:
```c
typedef enum {
    GROUP_TECH,      // Puzzle B solved fast, Puzzle A normal
    GROUP_NON_TECH,  // Puzzle A solved fast, Puzzle B struggled
    GROUP_MIXED,     // Both similar speed
} group_profile_t;
```

This determines the puzzle selection for Phase 3:
- TECH: P2 (circuit), P4 (radio), P5 (morse), P3 (QR), P7 (coffre)
- NON_TECH: P1 (son), P6 (symboles), P3 (QR), P5 (morse light), P7 (coffre)
- MIXED: P1, P2, P3, P5, P6, P7 (full set, Professor alternates)

## Hardware — Mobile Kit

### Suitcase 1: Hub Central

| Component | Model | Purpose | Power |
|-----------|-------|---------|-------|
| RTC_PHONE | Custom (ESP32 + SLIC codec) | NPC voice, player interaction hub | USB-C battery |
| BOX-3 | ESP32-S3-BOX-3 | Screen + camera (QR scan), status display | USB-C battery |
| WiFi Router | GL.iNet GL-MT3000 (or similar) | Local network + internet uplink for TTS | USB-C |
| Battery | Anker 20000mAh × 2 | Powers hub + router | — |
| Cables | USB-C × 5 | Interconnects | — |

### Suitcase 2: Puzzles

| Component | Enclosure | ESP32 | Sensors/Actuators |
|-----------|-----------|-------|-------------------|
| Boîte Sonore | 3D printed, 15×10×8cm | ESP32-S3-DevKit | I2S speaker MAX98357A, 4 arcade buttons |
| Breadboard Magnétique | Folding board 60×40cm | ESP32-DevKit | 8 reed switches, 8 LED strips (WS2812) |
| Poste Radio | 3D printed retro radio, 20×15×12cm | ESP32-DevKit | Rotary encoder, I2S DAC + speaker, OLED 128x64 |
| Télégraphe | Wood base + brass key | ESP32-DevKit | Tactile button, buzzer, LED |
| Tablette Symboles | Laser-cut wood, 30×20cm | ESP32-DevKit | RC522 NFC reader, 12 NTAG213 tags |
| Coffre Final | Wood box 25×20×15cm | ESP32-DevKit | SG90 servo (latch), 4×3 membrane keypad, RGB LED |
| QR Codes | 6 laminated A5 cards | — (BOX-3 camera) | — |

### Suitcase 3: Infrastructure

| Component | Purpose |
|-----------|---------|
| Speaker Bluetooth | Ambient audio (AudioCraft generated) |
| LED strip USB (3m) | Room ambiance lighting |
| Mini tripod + phone holder | For BOX-3 camera positioning |
| Signage (4 panels) | "Laboratoire du Pr. Zacus", rules, zones |
| Deployment guide (laminated) | 15-min setup checklist |
| Spare batteries + cables | Redundancy |

### Communication Protocol

```
                    WiFi (if available)
BOX-3 ←————————→ Internet → vLLM/XTTS (live TTS)
  ↓                              ↓ fallback
  ↓ ESP-NOW (always available)   Piper TTS (Tower)
  ↓                              ↓ fallback
RTC_PHONE ←——→ Puzzle ESP32s     SD card MP3s
  (broadcast)   (6 devices)
```

**Offline mode**: Everything works without WiFi via ESP-NOW + SD card MP3. The Professor speaks from pre-generated MP3s. QR scanning works locally on BOX-3.

**Online mode**: Live TTS via XTTS-v2 or Piper for dynamic responses. Analytics uploaded post-game.

### Setup Time Target: 15 minutes

1. Open suitcase 1, place phone + BOX-3 on table (2 min)
2. Open suitcase 2, distribute puzzles in room (5 min)
3. Open suitcase 3, plug speaker + LED + signage (3 min)
4. Power on all devices, wait for ESP-NOW mesh (2 min)
5. Game master configures: target_duration, WiFi credentials (3 min)

## Sprint 4: Audio Immersif

### XTTS-v2 Voice Cloning

- **Deploy**: Docker on KXKM-AI (RTX 4090)
- **Voice sample**: 30s recording of "Professor Zacus" voice (dramatic, French, theatrical)
- **API**: OpenAI-compatible endpoint for easy integration with existing tts_client.cpp
- **Latency**: < 3s for 10-word sentence (GPU inference)
- **Fallback chain**: XTTS → Piper TTS → SD card MP3
- **Quality**: XTTS-v2 for personality, emotion markers in text (excitement, worry, amusement)

### AudioCraft Ambient Sound

- **Deploy**: AudioCraft (Meta MusicGen) on KXKM-AI
- **Pre-generate** 6 ambient tracks (not live generation):
  - `lab_ambiance.mp3` (30 min loop: machines, beeps, ventilation)
  - `tension_rising.mp3` (5 min: dramatic buildup)
  - `victory.mp3` (30s: fanfare, applause)
  - `failure.mp3` (15s: buzzer, Professor laugh)
  - `thinking.mp3` (10 min loop: subtle suspense)
  - `transition.mp3` (10s: scene change swoosh)
- **Playback**: Bluetooth speaker from suitcase 3, controlled by BOX-3 via BLE

## Commercialization

### Tier 1: Animation (Premium)

| Item | Detail |
|------|--------|
| **Price** | 800-1500€ / session |
| **Duration** | 2-3h on-site (setup + game + debrief) |
| **Players** | 4-15 |
| **Includes** | Travel, setup, animation by L'Electron Rare, personalized debrief |
| **Target** | Corporate team building, events, conferences |
| **Margin** | ~80% (cost = time + transport) |

### Tier 2: Rental

| Item | Detail |
|------|--------|
| **Price** | 300-500€ / weekend |
| **Duration** | Fri delivery → Mon pickup |
| **Includes** | 3 suitcases, deployment guide, phone support, 1 pre-configured scenario |
| **Target** | Associations, municipalities, birthdays, school events |
| **Margin** | ~60% (depreciation + logistics) |
| **Deposit** | 1000€ (refundable) |

### Tier 3: Kit Sale

| Item | Detail |
|------|--------|
| **Price** | 3000-5000€ |
| **Includes** | 3 suitcases (all hardware), firmware flashed, 3 scenarios, documentation, 1h training, 1 year firmware updates |
| **Target** | Escape room businesses, leisure centers, museums, schools |
| **Margin** | ~83% (BOM ~491€) |
| **Recurring** | Additional scenarios 200€/year, premium support 500€/year |

### BOM (1 kit)

| Category | Items | Cost |
|----------|-------|------|
| Hub | BOX-3 (25€) + RTC_PHONE custom (40€) + WiFi router (30€) | 95€ |
| Puzzles | 6× ESP32-DevKit (48€) + speakers (15€) + NFC (15€) + servo+keypad (8€) | 86€ |
| Enclosures | 5× 3D printed (50€) + breadboard magnétique (30€) + composants mag (20€) + bois laser (30€) | 130€ |
| Power | 3× batteries 20000mAh (75€) | 75€ |
| Infra | Speaker BT (20€) + LED strip (10€) + valises (75€) + câbles (30€) | 135€ |
| **Total** | | **~521€** |

### Commercial Pipeline

```
Website (lelectronrare.fr/zacus) → Demo video → Contact form
    ↓
Dolibarr: Lead → Qualify → Demo (live or video call)
    ↓
Devis (Tier 1/2/3) → Signature → Invoice → Delivery/Animation
    ↓
Follow-up: additional scenarios, referrals, recurring revenue
```

### Marketing Assets Needed

- Demo video (2-3 min) showing a real playthrough
- Landing page on lelectronrare.fr/zacus
- One-pager PDF (printable) for sales meetings
- Pricing table with 3 tiers
- Testimonial from beta playtest

## Implementation Phases

| Phase | Scope | Dependencies | Est. Hours |
|-------|-------|-------------|------------|
| 1 | Game design finalization (puzzle specs, scoring, timing) | Existing npc_engine | 10h |
| 2 | Hardware prototyping (7 puzzle objects) | ESP32 boards, 3D printer, laser cutter | 40h |
| 3 | Firmware for 7 puzzles (ESP-NOW, game logic) | Phase 2 hardware | 30h |
| 4 | NPC integration complete (profile, adaptive, duration) | Phase 1 + existing npc_integration.cpp | 15h |
| 5 | XTTS-v2 deployment + voice clone | KXKM-AI GPU | 8h |
| 6 | AudioCraft ambiance generation | KXKM-AI GPU | 4h |
| 7 | Suitcase packaging + deployment guide | Phase 2 hardware, all firmware | 10h |
| 8 | Beta playtest (2 groups minimum) | Everything | 8h |
| 9 | Commercial: landing page, video, pricing, Dolibarr pipeline | Phase 8 feedback | 15h |
| **Total** | | | **~140h** |

## Success Criteria

- Beta playtest: 2 groups complete the game, NPS > 8/10
- Setup time: < 15 minutes from suitcase open to game start
- Offline mode: works completely without internet
- Duration accuracy: actual game time within ±10% of target_duration
- NPC profiling: correctly identifies group type in > 80% of playtests
- Voice quality: testers cannot distinguish XTTS from human in blind test
- Kit durability: survives 50+ deployments without hardware failure
