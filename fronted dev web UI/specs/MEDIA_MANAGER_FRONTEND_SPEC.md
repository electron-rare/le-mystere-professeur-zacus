# Spécification Frontend — Media Manager (Mode final)

## Contexte
- Source de vérité runtime: `hardware/firmware/data/story/scenarios/DEFAULT.json`
- FSM de travail: `scenario-ai-coherence/zacus_conversation_bundle_v3/fsm_mermaid.md`
- Handoff frontend: `fronted dev web UI/specs/MEDIA_MANAGER_FRONTEND_HANDOFF.md`
- Contrat media runtime: `specs/MEDIA_MANAGER_RUNTIME_SPEC.md`

Ce document formalise **ce que le frontend doit consommer** pour être robuste quand le scénario arrive sur la phase finale media manager.

## 1) Objectifs
1. Détecter de façon fiable l’entrée Media Manager malgré la mixité `STEP_*`/`SCENE_*`.
2. Consommer les endpoints médias sans hypothèses sur l’extension de fichier.
3. Exposer des erreurs exploitables (`ok=false`, `error`) sans crash UI.
4. Gérer le cas de lock NVS via l’affichage de statut de boot mode.

## 2) Détection d’écran / état actif
### 2.1 Règle de détection (priorité)
Le frontend doit considérer que le Media Manager est actif si l’une des conditions est vraie:
1. `story.screen === "SCENE_MEDIA_MANAGER"`
2. `story.step === "STEP_MEDIA_MANAGER"`
3. fallback legacy: payload status expose un champ équivalent `step`/`scene`/`currentStep` contenant `MEDIA_MANAGER`.

### 2.2 Tolérance IDs mixtes
- Ne jamais supposer que `step` commence par `STEP_`.
- Ne jamais supposer que `screen` commence par `SCENE_`.
- Conserver les deux champs en mémoire pour les logs d’audit.

## 3) Modèle de données utilisé par le frontend
### 3.1 Polling / stream
- Le frontend peut continuer avec la mécanique actuelle (stream en live si dispo, sinon polling).
- Le payload de référence doit contenir au minimum:
  - `story.scenario`
  - `story.step`
  - `story.screen`
  - `media.ready`
  - `media.playing`
  - `media.recording`
  - `media.record_simulated`
  - `media.last_error`
  - `media.record_limit_seconds`
  - `media.record_elapsed_seconds`

### 3.2 Priorité d’affichage
1. `story.screen` prend le pas sur `story.step` pour l’UI.
2. `media.last_error` doit être prioritaire sur le message générique du formulaire d’action.

## 4) Contrat API (front) à implémenter côté UI
### 4.1 Endpoints requis
- `GET /api/media/files?kind=music|picture|recorder`
  - réponse attendue 200: `{ ok: true, kind: "...", files: ["/music/a.mp3"] }`
  - réponse 400 sur kind invalide: `{ ok: false, kind: "...", error: "invalid_kind" }`
- `POST /api/media/play` body:
  - `{ "path": "/music/file.mp3" }` ou `{ "file": "file.mp3" }`
- `POST /api/media/stop`
- `POST /api/media/record/start` body:
  - `{ "seconds": 20, "filename": "take_1.wav" }`
- `POST /api/media/record/stop`
- `GET /api/media/record/status` (ou récupération de `media` dans `/api/status`)
- `POST /api/control` fallback pour compatibilité opérationnelle.

### 4.2 Format de succès/erreur UI
- Traiter tout appel média comme opération tri-state:
  - succès: `{ ok: true, action: "...", ... }`
  - erreur: `{ ok: false, error: "...", action: "..." }`
- En cas d’erreur, afficher immédiatement `media.last_error` + statut de commande.

## 5) Comportements de l’écran Media Hub
### 5.1 Listing
- Charger **les 3 catégories** `music`, `picture`, `recorder` en parallèle ou séquentiel.
- Afficher une section par catégorie quand disponible.
- Aucun filtrage d’extension dur codé côté UI.

### 5.2 Lecture
- Le clic sur un item déclenche `MEDIA_PLAY`.
- Interdire un replay simultané si `media.playing === true` sans `stop` explicite préalable.
- Le changement d’état doit refléter `media.playing` de l’API.

### 5.3 Enregistrement
- Bouton d’enregistrement -> `MEDIA_RECORD_START`.
- Respecter le `media.record_limit_seconds` du runtime.
- Afficher `record_simulated` pour expliciter la nature simulée/placeholder du capture path.

### 5.4 Erreurs
- Si `media.last_error` est non vide: montrer un encadré d’erreur persistante avec timestamp et action recommandée.
- Aucun toast silencieux.

## 6) Cas spéciaux
### 6.1 Lock NVS / boot mode
- Le backend peut avoir `kLockNvsMediaManagerMode` actif.
- Si lock actif:
  - afficher l’état de lock (ou trace de refus) sans bloquer la navigation media actuelle.
  - proposer des boutons de fallback (`BOOT_MODE_STATUS`, `BOOT_MODE_SET MEDIA_MANAGER`, `BOOT_MODE_CLEAR`) selon disponibilité.

### 6.2 Legacy compatibility
- Si l’API legacy remonte uniquement `current_step`, dériver `story.step`.
- Si `media` absent, désactiver actions médias et afficher `status incomplet`.

## 7) Acceptance frontend (conforme)
1. Arrivée en fin de scénario -> `SCENE_MEDIA_MANAGER` affichée et hub actif.
2. `STEP_MEDIA_MANAGER` reconnu en fallback.
3. `/api/media/files` retourne des listes sans parsing d’extension.
4. Play/stop alterne `media.playing` sans erreur persistante.
5. Record start/stop met à jour `media.recording` et respecte la limite temporelle.
6. Une erreur `kind` invalide renvoie bien `ok=false` et erreur visible.

## 8) Tests à préparer (non-code)
- **Contract mock**: payload `story.screen: SCENE_MEDIA_MANAGER` avec step mixte.
- **Contract mock**: payload sans `screen` mais `step: STEP_MEDIA_MANAGER`.
- **API mock**: `kind=video` -> 400.
- **UI mock**: replay play en cascade (double clic) sans erreur de crash.
- **UI mock**: `record_simulated=true` et `media.last_error` rempli.

## 9) Artefacts attendus par équipe
- `artifacts/runtime-sync/<date>/media-manager-front-checklist.md`
- `artifacts/runtime-sync/<date>/media-manager-front-mapping.md`

## 10) Sortie et limites
- Implémentation réalisée dans la refonte frontend SvelteKit (consommation UI seulement).
- Toute divergence détectée (`SCENE_MEDIA_MANAGER` restant sur une transition runtime attendue) est remontée via `SCN-601-MEDIA-BRIDGE`.
