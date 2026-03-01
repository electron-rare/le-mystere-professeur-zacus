# Plan d'execution - Runtime Firmware vers Bundle et Web

## Statut
- Etat: draft executable
- Date: 2026-03-01
- Source de verite runtime: `hardware/firmware/data/story/scenarios/DEFAULT.json`

## Objectif
Coordonner les agents et developpeurs pour:
1. synchroniser les artefacts conversationnels sur le runtime firmware,
2. garantir que le frontend consomme les donnees runtime sans hypothese fragile,
3. stabiliser les gates de validation cross-team.

## Contraintes
- Ne pas utiliser `scenario-ai-coherence/*` comme source runtime.
- Ne pas inferrer la logique de transitions depuis des docs narratifs.
- Les IDs de step (`STEP_*`, `SCENE_*`, mix) sont contractuels tels qu'exposes par le firmware.

## Ordonnancement
1. `FW-001` -> 2. `SCN-101` -> 3. `SCN-102` -> 4. `WEB-201` -> 5. `WEB-202` -> 6. `QA-301`

## Tickets

### FW-001 - Baseline runtime firmware
- Owner: Firmware dev
- Inputs: `hardware/firmware/data/story/scenarios/DEFAULT.json`
- Travail:
1. Confirmer `id`, `version`, `initial_step`, liste des `step_id`.
2. Exporter un snapshot runtime horodate dans `artifacts/runtime-sync/<date>/`.
3. Publier un changelog court des transitions (`event_type`, `event_name`, `target_step_id`).
- Outputs:
1. `artifacts/runtime-sync/<date>/DEFAULT.runtime.snapshot.json`
2. `artifacts/runtime-sync/<date>/transition-index.md`
- Definition of done:
1. Snapshot valide JSON.
2. Liste complete des steps et transitions.
3. Diff partage a l'equipe scenario et web.

### SCN-101 - Synchronisation du bundle conversationnel
- Owner: Scenario/Content agent
- Inputs:
1. Snapshot `FW-001`
2. `scenario-ai-coherence/zacus_conversation_bundle_v3/scenario_runtime.json`
- Travail:
1. Aligner `scenario.id`, `version`, `initial_step`.
2. Aligner l'ordre et le nombre de steps sur le runtime firmware.
3. Conserver les meta narratives hors runtime (boot policy, led policy, etc.) si non contradictoires.
- Outputs:
1. `scenario-ai-coherence/zacus_conversation_bundle_v3/scenario_runtime.json` aligne firmware
2. note de migration `artifacts/runtime-sync/<date>/bundle-sync-notes.md`
- Definition of done:
1. Aucun step runtime orphelin.
2. Aucun `target` vers step inexistant.
3. Validation runtime bundle executee.

### SCN-102 - Regeneration coherence visuelle/documentaire
- Owner: Scenario/Content agent
- Inputs:
1. Runtime bundle aligne (`SCN-101`)
2. FSM source `scenario-ai-coherence/zacus_conversation_bundle_v3/fsm_mermaid.md`
- Travail:
1. Regenerer la FSM mermaid depuis les transitions runtime.
2. Mettre a jour les sections de docs impactees par `initial_step`/nombre de steps.
3. Documenter les differences majeures versus version precedente.
- Outputs:
1. `scenario-ai-coherence/zacus_conversation_bundle_v3/fsm_mermaid.md`
2. `artifacts/runtime-sync/<date>/runtime-delta.md`
- Definition of done:
1. 1 noeud mermaid par step runtime.
2. 1 edge mermaid par transition runtime.
3. Relecture croisee firmware/scenario signee.

### WEB-201 - Alignement contrat API runtime
- Owner: Web dev
- Inputs:
1. Runtime firmware baseline (`FW-001`)
2. contrat API `specs/STORY_RUNTIME_API_JSON_CONTRACT.md`
- Travail:
1. Verifier que le parser frontend accepte les IDs mixtes.
2. Retirer toute hypothese de prefixe (`STEP_` only).
3. Aligner les actions de controle sur les routes effectivement exposees.
- Outputs:
1. patch frontend d'alignement API (si necessaire)
2. note `artifacts/runtime-sync/<date>/web-api-alignment.md`
- Definition of done:
1. UI affiche `scenario_id` et `current_step` reels.
2. Aucun crash parser sur steps heterogenes.
3. Actions `next/unlock/network` fonctionnelles selon flavor.

### WEB-202 - Tests de contrat
- Owner: Web dev + QA
- Inputs:
1. contrats de payload Story V2/Legacy
2. fixtures runtime issues de `FW-001`
- Travail:
1. Ajouter fixtures JSON conformes et non conformes.
2. Ajouter tests unitaires parser/status/list.
3. Ajouter tests e2e minimum sur transitions critiques.
- Outputs:
1. fixtures `tests/fixtures/runtime-contract/*`
2. tests unitaires/e2e de contrat
- Definition of done:
1. cas nominal + cas erreur couverts.
2. build/lint/tests front OK.
3. rapport de couverture de contrat.

### QA-301 - Gate integration cross-team
- Owner: QA gatekeeper
- Inputs:
1. livrables `FW-001`, `SCN-101`, `SCN-102`, `WEB-201`, `WEB-202`
- Travail:
1. Executer gates scenario/audio/printables.
2. Executer gates frontend (lint/build/unit/e2e).
3. Valider coherence runtime firmware <-> bundle.
- Outputs:
1. `artifacts/runtime-sync/<date>/qa-report.md`
- Definition of done:
1. toutes gates vertes ou ecarts documentes + action plan.
2. decision GO/NOGO explicite.

## Gates minimales a passer
- `python3 tools/scenario/validate_scenario.py game/scenarios/zacus_v2.yaml`
- `python3 tools/scenario/export_md.py game/scenarios/zacus_v2.yaml`
- `python3 tools/audio/validate_manifest.py audio/manifests/zacus_v2_audio.yaml`
- `python3 tools/printables/validate_manifest.py printables/manifests/zacus_v2_printables.yaml`
- `npm --prefix 'fronted dev web UI' run lint`
- `npm --prefix 'fronted dev web UI' run build`
- `npm --prefix 'fronted dev web UI' run test:unit -- --run`

## Regle de handoff entre agents
Chaque ticket passe seulement avec ces evidences:
1. diff des fichiers modifies,
2. commande(s) executee(s) + verdict,
3. limites connues et impacts.
