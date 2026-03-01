# Handoff - Équipe Front-end (Media Manager)

## Contexte
- FSM de travail: `scenario-ai-coherence/zacus_conversation_bundle_v3/fsm_mermaid.md`
- Contrat media manager: `specs/MEDIA_MANAGER_RUNTIME_SPEC.md`
- Scope: aucune modification firmware.

## Objectif métier
- Exposer le Media Manager de manière fiable à l’arrivée en fin de scénario.
- Accepter les identifiants step/scene mixtes sans casser l’UI.
- S’appuyer sur les APIs media existantes pour listage, lecture et enregistrement.

## Cibles de sortie (artefacts)
- `artifacts/runtime-sync/<date>/media-manager-web-checks.md`
- `artifacts/runtime-sync/<date>/media-manager-front-mapping.md`
- notes de migration/ajustement de parsing si nécessaire.
- Spec détaillée d'intégration: `fronted dev web UI/specs/MEDIA_MANAGER_FRONTEND_SPEC.md`

## Actions obligatoires
1. Parsing statut:
  - lire `story.screen` en priorité.
  - fallback tolérant sur `story.step == STEP_MEDIA_MANAGER`.
  - ne pas supposer `step_id` prefixé `STEP_` ou `SCENE_`.
2. Mapping Media Hub:
  - activer vue hub quand `story.screen == SCENE_MEDIA_MANAGER`.
  - si non dispo, fallback `story.step == STEP_MEDIA_MANAGER`.
3. Endpoints media:
  - `/api/media/files` (`kind` music/picture/recorder)
  - `/api/media/play`, `/api/media/stop`
  - `/api/media/record/start`, `/api/media/record/stop`
  - `/api/media/record/status`
4. Gestion erreurs:
  - gérer `ok=false` et `error` depuis `/api/control` et endpoints media.
  - afficher `media.last_error` en debug écran media.
5. Affichage media:
  - afficher clairement `media.record_simulated` pour éviter les ambiguïtés d’enregistrement réel.

## Critères d’acceptation
- Le Media Hub s’affiche à la fin de scénario sans crash.
- `SCENE_MEDIA_MANAGER` déclenche bien la vue média même avec `STEP_MEDIA_MANAGER` possible en fallback.
- `media.playing` bascule correctement via play/stop.
- Les erreurs API sont présentées de façon exploitable par l’opérateur.

## Remarques équipe
- Ne pas changer les contrats runtime, seulement adapter la consommation UI.
- Toute divergence métier (ex: cible `SCENE_MEDIA_MANAGER` encore présente) doit être reportée via ticket "scenario".
- Implémentation front alignée à la refonte SvelteKit (routes dédiées + store média centralisé).
