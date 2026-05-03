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
                                              Voice bridge :8200 (MacStudio)
                                                ↳ /tts → F5-TTS-MLX (cache → F5 → safe wav)
                                                ↳ /voice/intent → LiteLLM npc-fast (MLX 7B Q4)
                                                ↳ /voice/transcribe → whisper.cpp :8300
                                                    ↓
                                              Hints engine → /hints/ask (LLM rewrite via hints-deep MLX 32B Q4)
```

Key surfaces:
- **Scenario IR**: `game/scenarios/zacus_v2.yaml` → `tools/scenario/compile_runtime3.py` → portable Runtime 3 IR. Contract: `specs/ZACUS_RUNTIME_3_SPEC.md`.
- **Authoring**: `frontend-v3/` (pnpm monorepo: `apps/atelier/` Scratch-like studio + `apps/dashboard/` game-master live view).
- **Firmware**:
  - `ESP32_ZACUS/` submodule — Freenove ESP32-S3 master (NPC engine, voice pipeline, vision/QR, media manager). Branche `feat/idf-migration` en cours (port Arduino → ESP-IDF 5.4).
  - `PLIP_FIRMWARE/` — retro telephone annex (ES8388 dev kit bringup, Si3210 PCB target). REST endpoint consumed by the master ESP32.
- **Voice / NPC**: F5-TTS-MLX in-process sur MacStudio (`studio:8200`), voix Zacus clonée via `~/zacus_reference.wav`. Pré-rendu via `tools/tts/generate_npc_pool.py --backend f5`. Service down WAV hardcoded en graceful degradation. NPC phrases : `game/scenarios/npc_phrases.yaml`.
- **Hints engine**: `tools/hints/server.py` FastAPI (LLM rewrite via LiteLLM `hints-deep`, anti-cheat, adaptive level, safety filter `game/scenarios/hints_safety.yaml`).
- **Inference stack**: voir `tools/macstudio/README.md` — MLX-LM (npc + hints), whisper.cpp, F5-TTS, LiteLLM proxy `:4000`. Configs versionnées dans `tools/macstudio/`.
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
| Firmware code (Zacus master) | `ESP32_ZACUS/` submodule (own repo) |
| Firmware code (PLIP retro phone) | `PLIP_FIRMWARE/` (inlined; see README for submodule conversion) |

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

- **MacStudio** (`ssh studio`, M3 Ultra 512 GB) : **stack inférence Zacus complète** — MLX-LM (`npc-fast` 7B Q4 :8501, `hints-deep` 32B Q4 :8500), whisper.cpp `large-v3-turbo-q5_0` :8300, F5-TTS-MLX in-process via voice-bridge :8200 (cache disque + watchdog `*/2` + `service_down.wav` graceful), LiteLLM proxy :4000. Voir `tools/macstudio/README.md`.
- **Tower** (`clems@192.168.0.120`) : Suite Numérique, n8n, LiteLLM Tower (legacy). **Plus dans la chaîne live Zacus** ; container Docker `zacus-tts-piper` survit pour batch historique uniquement (voix EN-only, FR à déployer si besoin).
- **KXKM-AI** (`kxkm@kxkm-ai` via Tailscale) : RTX 4090, GPU inference (fine-tuning, plus utilisé en runtime).
- SSH is key-based only — never `sshpass`.

## Language

User speaks French → respond in French. Code, comments, commits, docs → English. Full diacritics required in French.

## Nested Guidance

Domain-specific rules live in nested `CLAUDE.md` files and load automatically when you read files in those directories. Closest file wins. Current nested:

- `game/`, `tools/`, `tests/`, `specs/`
- `frontend-v3/`, `desktop/`
