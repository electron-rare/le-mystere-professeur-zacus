# Workflows

1. **Mettre à jour le YAML canon** (`game/scenarios/zacus_v1.yaml` ou nouvel ID). Les stations/puzzles/solution sont la source des prompts audio et printables.
2. **Valider le scénario** avec `tools/scenario/validate_scenario.py` pour garantir structure, plages 6–14 enfants et `solution_unique`.
3. **Exporter les briefings Markdown** via `tools/scenario/export_md.py` pour mettre à jour `kit-maitre-du-jeu/_generated/` et `docs/_generated/SCENARIO_BRIEF.md`.
4. **Valider l’audio** avec `tools/audio/validate_manifest.py audio/manifests/zacus_v1_audio.yaml` afin que chaque piste corresponde à un fichier de `game/prompts/audio/`.
5. **Valider les printables** avec `tools/printables/validate_manifest.py printables/manifests/zacus_v1_printables.yaml` pour s’assurer que chaque ID a un prompt accessible.
