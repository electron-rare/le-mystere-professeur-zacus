# Runbook Synchronisation - Media Manager (FSM runtime DEFAULT)

## Objectif
Aligner la partie Media Manager entre:
- contrat runtime firmware (`hardware/firmware/data/story/scenarios/DEFAULT.json`),
- FSM de référence (`scenario-ai-coherence/zacus_conversation_bundle_v3/fsm_mermaid.md`),
- artefacts scenario (`scenario-ai-coherence/zacus_conversation_bundle_v3/scenario_runtime.json`).

## Règle de vérité
- La source exécutive est uniquement le firmware (`hardware/firmware/data/story/scenarios/DEFAULT.json`).
- Toute adaptation d'équipe scenario et frontend doit être un reflet contractuel de ce runtime.
- Aucun edit firmware/frontend pour cette passe.

## 1) Cible runtime Media Manager (à verrouiller)
- Sortie de fin: depuis `SCENE_FINAL_WIN`, transitions possibles vers `STEP_MEDIA_MANAGER` sur:
  - `timer:WIN_DUE`
  - `serial:BTN_NEXT`
  - `unlock:UNLOCK`
  - `serial:FORCE_WIN_ETAPE2`
- Step terminal: `STEP_MEDIA_MANAGER` avec écran `SCENE_MEDIA_MANAGER`, `mp3_gate_open: true`, actions `ACTION_TRACE_STEP` + `ACTION_SET_BOOT_MEDIA_MANAGER`, aucune transition sortante.

## 2) Tâche FW (rapport de validation)
Nom ticket: `FW-401-MEDIA-ENDING`
- Extraire un extrait `serial` depuis startup et transition vers media manager.
- Vérifier le comportement de `ACTION_SET_BOOT_MEDIA_MANAGER` avec `kLockNvsMediaManagerMode=true`.
- Confirmer la persistance NVS:
  - `zacus_boot.startup_mode`
  - `zacus_boot.media_validated`
- Déposer dans `artifacts/runtime-sync/<date>/media-manager-fw.md`.

DoD:
- preuve de ciblage runtime (`STEP_MEDIA_MANAGER`) dans les logs,
- preuve d'appel API `BOOT_MODE_STATUS`/`BOOT_MODE_SET` sur le firmware,
- preuve que le blocage NVS est connu et documenté si actif.

## 3) Tâche Scenario (bundle/FSM)
Nom ticket: `SCN-601-MEDIA-BRIDGE`
- Rejouer la map `SCENE_MEDIA_MANAGER <-> STEP_MEDIA_MANAGER` entre FSM et runtime.
- Garder la semantique des triggers du FSM final:
  - `SCENE_FINAL_WIN -> SCENE_MEDIA_MANAGER` pour `timer:WIN_DUE`, `serial:BTN_NEXT`, `unlock:UNLOCK`, `action:FORCE_WIN_ETAPE2`, `serial:FORCE_WIN_ETAPE2`.
- Vérifier que tout `SCENE_MEDIA_MANAGER` en sortie bundle est mappé sur `STEP_MEDIA_MANAGER` dans artifact runtime.
- Générer `artifacts/runtime-sync/<date>/media-manager-bundle-delta.md` avec:
  - transitions normalisées,
  - écarts détectés (ex: `FORCE_WIN_ETAPE2` présent, cible différente).

DoD:
- `scenario_runtime.json` cohérent avec `DEFAULT.json` au point de terminaison Media Manager,
- `scenario_runtime.json` ne contient pas de step/runtime orphelins liés au Media Manager.

## 4) Tâche Web (contract)
Nom ticket: `WEB-602-MEDIA-STATUS`
- Consommer `story.screen` prioritairement pour détecter le Media Manager.
- Tolérer `STEP_MEDIA_MANAGER` ou `SCENE_MEDIA_MANAGER` en fallback d'identifiant.
- Utiliser uniquement endpoints:
  - `/api/status`
  - `/api/media/files`
  - `/api/media/play`
  - `/api/media/stop`
  - `/api/media/record/start`
  - `/api/media/record/stop`
  - `/api/media/record/status`
  - `/api/media/record/status` et `media.last_error`.
- Ne pas parser d'extension à la main pour filtrage initial.
- Documenter la limitation lock NVS dans la vue Media Manager (`media_validated` + lock actuel).

DoD:
- mode d'affichage media hub activé si `story.screen == SCENE_MEDIA_MANAGER`,
- aucun crash si step id mixte `SCENE_*` / `STEP_*`,
- erreurs `/api/media/*` exposées via `ok=false` ou `/api/control` error.

## 5) QA gate
Nom ticket: `QA-703-MEDIA-GATE`
- Vérification runtime:
  - statut `/api/status.media` conforme (fields `ready`, `playing`, `recording`, `record_limit_seconds`, `record_simulated`).
  - `/api/media/files?kind=music|picture|recorder` retourne `ok=true`.
  - `kind=video` retourne 400 + `invalid_kind`.
  - `/api/media/play` + `/api/media/stop` font évoluer `media.playing`.
- Vérification transition finale:
  - preuve `SCENE_FINAL_WIN` -> terminal media dans logs/runtime.
- Vérification contrat:
  - la doc `specs/MEDIA_MANAGER_RUNTIME_SPEC.md` reste cohérente avec les sorties observées.

Sortie:
- `artifacts/runtime-sync/<date>/media-manager-qa-report.md` (pass/fail par test, commandes API, captures).

## 6) Ordre recommandé
`FW-401-MEDIA-ENDING` -> `SCN-601-MEDIA-BRIDGE` -> `WEB-602-MEDIA-STATUS` -> `QA-703-MEDIA-GATE`.

## 7) Risques connus
- `kLockNvsMediaManagerMode=true` empêche réellement la persistance boot media manager.
- `ACTION_SET_BOOT_MEDIA_MANAGER` peut être bloquée en runtime actif lock (comportement normal selon lock).
- Si le frontend lit uniquement `story.step`, il faut ajouter tolérance explicite sur `story.screen`.

## 8) Format micro-ticket opérationnel

### FW-401-MEDIA-ENDING
- Owner: firmware lead
- But: valider la vérité runtime de fin de parcours media manager.
- Inputs:
  - `hardware/firmware/data/story/scenarios/DEFAULT.json`
  - logs serial de transition
- Actions:
  - confirmer la cible runtime de `SCENE_FINAL_WIN`: `STEP_MEDIA_MANAGER`
  - confirmer que `STEP_MEDIA_MANAGER` est terminal
  - vérifier le comportement `ACTION_SET_BOOT_MEDIA_MANAGER` et l’effet du lock NVS
- Outputs:
  - `artifacts/runtime-sync/<date>/media-manager-fw.md`
  - extrait logs du passage final (`SCENE_FINAL_WIN` -> `STEP_MEDIA_MANAGER`)
- DoD:
  - preuve claire que le runtime expose le terminal media manager sur `STEP_MEDIA_MANAGER`
  - preuve de blocage ou de succès du boot mode selon `kLockNvsMediaManagerMode`
  - no behavior change en dehors du périmètre media manager

### SCN-601-MEDIA-BRIDGE
- Owner: scenario/content owner
- But: normaliser le bridge bundle/FSM vers runtime.
- Inputs:
  - `scenario-ai-coherence/zacus_conversation_bundle_v3/fsm_mermaid.md`
  - `scenario-ai-coherence/zacus_conversation_bundle_v3/scenario_runtime.json`
- Actions:
  - appliquer la règle `SCENE_MEDIA_MANAGER -> STEP_MEDIA_MANAGER` pour toutes les sorties bundle pertinentes
  - retirer/normaliser toute référence orpheline au media manager
  - aligner les déclencheurs finaux selon la FSM (liste et ordre de fallback)
- Outputs:
  - `artifacts/runtime-sync/<date>/media-manager-bundle-delta.md`
  - `scenario-ai-coherence/zacus_conversation_bundle_v3/scenario_runtime.json` (si révision demandée)
- DoD:
  - `steps_runtime_order` cohérent avec runtime
  - aucune transition finale invalide ou cible inconnue liée au media manager
  - changement documenté `SCENE_MEDIA_MANAGER` -> `STEP_MEDIA_MANAGER` si présent

### WEB-602-MEDIA-STATUS
- Owner: web lead
- But: rendre l’affichage media manager robuste à la mixité `STEP_*` / `SCENE_*`.
- Inputs:
  - `specs/MEDIA_MANAGER_RUNTIME_SPEC.md`
  - endpoint `/api/status`, `/api/media/*`
- Actions:
  - afficher media hub si `story.screen == SCENE_MEDIA_MANAGER`
  - fallback `story.step == STEP_MEDIA_MANAGER`
  - intégrer `media.last_error` et `media.record_simulated` dans l’UI
  - gérer les erreurs `/api/media/*` via schéma standard `ok=false`
- Outputs:
  - `artifacts/runtime-sync/<date>/media-manager-web-checks.md`
  - notes de cas limites (lock NVS, transitions non terminales)
- DoD:
  - transition affichée vers hub sans hypothese de préfixe step
  - aucun crash sur events de fin de scénario
  - erreurs API correctement exposées

### QA-703-MEDIA-GATE
- Owner: QA
- But: valider la chaîne entière sur firmware + bundle + UI media.
- Inputs:
  - artefacts FW/SCN/WEB ci-dessus
  - requêtes manuelles sur `/api/status`, `/api/media/files`, `/api/media/play`, `/api/media/stop`, `/api/media/record/start`, `/api/media/record/stop`
- Actions:
  - exécuter checks du tableau de la section 5 dans l’environnement réel
  - valider cohérence de `story.screen` et `media` dans `/api/status`
  - valider que le verrou NVS est bien reporté et non ambigu pour l’opérationnel
- Outputs:
  - `artifacts/runtime-sync/<date>/media-manager-qa-report.md`
  - verdict GO/NOGO + écarts mineurs/majeurs
- DoD:
  - tous les cas de test de statut et API passés
  - passage des conditions critiques (final win, fichiers médias, play/stop, record, transition hub)
  - release gate claire avec actions de correction ciblées si échec
