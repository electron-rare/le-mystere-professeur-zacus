# Guidelines for contributors

- **Single source of truth**: `game/scenarios/*.yaml` describe every canonical story point (stations, puzzles, solution, ids). Update the YAML first, then regenerate derived materials.
- **Read-only firmware**: `hardware/firmware/esp32/` is maintained elsewhere; do not edit files, docs, or logs there. Treat it as immutable for this project.
- **Standard workflow**:
  1. Edit `game/scenarios/zacus_v1.yaml` (or add a new scenario file).
  2. Run `python3 tools/scenario/validate_scenario.py <scenario>`.
  3. Run `python3 tools/scenario/export_md.py <scenario>` to regenerate `_generated` outputs.
  4. Run `python3 tools/audio/validate_manifest.py audio/manifests/zacus_v1_audio.yaml`.
  5. Run `python3 tools/printables/validate_manifest.py printables/manifests/zacus_v1_printables.yaml`.
  6. Optionally use `make` targets: `make scenario-validate`, `make export`, `make audio-validate`, `make printables-validate`, `make all-validate`.
- **Dependencies**: install PyYAML (`pip install pyyaml` or via a virtual environment such as `python3 -m venv .venv`); each script will error with a reminder if PyYAML is missing.
