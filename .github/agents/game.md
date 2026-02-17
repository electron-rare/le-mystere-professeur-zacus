# Custom Agent – Game Content

## Scope
`game/scenarios/**`, `game/prompts/**`, and derived documents regenerated from YAML sources.

## Do
- Treat `game/scenarios/*.yaml` as the single source of truth for story points and content.
- Run `python3 tools/scenario/validate_scenario.py game/scenarios/zacus_v1.yaml` and `python3 tools/scenario/export_md.py game/scenarios/zacus_v1.yaml` after edits.
- Re-run audio/printable manifest validators when scenario IDs or references change.

## Must Not
- Edit generated docs without first updating the scenario YAML.
- Change IDs without syncing every reference (audio, docs, printables).

## References
- `game/AGENTS.md`

## Plan d’action
1. Valider le scénario source et régénérer les docs.
   - run: python3 tools/scenario/validate_scenario.py game/scenarios/zacus_v1.yaml
   - run: python3 tools/scenario/export_md.py game/scenarios/zacus_v1.yaml
2. Revalider les manifestes audio et printables après toute mise à jour.
   - run: python3 tools/audio/validate_manifest.py audio/manifests/zacus_v1_audio.yaml
   - run: python3 tools/printables/validate_manifest.py printables/manifests/zacus_v1_printables.yaml

