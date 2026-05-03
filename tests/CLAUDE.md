# Tests

Python unittest suite covering Runtime 3 IR, NPC engine integration, and scenario invariants.

## Layout

| File / Dir | Scope |
|------------|-------|
| `test_npc_engine.py` | NPC state machine (triggers, mood, rule firing) |
| `test_npc_integration.py` | NPC ↔ TTS ↔ phrase bank wiring |
| `test_npc_phrases_schema.py` | `npc_phrases.yaml` schema invariants |
| `runtime3/test_runtime3_routes.py` | Runtime 3 compiler/IR/pivot tests |
| `runtime3/test_firmware_bundle.py` | E2E pipeline: scenario YAML → IR → firmware bundle JSON (subprocess) |

## Running

```bash
python3 -m unittest discover -s tests           # full suite
python3 -m unittest tests.test_npc_engine       # single module
make runtime3-test                              # via Makefile target
```

## Patterns

- Use `unittest.TestCase`, not pytest fixtures (suite is stdlib-only).
- Load YAML fixtures from `game/scenarios/`, never inline them — tests must catch drift in real content.
- For NPC trigger timing tests, inject a fake clock; never rely on `time.sleep`.
- Snapshot the compiled Runtime 3 IR in `runtime3/` tests — diff failures should point at the offending pivot/zone ID.

## Anti-Patterns

- Mocking `compile_runtime3.py` output instead of running it on real fixtures
- Tests that pass when YAML IDs are renamed but downstream consumers break
- Importing from `tools/` without going through public functions
- Suppressing UnicodeDecodeError on French content — fix the encoding instead
