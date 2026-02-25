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
3. Lancer les validateurs projet (non-régression canon/audio/printables) :
   - `python3 tools/scenario/validate_scenario.py game/scenarios/zacus_v1.yaml`
   - `python3 tools/scenario/export_md.py game/scenarios/zacus_v1.yaml`
   - `python3 tools/audio/validate_manifest.py audio/manifests/zacus_v1_audio.yaml`
   - `python3 tools/printables/validate_manifest.py printables/manifests/zacus_v1_printables.yaml`

## Notes
- Ce bundle n'est pas promu automatiquement dans `game/scenarios/*.yaml`.
- Toute promotion doit passer par une revue fonctionnelle dédiée.
Files:
- scenario_promptable_template.yaml : your editable template (prompt_input)
- scenario_canonical.yaml          : canonical YAML (copy into docs/... target)
- scenario_runtime.json            : runtime JSON (copy into data/story/scenarios/DEFAULT.json)
- fsm_mermaid.md                   : FSM diagram
- CODEX_PROMPT.md                  : ready-to-paste Codex prompt
- zacus_v2.yaml                    : narrative scenario aligned with runtime + gameplay reality
- CONVERSATION_SUMMARY.md          : what changed / key decisions

Typical flow:
1) Update prompt_input in scenario_promptable_template.yaml if needed.
2) Use canonical/runtime files to update repo targets.
3) Use zacus_v2.yaml as the printed narrative / MJ reference for the real game.

Generated: 2026-02-25
