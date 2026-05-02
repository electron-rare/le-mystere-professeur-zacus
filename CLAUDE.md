# CLAUDE.md

Guidance for Claude Code working in this repository.

## Overview

**Le Mystère du Professeur Zacus** — hybrid educational escape room game with ESP32-S3 hardware, React authoring studios, and a portable Runtime 3 scenario engine. Scenarios are authored in YAML, compiled to a portable IR, and executed on both web and ESP32 targets.

## Build & Test

```bash
make all-validate                                # Full pipeline (scenario + audio + printables + runtime3)
make content-checks                              # Schema validation only

# Runtime 3
make runtime3-compile                            # YAML → IR
make runtime3-simulate                           # Execute IR
make runtime3-verify                             # Verify pivots
make runtime3-test                               # Python unittest

# Compile a specific scenario
python3 tools/scenario/compile_runtime3.py game/scenarios/zacus_v2.yaml

# Dev CLI (12 actions)
./tools/dev/zacus.sh content-checks
./tools/dev/zacus.sh voice-bridge start|stop|status|test
```

Frontend, desktop, and tooling commands live in their nested CLAUDE.md.

## Architecture

```
YAML scenario → compile_runtime3.py → Runtime 3 IR → ESP32 / Web player
                                                    ↓
                                              Voice bridge → Piper TTS (Tower:8001)
                                                    ↓
                                              Hints engine → /hints/ask
```

Key surfaces:
- **Scenario IR**: `game/scenarios/zacus_v2.yaml` → `tools/scenario/compile_runtime3.py` → portable Runtime 3 IR. Contract: `specs/ZACUS_RUNTIME_3_SPEC.md`.
- **Authoring**: `frontend-v3/` (pnpm monorepo, dashboard + editor + simulation; editor + simulation will fuse into `apps/atelier/` per `docs/superpowers/specs/2026-05-01-v3-fusion-atelier-design.md`).
- **Firmware**: `ESP32_ZACUS/` submodule (separate repo, separate CI). Freenove ESP32-S3 + PlatformIO. NPC engine, voice pipeline, vision/QR, media manager.
- **Voice / NPC**: Piper TTS on Tower:8001 (zacus voice = tom-medium). NPC phrases in `game/scenarios/npc_phrases.yaml`. MP3 pool generator: `tools/tts/generate_npc_pool.py`.
- **MCP hardware**: `tools/dev/mcp_hardware_server.py` (stdio, 6 tools).
- **Desktop hub**: `desktop/` (Electron, macOS, bundles V3 frontends, talks USB serial).

## Where to Look

| Task | Location |
|------|----------|
| Edit scenarios, NPC phrases, prompts | `game/` |
| Modify Runtime 3 compiler / validators / TTS pool / dev CLI | `tools/` |
| Authoring UI / dashboard / simulation | `frontend-v3/` |
| Zacus Studio macOS app | `desktop/` |
| Add or change a contract spec | `specs/` |
| Python tests (Runtime 3, NPC) | `tests/` |
| Firmware code | `ESP32_ZACUS/` submodule (own repo) |

## Canonical Files

| File | Role |
|------|------|
| `game/scenarios/zacus_v2.yaml` | Scenario source of truth |
| `game/scenarios/npc_phrases.yaml` | NPC phrase bank (FR) |
| `specs/ZACUS_RUNTIME_3_SPEC.md` | Runtime contract |
| `specs/AI_INTEGRATION_SPEC.md` | AI layer architecture |
| `Makefile` | Top-level automation |
| `docs/QUICKSTART.md` | Getting started |
| `docs/DEPLOYMENT_RUNBOOK.md` | Field deployment |
| `docs/debt/codex-firmware-fixes-to-apply.md` | Pending firmware fixes |

## Infrastructure

- **Tower** (`clems@192.168.0.120`): Piper TTS FR on `:8001` (voices: tom-medium / siwis / upmc).
- **KXKM-AI** (`kxkm@kxkm-ai` via Tailscale): RTX 4090, GPU inference.
- SSH is key-based only — never `sshpass`.

## Language

User speaks French → respond in French. Code, comments, commits, docs → English. Full diacritics required in French.

## Nested Guidance

Domain-specific rules live in nested `CLAUDE.md` files and load automatically when you read files in those directories. Closest file wins. Current nested:

- `game/`, `tools/`, `tests/`, `specs/`
- `frontend-v3/`, `desktop/`
