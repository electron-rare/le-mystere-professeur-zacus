# Spec - Contrat de donnees Runtime Firmware <-> Web

## Statut
- Etat: draft de reference pour agents/devs
- Date: 2026-03-01
- Decision cle: pour le runtime executable, **le firmware est la source de verite**.

## 1) Objectif
Definir un contrat unique pour integrer et partager les donnees runtime entre:
- equipe firmware
- equipe frontend web
- agents de generation scenario

Ce document couvre uniquement le **runtime Story V2** et sa diffusion vers les artefacts de coherence.

## 2) Source de verite
Source runtime canonique:
- `hardware/firmware/data/story/scenarios/DEFAULT.json`

Artefacts derives (non source):
- `scenario-ai-coherence/zacus_conversation_bundle_v3/scenario_runtime.json`
- `scenario-ai-coherence/zacus_conversation_bundle_v3/fsm_mermaid.md`
- docs de briefing basees runtime

Regle:
1. Toute evolution de transitions, `step_id`, `event_type`, `initial_step`, actions, apps se fait d'abord dans le runtime firmware.
2. Les artefacts de coherence sont regeneres/updates depuis le runtime firmware, jamais l'inverse.

## 3) Constat actuel (ecart a resorber)
Etat firmware (`DEFAULT.json`):
- `initial_step = RTC_ESP_ETAPE1`
- `steps = 11`
- contient notamment: `SCENE_CREDIT`, `STEP_MEDIA_MANAGER`, IDs mixtes `SCENE_*` + `STEP_*`

Etat bundle conversationnel (`scenario_runtime.json`):
- `initial_step = STEP_U_SON_PROTO`
- `steps_runtime_order = 9`
- nomenclature majoritairement `STEP_*`

Decision d'integration:
- l'ecart se corrige en alignant le bundle sur le firmware, pas en modifiant le firmware a partir du bundle.

## 4) Contrat de donnees minimal

### 4.1 Scenario root (runtime firmware)
Champs obligatoires:
- `id` (string)
- `version` (number)
- `initial_step` (string)
- `app_bindings` (array d'objets)
- `steps` (array d'objets)

### 4.2 Step
Champs obligatoires:
- `step_id`
- `screen_scene_id`
- `audio_pack_id` (peut etre vide)
- `actions` (array)
- `apps` (array)
- `transitions` (array)

### 4.3 Transition
Champs obligatoires:
- `id`
- `trigger`
- `event_type`
- `event_name`
- `target_step_id`
- `priority`
- `after_ms`

Champs optionnels:
- `debug_only`

## 5) Vocabulaire evenementiel a respecter
Types supportes runtime:
- `button`
- `serial`
- `timer`
- `audio_done`
- `unlock`
- `espnow`
- `action`

Regles:
1. Pas de nouveau `event_type` sans spec + validation croisee firmware/web.
2. `event_name` est contractuel (ex: `ACK_WIN1`, `UNLOCK_QR`, `FORCE_DONE`).
3. `target_step_id` doit toujours referencer un step existant.

## 6) Contrat d'integration Web
Le frontend ne doit pas deduire le runtime depuis des docs narratifs.
Il consomme:
- runtime status (`/api/story/*` en mode Story V2)
- fallback legacy (`/api/status`, `/api/scenario/*`) si necessaire

Le frontend doit:
1. afficher `scenario_id`, `current_step`, et statut run sans assumptions sur prefixes `STEP_`/`SCENE_`.
2. accepter des graphes avec steps > 9 et IDs heterogenes.
3. ne pas hardcoder l'`initial_step`.

## 7) Flux de travail agents/devs

### 7.1 Firmware agent
1. Modifier runtime firmware (`DEFAULT.json`).
2. Verifier coherence locale des transitions.
3. Publier diff runtime + note de migration des IDs/evenements.

### 7.2 Scenario/Content agent
1. Lire runtime firmware.
2. Repercuter dans `scenario_runtime.json` (format bundle).
3. Mettre a jour la vue FSM mermaid et notes de coherence.

### 7.3 Web agent
1. Verifier que l'UI lit les steps reels exposes par l'API.
2. Valider que les controles (`next`, `unlock`, etc.) restent compatibles.
3. Ajouter/mettre a jour tests e2e sur les transitions modifiees.

## 8) Checklists de validation

### 8.1 Gate coherence runtime
- le nombre de steps du bundle correspond au firmware
- `initial_step` bundle = `initial_step` firmware
- tous les `target_step_id` du bundle existent
- aucun `event_type` hors contrat

### 8.2 Gate web
- lint/build/tests unitaires OK
- e2e mock: navigation et controles story
- e2e live: lecture statut + commandes de base

## 9) RACI rapide
- Firmware team: owner du runtime executable
- Web team: owner de la consommation UI/API
- Agents scenario: owner de la synchronisation des artefacts derives

## 10) Livrable attendu pour la prochaine passe
- Bundle conversationnel synchronise sur `DEFAULT.json` firmware
- FSM mermaid regeneree depuis ce meme runtime
- note de migration listant les differences de `step_id` et `initial_step`
