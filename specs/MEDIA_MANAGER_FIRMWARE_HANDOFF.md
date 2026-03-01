# Handoff - Équipe Firmware (Media Manager)

## Contexte
- Source de vérité: `hardware/firmware/data/story/scenarios/DEFAULT.json`
- FSM de travail: `scenario-ai-coherence/zacus_conversation_bundle_v3/fsm_mermaid.md`
- Spécification générale: `specs/MEDIA_MANAGER_RUNTIME_SPEC.md`
- Scope: aucune modification frontend.

## Objectif métier
- Verrouiller la sortie finale vers Media Manager au niveau runtime.
- Rendre la fin de parcours reproductible: `SCENE_FINAL_WIN` -> `STEP_MEDIA_MANAGER`.
- Documenter l’impact réel du lock NVS (`kLockNvsMediaManagerMode`).

## Cibles de sortie (artefacts)
- `artifacts/runtime-sync/<date>/media-manager-fw.md`
- `artifacts/runtime-sync/<date>/media-manager-fw-serial.txt`
- `artifacts/runtime-sync/<date>/media-manager-fw-gate.md`
- mise à jour éventuelle de runbook/tickets si changement nécessaire.

## Actions obligatoires
1. Extraire la liste des transitions de fin depuis le runtime et confirmer:
  - `SCENE_FINAL_WIN` a bien les transitions `WIN_DUE`, `BTN_NEXT`, `UNLOCK`, `FORCE_WIN_ETAPE2`.
  - les cibles sont normalisées sur `STEP_MEDIA_MANAGER`.
2. Vérifier le step terminal `STEP_MEDIA_MANAGER`:
  - `mp3_gate_open == true`
  - `transitions` vide
  - actions: `ACTION_TRACE_STEP`, `ACTION_SET_BOOT_MEDIA_MANAGER`
3. Vérifier `/api/status` et `/api/media/*` côté firmware:
  - champ `story.screen == SCENE_MEDIA_MANAGER` quand terminal atteint
  - snapshot `media` complet (`ready`, `playing`, `recording`, `record_simulated`, `last_error`).
4. Vérifier la persistance de boot mode:
  - lire `zacus_boot` namespace
  - `startup_mode` et `media_validated`
  - comportement réel de `ACTION_SET_BOOT_MEDIA_MANAGER` avec `kLockNvsMediaManagerMode`.

## Commandes/points de vérification (sans exécution ici, seulement trace attendue)
- `/api/status` après transition finale
- `BOOT_MODE_STATUS` via `/api/control`
- `BOOT_MODE_SET MEDIA_MANAGER`
- `BOOT_MODE_CLEAR`
- `BOOT_MODE_SET STORY`

## Critères d’acceptation
- Confirmation serial/logs de passage final vers `STEP_MEDIA_MANAGER`.
- Confirmation que `STEP_MEDIA_MANAGER` reste un point terminal sans transition sortante.
- Rapport explicite si `kLockNvsMediaManagerMode` bloque la persistance.
- Aucun impact sur les chemins autre que la fin de scénario.

## Remarques équipe
- Ne pas modifier la firmware pour cette passe.
- Tout écart observé est reporté comme ticket/risque, pas patché ici.
