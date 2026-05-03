# Tools

Python tooling for scenario compilation, validation, voice pipeline, MCP hardware, and dev workflow.

## Layout

| Subdir | Purpose |
|--------|---------|
| `scenario/` | Runtime 3 compiler, validator, simulator, pivot verifier, MD/firmware exporters |
| `audio/` | Audio manifest validator |
| `printables/` | Printables manifest validator |
| `tts/` | NPC phrase pool generator (Piper TTS or voice-bridge F5 → `hotline_tts/`) |
| `stt/` | Speech-to-text helpers |
| `dev/` | TUI dashboard, MCP hardware server, `zacus.sh` CLI (12 actions) |
| `playtest/` | Playtest harness |
| `setup/`, `repo_state/`, `requirements/`, `images/`, `test/` | Auxiliary |

## Conventions

- Every script accepts `--help` (argparse). Run it before invoking with new args.
- Idempotent by default — generators (e.g. `tts/generate_npc_pool.py`) skip already-produced artefacts. Add `--force` flags rather than breaking idempotency.
- Scripts run from repo root; use repo-relative paths via `pathlib.Path(__file__).resolve().parents[N]`.
- All scenario tools share `scenario/runtime3_common.py` for IR types — extend it, don't redefine types per script.

## TTS Pool Backends (`tts/generate_npc_pool.py`)

Two backends, both idempotent (skip existing files; pass `--force` to rebuild):

- **`--backend piper`** (default, legacy): hits Tower :8001 OpenAI-compatible API, MP3 output → `hotline_tts/<key>.mp3`. Manifest: `hotline_tts/manifest.json`.
- **`--backend f5`** (P1 part9b): hits voice-bridge `/tts` on MacStudio, WAV output → `hotline_tts/f5/<key>.wav`. Manifest: `hotline_tts/f5/manifest.json` carries `cache_key`, `server_backend`, `server_cache_hit`, `server_latency_ms` per entry. Cache key derivation must mirror `tools/macstudio/voice-bridge/main.py::_voice_ref_token` — hash the `voice_ref` string verbatim (or sentinel `"default"` when omitted), never the file content.

## Validate Changes

```bash
python3 -m py_compile <changed_files>
python3 tools/scenario/<changed_script>.py --help
make all-validate                          # if scenario tools changed
python3 -m unittest discover -s tests      # if NPC/runtime touched
```

## Anti-Patterns

- Hardcoding machine-specific paths/ports (Tower IP `192.168.0.120` is acceptable but should be overridable via env)
- Requiring chat interaction when a local prompt/script wait suffices
- Diverging IR shape between `compile_runtime3.py` and `runtime3_common.py`
- Adding new validators without wiring them into `Makefile` `all-validate`
- Writing to `game/scenarios/exports/` directly — only `export_md.py` does that
