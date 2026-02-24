# API Coordination Agent — Freenove ⇄ RTC_BL_PHONE

Contexte opérationnel:
- Carte locale: Freenove `ui_freenove_allinone` (ESP32-S3).
- Carte pair: `RTC_BL_PHONE` (branche API cible: `esp32_RTC_ZACUS`).
  - Référence: https://github.com/electron-rare/RTC_BL_PHONE/tree/esp32_RTC_ZACUS

Règle transport:
- ESP-NOW est **toujours en broadcast** (`ff:ff:ff:ff:ff:ff`) côté Freenove.
- Aucune configuration peer unicast n’est attendue pour l’envoi applicatif.

Contrat canonical:
- `docs/ESP_NOW_API_CONTRACT_FREENOVE_V1.md`
- `docs/ESP_NOW_API_CONTRACT_FREENOVE_V1_QUICK.md`
- `docs/ESP_NOW_API_CONTRACT_FREENOVE_V1_MINI_SCHEMA.md`

Contrôle d’exécution:
- Les commandes importantes attendues côté RTC: `RING`, `SCENE <scenario_id>`, `UNLOCK`, `NEXT`, états et actions générales.
- Extension debug Freenove acceptée: `SCENE_GOTO <scene_id>` (one-shot, scene courante).
- Réponses attendues côté Freenove via `ack` si `ack=true` est demandé dans la trame.

Checklist de divergence:
- Quand un changement est fait côté RTC, valider d’abord la compatibilité sur ces docs.
- En cas de doute sur mapping commande/réponse, corriger d’abord le contrat avant le firmware.
