# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Overview

**Le Mystère du Professeur Zacus** — hybrid educational escape room game with ESP32-S3 hardware, React authoring studio, and a portable Runtime 3 scenario engine. Scenarios are authored in YAML, compiled to a portable IR, and executed on both web and ESP32 targets.

## Build & Test

```bash
# Full validation pipeline
make all-validate

# Content checks (schema validation for scenarios, audio, printables)
make content-checks

# Runtime 3 — compile, simulate, verify, test
make runtime3-compile                    # YAML → Runtime 3 IR
make runtime3-simulate                   # simulate scenario execution
make runtime3-verify                     # verify pivot logic
make runtime3-test                       # Python unittest suite

# Frontend (React 19 + Blockly + Vite)
cd frontend-scratch-v2
npm test                                 # Vitest (18 tests)
npm run build                            # tsc -b + vite build
npx vitest run tests/specific.test.ts    # single test

# Compile a specific scenario
python3 tools/scenario/compile_runtime3.py game/scenarios/zacus_v2.yaml

# Dev CLI (12 actions)
./tools/dev/zacus.sh content-checks
./tools/dev/zacus.sh runtime3-compile
./tools/dev/zacus.sh voice-bridge start|stop|status|test
```

## Architecture

### Data Flow
```
YAML scenario → compile_runtime3.py → Runtime 3 IR → ESP32 / Web player
                                                    ↓
                                              Voice bridge → Piper TTS (Tower:8001)
                                                    ↓
                                              Hints engine → /hints/ask (3 puzzles, 3 levels)
```

### Key Components

- **Scenario engine**: `game/scenarios/zacus_v2.yaml` is the canonical source. Runtime 3 spec in `specs/ZACUS_RUNTIME_3_SPEC.md`.
- **Frontend studio** (`frontend-scratch-v2/`): React 19 + Blockly 12.4 + Monaco Editor + Zod validation. Vite bundler, TypeScript.
- **ESP32 firmware** (`ESP32_ZACUS/` submodule): Freenove ESP32-S3, PlatformIO build. Separate repo with its own CI. Contains voice pipeline scaffold, audio/vision/QR detection, media manager.
- **Runtime 3 compiler** (`tools/scenario/compile_runtime3.py`): YAML → portable IR with pivots, zones, triggers.
- **Voice pipeline**: Piper TTS on Tower:8001 (3 voices: zacus=tom-medium, siwis, upmc), ESP-SR for wake word. Voice bridge routes `[HINT:puzzle:level]` to hints engine.
- **MCP hardware server** (`tools/dev/mcp_hardware_server.py`): 6 tools, stdio transport for hardware interaction.
- **TUI dashboard** (`tools/dev/zacus_tui.py`): 12 actions, logs, CI mode.
- **Analytics**: ESP32 module + 6 web endpoints + Dashboard UI.
- **NPC Engine** (`ESP32_ZACUS/ui_freenove_allinone/include/npc/npc_engine.h` + `src/npc/npc_engine.cpp`): Lightweight C state machine for Professor Zacus NPC. Trigger rules (stuck timer, QR scan, fast/slow progress, hint request), mood system (neutral/impressed/worried/amused), hybrid audio routing (live Piper TTS when Tower reachable, SD card MP3 fallback). NPC phrase bank in `game/scenarios/npc_phrases.yaml`.
- **TTS Client** (`ESP32_ZACUS/ui_freenove_allinone/include/npc/tts_client.h` + `src/npc/tts_client.cpp`): HTTP client for Piper TTS on Tower:8001 with health-check, PSRAM WAV buffer, and SD card fallback. Voice: tom-medium.
- **NPC Phrase Bank** (`game/scenarios/npc_phrases.yaml`): All Professor Zacus lines in French, organized by category: hints (3 levels × 6 scenes), congratulations, warnings, personality comments (by mood), adaptation phrases (skip/challenge/timer), narrative bridges, false leads, and ambiance (intro/outro/idle).
- **NPC MP3 Pool Generator** (`tools/tts/generate_npc_pool.py`): Python tool that reads `npc_phrases.yaml`, calls Piper TTS API for each phrase, writes MP3 files to `hotline_tts/`, and generates `hotline_tts/manifest.json`. Idempotent (skips already-generated files). Run: `python3 tools/tts/generate_npc_pool.py [--dry-run]`.

### AI Integration
- 6 AI agent definitions in `.github/agents/` (voice, tts, vision, hints, audio_gen, mcp)
- Hints engine: anti-cheat, 3 difficulty levels, per-puzzle context
- Spec: `specs/AI_INTEGRATION_SPEC.md`

## Canonical Files

| File | Role |
|------|------|
| `game/scenarios/zacus_v2.yaml` | Scenario source of truth (v3, Runtime 3) |
| `game/scenarios/npc_phrases.yaml` | Professor Zacus NPC phrase bank (all categories, French) |
| `tools/tts/generate_npc_pool.py` | NPC MP3 pool generator (Piper TTS → hotline_tts/) |
| `specs/ZACUS_RUNTIME_3_SPEC.md` | Runtime contract definition |
| `specs/AI_INTEGRATION_SPEC.md` | AI layer architecture |
| `Makefile` | Main automation entry point |
| `docs/QUICKSTART.md` | Getting started |
| `docs/DEPLOYMENT_RUNBOOK.md` | Field deployment |
| `docs/debt/codex-firmware-fixes-to-apply.md` | Pending firmware fixes (I2S, CORS, AP password) |

## Language & Communication

- User speaks **French**, code and docs in **English**
- Respond in French for conversation, English for code/comments/commits

## Infrastructure

- **Tower** (`clems@192.168.0.120`): Piper TTS FR on port 8001
- **KXKM-AI** (`kxkm@kxkm-ai`): RTX 4090, GPU inference
- SSH is key-based only, never use sshpass
