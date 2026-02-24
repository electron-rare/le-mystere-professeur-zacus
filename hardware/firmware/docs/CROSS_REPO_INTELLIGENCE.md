# Cross-Repo Intelligence (Freenove ↔ RTC_BL_PHONE)

## Principes

- Cette fiche sert de référence pour l’alignement protocolaire et validation croisée entre:
  - `electron-rare/le-mystere-professeur-zacus`
  - `electron-rare/RTC_BL_PHONE` (branche de travail locale: `esp32_RTC_ZACUS`)
- Référence locale RTC (lecture seule): repo local pointé par `RTC_BL_PHONE_REPO` (ou `RTC_REPO`).
- Aucun changement n’est écrit automatiquement dans le repo RTC tant qu’il n’est pas explicitement autorisé.

## Sources de vérité (référençage)

- Zacus:
  - `docs/ESP_NOW_API_CONTRACT_FREENOVE_V1.md`
  - `docs/ESP_NOW_API_CONTRACT_FREENOVE_V1_QUICK.md`
  - `docs/ESP_NOW_API_CONTRACT_FREENOVE_V1_MINI_SCHEMA.md`
  - `docs/ESP_NOW_API_COORDINATION_AGENT.md`
- RTC_BL_PHONE:
  - `docs/espnow_contract.md`
  - Implémentation runtime: `src/main.cpp`, `src/props/EspNowBridge.cpp`

## Contrat opérationnel commun (résumé)

- ESP-NOW transport côté Freenove: émission toujours en broadcast (`ff:ff:ff:ff:ff:ff`).
- Commandes attendues côté pair pour opérations story: `RING`, `SCENE <scenario_id>`, `UNLOCK`, `NEXT`, `SC_EVENT`.
- Extension Freenove locale: `SCENE_GOTO <scene_id>` (one-shot debug).
- `ESPNOW_SEND` côté Freenove accepte un format texte ou JSON, avec retour ACK si `ack=true`.
- Récepteur RTC tolère déjà les alias `cmd`, `command`, `action`, et les formats imbriqués.

## Contrôle de coordination

Avant toute évolution protocolaire, vérifier:

1. Compatibilité commande (liste et alias) dans `docs/espnow_contract.md` (RTC) et `docs/ESP_NOW_API_CONTRACT_FREENOVE_V1*.md` (Zacus).
2. Comportement d’envoi (broadcast/target) identique côté pair.
3. Gestion erreurs (`unsupported_command`, erreurs SCENE, statuts ack).
4. Mise à jour de la version documentaire correspondante dans les deux dépôts.
5. Validation en environnement hardware (build + smoke), puis log de résultat.

## Notes de suivi

- Si divergence détectée sur une commande ou un payload:
  - bloquer la livraison de la branche,
  - corriger d’abord la source documentaire commune,
  - valider ensuite le firmware.

### Check automatisé (prêt à lancer)

- Commande:
  - `python3 tools/dev/check_cross_repo_espnow_contract.py --rtc-repo "$RTC_BL_PHONE_REPO"`
- Sortie:
  - `PASS` si les jeux de commandes détectés sont alignés,
  - `DRIFT` + code `1` en cas d’écart,
  - `ERROR` + code `2` si un chemin/doc est inaccessible.
- Cette vérification est **lecture seule** sur le repo RTC tant que la sortie est validée avant commit.
