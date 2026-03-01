# Zacus Conversation Bundle (v3)

## Purpose
Bundle de travail conversationnel/IA conservé en dehors de la source canon gameplay (`game/scenarios/*.yaml`).

## Files
- `scenario_promptable_template.yaml`: template éditable (input prompt)
- `scenario_canonical.yaml`: YAML canonique conversation bundle
- `scenario_runtime.json`: JSON runtime dérivé
- `fsm_mermaid.md`: diagramme FSM
- `CODEX_PROMPT.md`: prompt Codex prêt à l'emploi
- `zacus_v2.yaml`: narration MJ alignée bundle
- `CONVERSATION_SUMMARY.md`: synthèse des décisions

## Validation workflow
1. Installer les dépendances validateurs :
   - `bash tools/setup/install_validators.sh`
2. Valider la cohérence interne du bundle :
   - `python3 tools/scenario/validate_runtime_bundle.py`
3. Lancer les validateurs projet (non-régression canon/audio/printables en V2) :
   - `python3 tools/scenario/validate_scenario.py game/scenarios/zacus_v2.yaml`
   - `python3 tools/scenario/export_md.py game/scenarios/zacus_v2.yaml`
   - `python3 tools/audio/validate_manifest.py audio/manifests/zacus_v2_audio.yaml`
   - `python3 tools/printables/validate_manifest.py printables/manifests/zacus_v2_printables.yaml`

## Notes
- Ce bundle n'est pas promu automatiquement dans `game/scenarios/*.yaml`.
- Toute promotion doit passer par une revue fonctionnelle dédiée.
## Typical flow
1. Modifier `prompt_input` dans `scenario_promptable_template.yaml`.
2. Revalider la cohérence (`validate_runtime_bundle.py`) puis les validateurs projet V2.
3. Préparer la promotion firmware via un diff `scenario_runtime.json` -> `hardware/firmware/data/story/scenarios/DEFAULT.json` avant toute application.
