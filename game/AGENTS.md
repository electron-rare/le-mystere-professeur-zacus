# Game Agent Contract

Purpose: scenario and story content source-of-truth management.

Allowed scope:
- `game/scenarios/**`
- `game/prompts/**`
- generated docs derived from scenario YAML

Validate:
- `python3 tools/scenario/validate_scenario.py game/scenarios/zacus_v1.yaml`
- `python3 tools/scenario/export_md.py game/scenarios/zacus_v1.yaml`
- `python3 tools/audio/validate_manifest.py audio/manifests/zacus_v1_audio.yaml`
- `python3 tools/printables/validate_manifest.py printables/manifests/zacus_v1_printables.yaml`

Common commands:
- `rg -n "id:|station|solution" game/scenarios`
- `rg --files game`

Do not:
- edit generated outputs without updating scenario YAML first
- change IDs without syncing cross-file references
