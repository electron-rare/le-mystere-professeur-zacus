# Protocoles d’échange firmware

Ce dossier centralise les contrats partagés entre firmwares (ESP32, RP2040, ESP8266).

## Source of truth
- `PROTOCOL.md`: protocole UART JSONL UI <-> ESP32 + commandes série canoniques.
- `UI_SPEC.md`: comportement UI et contrat des messages.
- `story_spec_v1.yaml` + templates/scénarios: contrat auteur STORY.

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
