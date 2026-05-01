# Game Content

Source-of-truth for scenarios, NPC dialogue, and prompts. Edits here propagate to firmware, frontends, and TTS pools.

## Canonical Files

| File | Role |
|------|------|
| `scenarios/zacus_v2.yaml` | Master scenario (Runtime 3 IR source) |
| `scenarios/npc_phrases.yaml` | All Professor Zacus lines (FR), categorised |
| `prompts/` | LLM prompt templates for hints/coherence |

## Editing Rules

- IDs are foreign keys: changing `puzzle.id` or `zone.id` breaks audio + printables manifests, firmware bundles, and frontend tests. Sync cross-file when renaming.
- Never edit `exports/` or generated MD — regenerate via `tools/scenario/export_md.py`.
- French dialogue only in `npc_phrases.yaml`; full diacritics required.
- New NPC phrase categories must be added to schema validator and `tools/tts/generate_npc_pool.py`.

## Validate Before Commit

```bash
python3 tools/scenario/validate_scenario.py scenarios/zacus_v2.yaml
python3 tools/scenario/compile_runtime3.py scenarios/zacus_v2.yaml
python3 tools/audio/validate_manifest.py ../audio/manifests/zacus_v2_audio.yaml
python3 tools/printables/validate_manifest.py ../printables/manifests/zacus_v2_printables.yaml
```

## Anti-Patterns

- Editing generated outputs in `exports/` instead of YAML source
- Renaming IDs without grepping the repo for cross-references
- Adding NPC phrases without regenerating the MP3 pool
- Mixing French narrative with English code identifiers in YAML values
