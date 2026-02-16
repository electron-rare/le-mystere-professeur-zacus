# Index des protocoles et specs

Ce dossier est la référence unique des interfaces partagées firmware.

## Index

| Fichier | Statut | Description |
|---|---|---|
| `PROTOCOL.md` | Source of truth | Commandes série canoniques ESP32 (debug/ops) |
| `UI_SPEC.md` | Source of truth | Spécification UI tactile RP2040 (pages, gestes, mode dégradé, scenes Story) |
| `story_specs/schema/story_spec_v1.yaml` | Source of truth | Schéma YAML des scénarios STORY |
| `story_specs/templates/scenario.template.yaml` | Modèle | Template de scénario STORY |
| `story_specs/prompts/spectre_radio_lab.prompt.md` | Exemple | Prompt de génération STORY |
| `story_specs/scenarios/spectre_radio_lab.yaml` | Exemple | Scénario STORY d'exemple |
| `GENERER_UN_SCENARIO_STORY_V2.md` | Guide | Process auteur STORY |
| `story_README.md` | Guide | Usage général specs/scénarios STORY |
| `STORY_V2_PIPELINE.md` | Guide | Pipeline YAML -> gen -> runtime -> apps -> screen |
| `RELEASE_STORY_V2.md` | Guide | Release Story V2 (gates, rollback, checks) |
| `story_specs/README.md` | Guide | Organisation des specs STORY |
| `README.md` | Guide | Règles d'évolution et validation |

## Règle
- Toute divergence entre code et docs doit être corrigée dans ce dossier au même cycle de livraison.
- Le protocole UART v2 partage est maintenu dans `../../protocol/ui_link_v2.md`.
