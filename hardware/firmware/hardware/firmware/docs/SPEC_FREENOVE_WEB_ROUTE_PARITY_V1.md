# Spécification Orchestrateur Zacus — Freenove Web Route Parity (v1)

Date: 2026-02-20  
Repo: `electron-rare/le-mystere-professeur-zacus`  
Issue liée: `#94`

## 1. Contexte

La cible Freenove expose une WebUI embarquée et plusieurs endpoints `/api/...` définis dans:
- `hardware/firmware/hardware/firmware/ui_freenove_allinone/src/main.cpp`

L'alignement RTC/Zacus a été réalisé en PR #92, mais la prévention de dérive frontend/backend n'est pas encore formalisée par un gate.

## 2. Objectif

Définir un contrat de parité route frontend/backend pour Freenove afin de:
1. éviter les appels WebUI vers des routes inexistantes,
2. documenter explicitement la coexistence routes legacy et routes `/api/network/*`,
3. produire une preuve de validation reproductible.

## 3. Périmètre

In-scope:
- extraction statique des routes frontend appelées dans la WebUI embarquée,
- extraction statique des routes backend exposées (`g_web_server.on(...)`),
- comparaison et gate de non-régression,
- publication d'un rapport d'évidence.

Out-of-scope:
- migration complète de l'UI vers un autre framework,
- changement du protocole ESP-NOW.

## 4. Routes de référence (baseline v1)

Routes frontend legacy présentes dans la WebUI embarquée:
- `/api/status`
- `/api/scenario/unlock`
- `/api/scenario/next`
- `/api/wifi/connect`
- `/api/wifi/disconnect`
- `/api/espnow/send`

Routes backend exposées en plus (alias/network API):
- `/api/network/wifi`
- `/api/network/wifi/connect`
- `/api/network/wifi/disconnect`
- `/api/network/espnow`
- `/api/network/espnow/send`
- `/api/network/espnow/peer` (GET/POST/DELETE)
- `/api/control`

## 5. Exigences fonctionnelles

### EZ-01 — Checker parity Freenove
- Un script de parity DOIT comparer routes frontend et backend.
- Le gate DOIT échouer si une route frontend n'existe pas côté backend.

### EZ-02 — Support routes alias
- Le checker DOIT accepter la coexistence legacy + `/api/network/*`.
- Le rapport DOIT lister explicitement les routes backend non utilisées par la WebUI.

### EZ-03 — Preuve de validation
- Le run parity DOIT produire un rapport JSON ou texte versionnable dans l'evidence pack firmware.

### EZ-04 — Cohérence cross-repo
- La nomenclature d'actions route/controller DOIT rester compatible avec RTC:
  - routes réseau: `/api/network/*`
  - dispatch: `/api/control`

## 6. Exigences non fonctionnelles

- Exécution du checker < 5 secondes sur arbre firmware local.
- Aucun accès réseau externe.
- Message d'erreur actionnable (méthode + route manquante).

## 7. Critères d'acceptation (DoD)

- [ ] Checker parity implémenté et documenté.
- [ ] Un run de preuve est archivé dans les artefacts.
- [ ] La doc firmware référence ce checker.
- [ ] Issue `#94` mise à jour avec preuve de run.
- [ ] Référence RTC `RTC_BL_PHONE#11` ajoutée dans le suivi.

## 8. Coordination

- Pattern source: `Kill_LIFE` PR #2.
- RTC companion issue (canonique): `RTC_BL_PHONE#11`.
- Contrat ESP-NOW inchangé: `cmd/raw/command/action` + `event/message/payload`.
