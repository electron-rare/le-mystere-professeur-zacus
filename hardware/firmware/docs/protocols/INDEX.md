# Index des protocoles et specs

Ce dossier est la référence unique des interfaces partagées firmware.

## Index

| Fichier | Statut | Description |
|---|---|---|
| `PROTOCOL.md` | Source of truth | Commandes série canoniques ESP32 (debug/ops) |
| `UI_SPEC.md` | Source of truth | Spécification UI tactile RP2040 (pages, gestes, mode dégradé) |
| `story_spec_v1.yaml` | Source of truth | Schéma YAML des scénarios STORY |
| `scenario.template.yaml` | Modèle | Template de scénario STORY |
| `spectre_radio_lab.prompt.md` | Exemple | Prompt de génération STORY |
| `spectre_radio_lab.yaml` | Exemple | Scénario STORY d'exemple |
| `GENERER_UN_SCENARIO_STORY_V2.md` | Guide | Process auteur STORY |
| `story_README.md` | Guide | Usage général specs/scénarios STORY |
| `README.md` | Guide | Règles d'évolution et validation |

## Règle
- Toute divergence entre code et docs doit être corrigée dans ce dossier au même cycle de livraison.
- Le protocole UART v2 partage est maintenu dans `../../protocol/ui_link_v2.md`.
