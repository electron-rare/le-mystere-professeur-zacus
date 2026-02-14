# Protocoles d’échange firmware

Ce dossier centralise les spécifications de protocoles, formats de messages, et interfaces partagées entre les firmwares (ESP32, RP2040, etc.).

## Contenu proposé
- ui_protocol.md : description du protocole UART entre UI et ESP32
- audio_protocol.md : format des commandes audio
- story_protocol.md : format des scénarios et commandes story

## Règles
- Toute évolution de protocole doit être documentée ici.
- Les fichiers .h de protocoles doivent pointer vers ce dossier dans leur header.

---

*Compléter ce dossier avec les specs existantes et à venir.*
