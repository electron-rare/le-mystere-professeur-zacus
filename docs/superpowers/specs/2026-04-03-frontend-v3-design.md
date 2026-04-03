# Frontend Zacus V3 — Éditeur Blockly + Dashboard Game Master + Simulation 3D — Design Spec

## Summary

Monorepo with 3 apps: Blockly scenario editor (7 V3 puzzles, adaptive NPC config), real-time game master dashboard (expert + simple modes via WebSocket to BOX-3), and 3D digital twin simulation (Three.js, playable in browser, demo/sandbox/test modes). Shared scenario-engine portable to both web and ESP32.

## Architecture

```
frontend-v3/ (monorepo, pnpm workspaces)
├── apps/
│   ├── editor/          React 19 + Blockly 12.4 — scenario editor
│   ├── dashboard/       React 19 + WebSocket — game master cockpit
│   └── simulation/      React 19 + Three.js — 3D digital twin
├── packages/
│   ├── shared/          TS types, constants, YAML parser
│   ├── scenario-engine/ Runtime 3 IR engine (portable web + C)
│   └── ui/              Design system (Tailwind + Radix)
```

## Component 1: Blockly Editor V3 (apps/editor)

### Purpose
Visual scenario editor. Game designers build escape room scenarios by dragging blocks. Outputs `zacus_v3_complete.yaml`.

### New Blockly Blocks (V3)

| Block | Category | Inputs | Output |
|-------|----------|--------|--------|
| `puzzle_sequence_sonore` | Puzzles | melody notes[], difficulty, code_digits | P1 config |
| `puzzle_circuit_led` | Puzzles | components[], valid_circuit, code_digit | P2 config |
| `puzzle_qr_treasure` | Puzzles | qr_codes[6], correct_order[], code_digit | P3 config |
| `puzzle_radio` | Puzzles | target_freq, range_min, range_max, code_digit | P4 config |
| `puzzle_morse` | Puzzles | message, mode(tech/light), code_digits | P5 config |
| `puzzle_symboles_nfc` | Puzzles | symbols[12], correct_order[], code_digits | P6 config |
| `puzzle_coffre_final` | Puzzles | code_length, max_attempts | P7 config |
| `npc_profiling` | NPC | tech_threshold_s, nontech_threshold_s | Profiling config |
| `npc_duration` | NPC | target_minutes, mode(30/45/60/90) | Duration config |
| `npc_adaptive_rule` | NPC | condition(fast/slow), action(add/skip/hint) | Rule |
| `phase_container` | Flow | phase_type(INTRO/PROFILING/ADAPTIVE/CLIMAX/OUTRO) | Phase wrapper |
| `puzzle_selector` | Flow | profile_filter(TECH/NON_TECH/BOTH), puzzle_ref | Puzzle in phase |

### Bidirectional YAML

- **Export**: Blockly workspace → `zacus_v3_complete.yaml` via `compile_runtime3.py`
- **Import**: `zacus_v3_complete.yaml` → Blockly workspace (reverse parser)
- Lossless round-trip for all V3 constructs

### UI Layout

```
┌─────────────────────────────────────────────────┐
│ [File ▾] [Edit ▾] [Run ▾]  Zacus V3 Editor     │
├────────────┬────────────────────────────────────┤
│ Toolbox    │  Blockly Workspace                 │
│            │                                    │
│ 📦 Puzzles │  ┌──────────────────────────┐      │
│  P1 Son    │  │ PHASE: INTRO             │      │
│  P2 LED    │  │  └─ NPC say "Bienvenue"  │      │
│  P3 QR     │  ├──────────────────────────┤      │
│  P4 Radio  │  │ PHASE: PROFILING         │      │
│  P5 Morse  │  │  ├─ P1 (BOTH)            │      │
│  P6 NFC    │  │  └─ P2 (TECH)            │      │
│  P7 Coffre │  ├──────────────────────────┤      │
│            │  │ PHASE: ADAPTIVE          │      │
│ 🤖 NPC     │  │  ├─ if TECH: P4,P5,P3   │      │
│  Profiling │  │  └─ if NON_TECH: P6,P1   │      │
│  Duration  │  ├──────────────────────────┤      │
│  Rule      │  │ PHASE: CLIMAX            │      │
│            │  │  └─ P7 Coffre            │      │
│ 🔀 Flow    │  └──────────────────────────┘      │
│  Phase     │                                    │
│  Selector  │  [YAML Preview]  [Validate]        │
├────────────┴────────────────────────────────────┤
│ Console: ✅ Scenario valid, 7 puzzles, ~60 min  │
└─────────────────────────────────────────────────┘
```

### Validation

- All puzzle code_digits sum to 8 total digits
- Every puzzle referenced in at least one phase
- Profiling phase has exactly 2 puzzles (one TECH, one BOTH)
- Coffre final is in CLIMAX phase
- Duration target is set
- NPC phrases exist for all referenced puzzles

## Component 2: Dashboard Game Master (apps/dashboard)

### Purpose

Real-time cockpit for controlling a live game session. Connects to BOX-3 via WebSocket.

### Expert Mode (for L'Electron Rare animator)

| Panel | Content |
|-------|---------|
| **Header** | Session ID, elapsed time, target duration, progress bar |
| **Puzzle Status** | 7 puzzle cards: ✅ solved / 🔄 active / ⬜ pending / ⏭ skipped. Time per puzzle. |
| **NPC Panel** | Current mood (emoji), detected profile, hints given, last phrase spoken |
| **Timeline** | Scrolling log of all events (puzzle solved, hint given, NPC spoke, phone rang) |
| **Controls** | Force Hint, Skip Puzzle, Add Bonus, Ring Phone, Change Duration, Manual TTS input, Override NPC mood |
| **Audio** | Current ambient track, volume slider, manual track select |

### Simple Mode (for Tier 2 rental clients)

| Panel | Content |
|-------|---------|
| **Header** | Timer, progress bar, puzzle count |
| **Status** | "Le Professeur gère tout automatiquement" |
| **Controls** | Start, Pause, End. That's it. |
| **Emergency** | "Appeler le support" button (calls L'Electron Rare) |

### WebSocket Protocol

```typescript
// BOX-3 → Dashboard (events)
interface GameEvent {
  type: 'puzzle_solved' | 'puzzle_skipped' | 'hint_given' | 'npc_spoke' |
        'phone_rang' | 'phase_changed' | 'timer_update' | 'profile_detected' |
        'game_start' | 'game_end' | 'hw_failure';
  timestamp: number;
  data: Record<string, any>;
}

// Dashboard → BOX-3 (commands)
interface GameCommand {
  type: 'force_hint' | 'skip_puzzle' | 'add_bonus' | 'ring_phone' |
        'set_duration' | 'manual_tts' | 'override_mood' | 'pause' |
        'resume' | 'end_game';
  data: Record<string, any>;
}
```

### Connection

- Auto-discover BOX-3 on local WiFi via mDNS (`zacus-box3.local:81`)
- Fallback: manual IP input
- Reconnect on disconnect (3s retry)
- Offline indicator when disconnected

## Component 3: Simulation 3D (apps/simulation)

### Purpose

3D digital twin of the physical escape room kit. Playable in browser. Three modes: demo (auto-play for commercial), sandbox (free play), test (validate scenario YAML).

### 3D Scene

Low-poly room (reuse style from Moodle 3D lab):
- Room: 6m × 4m, industrial style, dim lighting
- Central table: RTC_PHONE (3D model, rings when NPC calls)
- 7 puzzle stations around the room (3D models matching physical objects)
- Ambient particles (dust in light beams)
- Soundtrack from AudioCraft tracks

### Puzzle Interactions

| Puzzle | 3D Model | Browser Interaction |
|--------|----------|-------------------|
| P1 Séquence | Box with 4 colored buttons | Click buttons, hear tones via Web Audio |
| P2 Circuit | Breadboard on table | Drag magnetic components onto board |
| P3 QR | Cards on walls | Click card → camera view → auto-scan |
| P4 Radio | Retro radio on shelf | Rotate dial (mouse drag), hear static/signal |
| P5 Morse | Telegraph on desk | Press spacebar = morse key, hear buzzer |
| P6 Symboles | Wooden tablet | Drag symbol tiles into NFC reader slots |
| P7 Coffre | Box in corner | Click keypad numbers, enter code |

### Modes

**Demo Mode** (auto-play):
- Virtual players solve puzzles automatically (scripted sequence)
- NPC narrates throughout (TTS via Web Speech API or Piper)
- Camera moves cinematically between puzzles
- Perfect for embedding on lelectronrare.fr/zacus

**Sandbox Mode** (free play):
- Player controls camera (orbit)
- Click puzzles to interact
- NPC reacts to player actions
- No timer pressure
- Good for testing puzzle design

**Test Mode** (scenario validation):
- Load any `zacus_v3_complete.yaml`
- Simulates a group profile (configurable: TECH/NON_TECH/MIXED/FAST/SLOW)
- Runs the scenario-engine at accelerated speed
- Reports: estimated duration, puzzle order, NPC decisions, code assembly
- Validates timing, profiling logic, scoring

### NPC in Simulation

- Text-to-speech via Web Speech API (free, offline) or Piper TTS API (if online)
- Speech bubbles above the phone 3D model
- Mood visualized: phone glows with mood color (blue=neutral, green=impressed, orange=worried, purple=amused)

## Shared Package: scenario-engine

Portable game engine — same logic runs in:
- **Browser** (simulation + editor validation)
- **ESP32** (BOX-3 firmware, compiled to C)

### API

```typescript
interface ScenarioEngine {
  load(yaml: string): void;
  start(config: { targetDuration: number; mode: '30'|'45'|'60'|'90' }): void;
  tick(nowMs: number): EngineState;
  onEvent(event: GameEvent): EngineDecision[];
  getState(): EngineState;
  getScore(): ScoreResult;
}

interface EngineState {
  phase: Phase;
  groupProfile: GroupProfile | null;
  activePuzzle: string | null;
  solvedPuzzles: string[];
  skippedPuzzles: string[];
  hintsGiven: Record<string, number>;
  npcMood: NpcMood;
  elapsedMs: number;
  codeAssembled: string;
}

interface EngineDecision {
  action: 'speak' | 'ring_phone' | 'play_sound' | 'add_puzzle' | 'skip_puzzle' | 'change_mood';
  data: Record<string, any>;
}
```

### Portability

- Written in TypeScript
- Compiled to JS for browser (standard)
- Compiled to C via codegen script for ESP32 (generates `scenario_engine.c` from the TS source)

## Shared Package: ui

Design system matching Theme FER (Apple SOTA 2026):
- Tailwind CSS
- Radix UI primitives
- Components: Button, Card, Badge, Progress, Dialog, Tooltip, Toggle
- Dark mode support
- Colors: same as Theme FER (`--fer-accent: #0071e3`, course accent colors)

## Monorepo Setup

```json
// pnpm-workspace.yaml
packages:
  - 'apps/*'
  - 'packages/*'

// turbo.json
{
  "pipeline": {
    "build": { "dependsOn": ["^build"] },
    "dev": { "cache": false },
    "test": { "dependsOn": ["build"] },
    "lint": {}
  }
}
```

## Testing

| App | Tests |
|-----|-------|
| editor | Vitest: block serialization, YAML round-trip, validation rules |
| dashboard | Vitest: WebSocket mock, state management, command dispatch |
| simulation | Vitest + Playwright: 3D scene loads, puzzle interactions work |
| scenario-engine | Vitest: all game logic, profiling, scoring, duration targeting |

## Implementation Priority

| Order | Component | Est. Hours |
|-------|-----------|-----------|
| 1 | packages/shared + scenario-engine | 15h |
| 2 | apps/editor (Blockly V3 blocks + YAML) | 25h |
| 3 | apps/dashboard (expert + simple modes) | 20h |
| 4 | apps/simulation (3D scene + 7 puzzles) | 35h |
| 5 | packages/ui (design system) | 8h |
| 6 | Integration + testing | 12h |
| **Total** | | **~115h** |
