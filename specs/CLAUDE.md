# Specs

Contracts between firmware, frontends, tooling, and AI agents. Treat each spec as a versioned interface — a breaking change here triggers work in 2+ subsystems.

## Canonical Specs

| Spec | Governs |
|------|---------|
| `ZACUS_RUNTIME_3_SPEC.md` | Scenario IR + execution semantics |
| `AI_INTEGRATION_SPEC.md` | Agent layer (hints, voice, vision, TTS, audio_gen, MCP) |
| `FIRMWARE_WEB_DATA_CONTRACT.md` | ESP32 ↔ frontend HTTP/WebSocket payloads |
| `MCP_HARDWARE_SERVER_SPEC.md` | MCP stdio tools (6) for hardware interaction |
| `MEDIA_MANAGER_*` | Media manager (firmware/frontend/runtime/sync) |
| `STORY_DESIGNER_SCRATCH_LIKE_SPEC.md` | Frontend Blockly-like authoring |
| `STORY_RUNTIME_API_JSON_CONTRACT.md` | Runtime API JSON shape |
| `LOCAL_AI_STUDIO_SPEC.md` | Local AI studio integration |
| `QA_TEST_MATRIX_SPEC.md` | QA matrix + acceptance criteria |

## Editing Rules

- A spec change is a contract change. List affected consumers in the PR description.
- Bump the version header at the top of the file when altering existing fields; never silently rename keys.
- New spec docs must be linked from root `CLAUDE.md` "Canonical Files" if they introduce a new contract.
- Pair runtime/frontend spec changes — the matching `tools/scenario/compile_runtime3.py` and `frontend-scratch-v2/src/lib/runtime3.ts` updates ship in the same PR.

## Anti-Patterns

- "Implementation detail" comments in specs — if it's not part of the contract, remove it
- Adding fields without specifying optionality, defaults, and version
- Letting the IR drift between Python compiler and TS runtime without updating the spec first
- Documenting hardware pins/ports here — they belong in `ESP32_ZACUS/` firmware sources
