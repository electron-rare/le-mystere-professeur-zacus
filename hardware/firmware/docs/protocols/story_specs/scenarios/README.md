# Story V2 Scenarios

Reference collection of Story V2 scenario YAML files.

## Canonical Reference

- **`example_story_n_codepack.yaml`** ⭐ **[START HERE]**
  - Reference implementation using Story_n_CodePackApp pattern (5 steps: unlock → proto → wait → etape2 → done)
  - Demonstrates all event types (unlock, audio_done, timer, serial, key)
  - Shows LA behavior (60s timeout, >3s cumul hold)
  - AI-generatable skeleton for future stories
  - **Use as template for new scenario creation**

## Existing Scenarios

- `default_unlock_win_etape2.yaml` — Default flow scenario
- `example_unlock_express.yaml` — Quick unlock + victory variant
- `example_unlock_express_done.yaml` — Minimal done state scenario
- `spectre_radio_lab.yaml` — Spectre Radio Lab story
- `zacus_v1_unlock_and_etape2.yaml` — Zacus V1 compatibility scenario

## How to Create a New Scenario

1. **Copy** `example_story_n_codepack.yaml` and rename (e.g., `my_new_story.yaml`)
2. **Update** scenario_id, description, and step IDs to match your story
3. **Configure** Story_n_CodePackApp instances with your logic (hold_ms, timeout_ms, key sequences, etc.)
4. **Bind** screen_scene_id, audio_pack_id, and custom app_bindings to your resources
5. **Validate** with: `python3 tools/story_gen/story_gen.py validate docs/protocols/story_specs/scenarios/my_new_story.yaml`
6. **Generate** C++: `python3 tools/story_gen/story_gen.py generate docs/protocols/story_specs/scenarios/my_new_story.yaml`

## Scenario Structure

Each scenario includes:
- **scenario_id** (string) — Unique identifier
- **version** (string) — Semantic version
- **description** (string) — Human-readable summary
- **initial_step** (string) — Entry point step ID
- **steps** (array) — Step definitions with resources and transitions
  - Each step can bind Story_n_CodePackApp instances
  - Each step can have multiple event-driven transitions

## Key Constraints

- **Max 100 steps** per scenario
- **Max 5 transitions** per step
- **Max 1 audio pack** per step
- Event types: `unlock`, `audio_done`, `timer`, `serial`, `key`

## Story_n_CodePackApp Pattern

See [example_story_n_codepack.yaml](./example_story_n_codepack.yaml) for complete documentation on:
- LA Detector behavior (60s timeout, >3s hold)
- Key sequence alternatives
- Serial command support (FORCE_STEP, SKIP, FORCE_UNLOCK)
- Extensibility model for AI-generated apps

---

**Questions?** See [docs/protocols/story_specs/README.md](../README.md)
