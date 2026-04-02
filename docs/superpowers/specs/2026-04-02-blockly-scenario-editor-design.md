# Blockly Scenario Editor — Design Spec

**Date**: 2026-04-02
**Repo**: electron-rare/le-mystere-professeur-zacus
**Location**: Integrated into `frontend-scratch-v2/` as new tab

## Goal

Visual Blockly-based scenario editor for creating escape room games. Targets open-source community (makers) and educational workshops (kids/teens). Outputs Runtime 3 YAML directly, ready to compile and flash to ESP32.

## Target Users

- Open-source makers creating custom Zacus escape rooms
- Kids/teens in educational workshops (Scratch-like experience)
- Game designers without coding skills

## Architecture

```
frontend-scratch-v2/src/
├── components/
│   ├── ScenarioEditor/              ← NEW
│   │   ├── ScenarioEditor.tsx        ← Main: Blockly workspace + YAML preview
│   │   ├── blocks/
│   │   │   ├── scene.ts              ← Scene, transition, timer, variable
│   │   │   ├── puzzle.ts             ← Puzzle, condition, validation (QR/button/sequence)
│   │   │   ├── npc.ts                ← NPC say, mood, hints, conversation (LLM)
│   │   │   ├── hardware.ts           ← GPIO, LED, buzzer, audio, QR scanner
│   │   │   └── deploy.ts             ← WiFi, TTS, LLM, OTA config
│   │   ├── generators/
│   │   │   └── yaml.ts               ← Blockly block tree → YAML Runtime 3 string
│   │   ├── toolbox.ts                ← 6-category toolbox definition
│   │   └── preview.ts                ← Real-time YAML preview (Monaco read-only)
```

## Block Categories (6 families, ~30 blocks)

### Scenario (purple)
| Block | Inputs | Output |
|-------|--------|--------|
| `scene` | name, description, duration_max | Scene container |
| `transition` | from, to, condition | Scene link |
| `on_start` / `on_end` | action list | Scene lifecycle |
| `timer` | seconds, on_expire action | Countdown |
| `variable_set` / `variable_get` | name, value | Game state |

### Puzzles (blue)
| Block | Inputs | Output |
|-------|--------|--------|
| `puzzle` | name, type (QR/button/sequence/free) | Puzzle definition |
| `condition` | type (puzzle_solved/timer/variable) | Boolean |
| `validation_qr` | expected QR string | QR check |
| `validation_button` | pin number | GPIO check |
| `validation_sequence` | ordered action list | Sequence check |

### NPC / Dialogue (green)
| Block | Inputs | Output |
|-------|--------|--------|
| `npc_say` | text, mood | TTS phrase |
| `npc_mood` | mood enum | Mood change |
| `hint` | level (1-3), text, puzzle_id | Hint definition |
| `npc_react` | condition, response text | Conditional dialogue |
| `conversation` | system_prompt, context | LLM block |

### Hardware (orange)
| Block | Inputs | Output |
|-------|--------|--------|
| `gpio_write` | pin, state | Digital output |
| `gpio_read` | pin, variable | Digital input |
| `led_set` | color, animation | RGB control |
| `buzzer_tone` | frequency, duration | Audio alert |
| `play_audio` | filename | MP3 from SD |
| `qr_scan_trigger` | — | Start QR scan |

### Deploy (red)
| Block | Inputs | Output |
|-------|--------|--------|
| `config_wifi` | ssid, password | Network setup |
| `config_tts` | url, voice | Piper TTS endpoint |
| `config_llm` | url, model, system_prompt | Ollama endpoint |
| `config_ota` | firmware_url | OTA update |
| `deploy_esp32` | — | Generate flash bundle |

### Logic (grey — Blockly built-in)
- if/else, and/or/not, repeat, wait — standard Blockly blocks

## YAML Generator

The Blockly generator walks the block tree and produces `zacus_v2.yaml` format directly:

```yaml
version: 3
metadata:
  name: "My Escape Room"
  author: "Player"
  duration_max: 3600

scenes:
  - id: intro
    npc:
      say: "Welcome to my laboratory!"
      mood: neutral
    transitions:
      - to: puzzle_1
        condition: { type: timer, seconds: 10 }

puzzles:
  - id: puzzle_1
    type: qr
    solution: "ZACUS_KEY_1"
    hints:
      - level: 1
        text: "Look around the bookshelf"

hardware:
  gpio:
    - pin: 4
      mode: output
      trigger: { puzzle: puzzle_1, on: solved }
      action: HIGH

deploy:
  tts: { url: "http://192.168.0.120:8001", voice: "tom-medium" }
  llm: { url: "http://kxkm-ai:11434", model: "devstral" }
```

## UI Layout

Split panel:
- **Left (60%)**: Blockly workspace with categorized toolbox
- **Right (40%)**: Monaco editor showing generated YAML (read-only, live update)
- **Top bar**: File name, Save, Export YAML, Export Bundle (ZIP), Flash ESP32, Share link
- **Bottom bar**: Validation status (schema errors from Zod), block count, scenario stats

## Export Options

1. **Download YAML** — single `.yaml` file
2. **Download Bundle** — ZIP containing YAML + auto-generated MP3 pool (if NPC phrases defined) + deploy config
3. **Flash ESP32** — Web Serial API (Chrome) for direct USB flash from browser
4. **Share** — URL with scenario encoded (base64 + LZ compression), shareable link

## Validation

- Real-time Zod schema validation on generated YAML
- Blockly workspace validation: orphan blocks, missing connections
- Runtime 3 compiler compatibility check (can the YAML compile?)
- Warnings: unreachable scenes, puzzles without hints, NPC without dialogue

## Testing Strategy

- Unit tests for each block generator (block → YAML fragment)
- Integration test: full scenario → YAML → compile_runtime3.py → valid IR
- Snapshot tests for complex scenarios (known input blocks → known YAML output)
- E2E: Playwright test creating a simple scenario in the browser

## Dependencies

Already in frontend-scratch-v2:
- Blockly 12.4
- Monaco Editor
- React 19
- Vite + TypeScript
- Zod (validation)
- Vitest (testing)

New (none required — all deps already present).

## Out of Scope (v1)

- Collaborative editing (multi-user)
- Version history / undo beyond Blockly's built-in
- Scenario marketplace / community sharing platform
- Visual simulation (play the scenario in browser) — separate feature
