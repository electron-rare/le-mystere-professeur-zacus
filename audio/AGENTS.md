# Audio Agent Contract

Purpose: manage audio manifests, generated indexes, and validation gates.

Allowed scope:
- `audio/manifests/**`
- `audio/generated/**` (derived outputs only after source changes)

Validate:
- `python3 tools/audio/validate_manifest.py audio/manifests/zacus_v1_audio.yaml`
- `rg -n "id:|file:" audio/manifests`

Common commands:
- `rg --files audio`
- `python3 tools/scenario/export_md.py game/scenarios/zacus_v1.yaml`

Do not:
- regenerate binary audio assets unless explicitly requested
- rename manifest IDs without updating all references
