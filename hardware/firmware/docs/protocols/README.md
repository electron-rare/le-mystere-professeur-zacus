# Protocoles d’échange firmware

Ce dossier centralise les contrats partagés entre firmwares (ESP32, RP2040, ESP8266).

## Source of truth
- `../../protocol/ui_link_v2.md`: protocole UART UI Link v2 (ESP32 <-> OLED/TFT).
- `PROTOCOL.md`: commandes série canoniques ESP32 (debug/ops).
- `UI_SPEC.md`: comportement UI et contrat fonctionnel.
- `story_specs/schema/story_spec_v1.yaml` + templates/scenarios: contrat auteur STORY.
- `story_specs/prompts/*.prompt.md`: prompts d'autoring Story (distincts des prompts ops).
- `STORY_V2_PIPELINE.md`: pipeline YAML -> gen -> runtime -> apps -> screen.
- `RELEASE_STORY_V2.md`: guide release Story V2 (gates/rollback).

## Règles d’évolution
- Toute évolution de protocole passe d'abord par ce dossier.
- Politique additive par défaut:
  - ajouter des champs/tokens est autorisé,
  - supprimer/renommer exige une dépréciation documentée.
- Les implémentations firmware doivent référencer ces documents dans leur README.

## Vérification minimale à chaque changement
1. Build des environnements concernés (`pio run -e <env>`).
2. Vérification de traçabilité commande/spec/code.
3. Mise à jour de `INDEX.md` si ajout ou déplacement de fichiers.
