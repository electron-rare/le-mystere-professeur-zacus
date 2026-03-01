# Media Manager – Pack par équipe

## Référentiel source
- Runtime authoritative: `hardware/firmware/data/story/scenarios/DEFAULT.json`
- FSM de coordination: `scenario-ai-coherence/zacus_conversation_bundle_v3/fsm_mermaid.md`

## Pack Equipe Firmware
- Spécification de travail: `specs/MEDIA_MANAGER_RUNTIME_SPEC.md`
- Handoff opérationnel firmware: `specs/MEDIA_MANAGER_FIRMWARE_HANDOFF.md`
- Runbook sync (section FW): `specs/MEDIA_MANAGER_SYNC_RUNBOOK.md`

## Pack Equipe Front-end
- Handoff opérationnel frontend: `fronted dev web UI/specs/MEDIA_MANAGER_FRONTEND_HANDOFF.md`
- Spécification détaillée frontend: `fronted dev web UI/specs/MEDIA_MANAGER_FRONTEND_SPEC.md`
- Runbook sync (section WEB): `specs/MEDIA_MANAGER_SYNC_RUNBOOK.md`
- Contrat runtime API global: `specs/FIRMWARE_WEB_DATA_CONTRACT.md`

## Point de cohérence à vérifier
1. Fin de scénario: `STEP_FINAL_WIN` -> entrée Media Manager (via `SCENE_FINAL_WIN`/`STEP_MEDIA_MANAGER` selon runtime)
2. Affichage media: baser la détection sur `story.screen == SCENE_MEDIA_MANAGER` avec fallback `story.step == STEP_MEDIA_MANAGER`
3. API: utiliser `/api/media/*` et vérifier les erreurs `ok=false`
4. Boot mode: conserver et tester la contrainte `kLockNvsMediaManagerMode` en documentation d'acceptation

## Format de livraison attendue
- Chaque équipe dépose ses notes dans `artifacts/runtime-sync/<date>/` selon `specs/MEDIA_MANAGER_SYNC_RUNBOOK.md`
- Aucun changement firmware/frontend dans cette passe
