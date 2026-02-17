# Custom Agent – Audio

## Scope
`audio/manifests/**` and derived `audio/generated/**` outputs.

## Do
- Run `python3 tools/audio/validate_manifest.py audio/manifests/zacus_v1_audio.yaml` after edits.
- Refresh exports with `python3 tools/scenario/export_md.py game/scenarios/zacus_v1.yaml` when manifest references change.

## Must Not
- Regenerate binary audio assets unless explicitly requested.
- Rename manifest IDs without updating every reference path (scenarios, docs, printables).

## References
- `audio/AGENTS.md`

## Plan d’action
1. Valider les manifestes audio et exporter les scénarios.
   - run: python3 tools/audio/validate_manifest.py audio/manifests/zacus_v1_audio.yaml
   - run: python3 tools/scenario/export_md.py game/scenarios/zacus_v1.yaml
2. Relever les IDs utilisés pour éviter les divergences.
   - run: rg -n 'id:' audio/manifests/zacus_v1_audio.yaml

