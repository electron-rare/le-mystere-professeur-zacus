# Story Authoring Prompts (Canonical Location)

Ce dossier centralise les **prompts d'authoring Story V2** — aides à la génération/conception de scénarios.

## Contenu

- `spectre_radio_lab.prompt.md` — Exemple de scénario "laboratoire radio" avec progression audio claire

## Usage

Ces prompts sont destinés au **story authoring** (création de scénarios YAML). Ils ne sont **pas** des prompts ops/debug.

Cependant, ils peuvent être utilisés par les outils **Codex** (si nécessaire) pour assister la création de scénarios.

## Redirect Note

Les emplacements antérieurs:
- `story_generator/story_specs/prompts/spectre_radio_lab.prompt.md` ← **DEPRECATED**, voir ci-dessus
- `docs/protocols/spectre_radio_lab.prompt.md` ← **DEPRECATED**, voir ci-dessus

Tous les prompts auteurs Story doivent être maintenus **ici** seulement.

## Pour Ajouter un Prompt

1. Crée `<mon_scenario>.prompt.md` dans ce dossier
2. Inclue sections **Intent**, **Contraintes techniques**, **Config LA**, **Ressources visées**, **Flux attendu**
3. Référence le prompt dans la doc authoring: `docs/protocols/GENERER_UN_SCENARIO_STORY_V2.md`
