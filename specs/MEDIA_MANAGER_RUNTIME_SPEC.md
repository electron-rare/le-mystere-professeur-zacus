# Spec - Media Manager (Scenario Zacus FSM DEFAULT)

## Statut
- Etat: draft executable
- Date: 2026-03-01
- FSM de travail: `scenario-ai-coherence/zacus_conversation_bundle_v3/fsm_mermaid.md`
- Source runtime executable: `hardware/firmware/data/story/scenarios/DEFAULT.json`
- Source story points: `game/scenarios/default_unlock_win_etape2.yaml` et `game/scenarios/zacus_v2.yaml`

## 1) Cadrage
Cette spec definit le contrat de la partie Media Manager pour:
- firmware (runtime + boot mode),
- frontend web (API et etats UI),
- agents scenario (sync bundle/FSM -> runtime firmware).

Contraintes:
- firmware = source de verite executable,
- aucun changement firmware/frontend dans cette passe,
- les ecarts sont captures comme exigences de sync.

## 2) Identifiants contractuels
- Step runtime terminal: `STEP_MEDIA_MANAGER`
- Scene UI associee: `SCENE_MEDIA_MANAGER`
- Action de post-victoire: `ACTION_SET_BOOT_MEDIA_MANAGER`

Regle de normalisation:
1. Dans les artefacts conversationnels/FSM, un target `SCENE_MEDIA_MANAGER` est accepte.
2. Avant deployer vers runtime firmware, ce target doit etre normalise vers `STEP_MEDIA_MANAGER`.
3. Cote UI, l'affichage peut rester base sur `story.screen == SCENE_MEDIA_MANAGER`.

## 3) Entree Media Manager depuis la fin de partie
### 3.1 Scenario retenu (FSM bundle choisi)
Depuis `STEP_FINAL_WIN`:
- `timer:WIN_DUE -> SCENE_MEDIA_MANAGER`
- `serial:BTN_NEXT -> SCENE_MEDIA_MANAGER`
- `unlock:UNLOCK -> SCENE_MEDIA_MANAGER`
- `action:FORCE_WIN_ETAPE2 -> SCENE_MEDIA_MANAGER`
- `serial:FORCE_WIN_ETAPE2 -> SCENE_MEDIA_MANAGER`

### 3.2 Runtime firmware actuel
Depuis `SCENE_FINAL_WIN`:
- `timer:WIN_DUE -> STEP_MEDIA_MANAGER`
- `serial:BTN_NEXT -> STEP_MEDIA_MANAGER`
- `unlock:UNLOCK -> STEP_MEDIA_MANAGER`
- `serial:FORCE_WIN_ETAPE2 -> STEP_MEDIA_MANAGER` (debug)

### 3.3 Exigence de sync
- L equipe scenario maintient la semantique FSM choisie.
- L equipe firmware maintient le target runtime `STEP_MEDIA_MANAGER`.
- Le pipeline de conversion documente explicitement la correspondance `SCENE_MEDIA_MANAGER <-> STEP_MEDIA_MANAGER`.
- L ecart `action:FORCE_WIN_ETAPE2` (present bundle, absent firmware) doit etre trace dans le delta de release.

## 4) Contrat du step terminal `STEP_MEDIA_MANAGER`
Definition attendue (runtime):
```json
{
  "step_id": "STEP_MEDIA_MANAGER",
  "screen_scene_id": "SCENE_MEDIA_MANAGER",
  "audio_pack_id": "",
  "actions": ["ACTION_TRACE_STEP", "ACTION_SET_BOOT_MEDIA_MANAGER"],
  "apps": ["APP_SCREEN", "APP_GATE", "APP_WIFI", "APP_ESPNOW"],
  "mp3_gate_open": true,
  "transitions": []
}
```

Invariants:
- step terminal (aucune transition sortante),
- `mp3_gate_open` a `true` pour ouvrir l usage media,
- l action de boot mode est executee a l entree du step.

## 5) Boot mode et persistence
### 5.1 Contrat cible
Apres victoire finale + entree Media Manager:
- mode de demarrage cible: `media_manager`,
- flag de validation media: `true`,
- au reboot, routage vers `SCENE_MEDIA_MANAGER`.

### 5.2 Etat runtime actuel a respecter
- Le code contient un verrou compile: `kLockNvsMediaManagerMode = true`.
- Quand ce verrou est actif, `ACTION_SET_BOOT_MEDIA_MANAGER` est bloquee et loggee.
- Les commandes de controle manuel restent valides:
  - `BOOT_MODE_STATUS`
  - `BOOT_MODE_SET STORY`
  - `BOOT_MODE_SET MEDIA_MANAGER`
  - `BOOT_MODE_CLEAR`

### 5.3 Stockage NVS
Namespace: `zacus_boot`
- `startup_mode` (`story` ou `media_manager`)
- `media_validated` (bool)

## 6) Contrat API Web pour Media Manager
### 6.1 Etat global
`GET /api/status` doit exposer au minimum:
```json
{
  "story": {
    "scenario": "DEFAULT",
    "step": "STEP_MEDIA_MANAGER",
    "screen": "SCENE_MEDIA_MANAGER"
  },
  "media": {
    "ready": true,
    "playing": false,
    "recording": false,
    "record_limit_seconds": 30,
    "record_elapsed_seconds": 0,
    "record_file": "",
    "record_simulated": true,
    "music_dir": "/music",
    "picture_dir": "/picture",
    "record_dir": "/recorder",
    "last_ok": true,
    "last_error": ""
  }
}
```

### 6.2 Endpoints media
1. `GET /api/media/files?kind=<music|picture|recorder>`
   - 200: `{ "ok": true, "kind": "...", "files": ["/music/a.mp3"] }`
   - 400: `{ "ok": false, "kind": "...", "error": "invalid_kind" }`
2. `POST /api/media/play` body `{ "path": "/music/file.mp3" }` ou `{ "file": "file.mp3" }`
   - 200/400: `{ "action": "MEDIA_PLAY", "ok": <bool> }`
3. `POST /api/media/stop`
   - 200/400: `{ "action": "MEDIA_STOP", "ok": <bool> }`
4. `POST /api/media/record/start` body `{ "seconds": 20, "filename": "take1.wav" }`
   - 200/400: `{ "action": "REC_START", "ok": <bool> }`
5. `POST /api/media/record/stop`
   - 200/400: `{ "action": "REC_STOP", "ok": <bool> }`
6. `GET /api/media/record/status`
   - 200: objet status media (meme schema que `media` dans `/api/status`)

### 6.3 Endpoint control (fallback/ops)
`POST /api/control` body `{ "action": "..." }` supporte au minimum:
- `MEDIA_LIST <picture|music|recorder>`
- `MEDIA_PLAY <path>`
- `MEDIA_STOP`
- `REC_START [seconds] [filename]`
- `REC_STOP`
- `REC_STATUS`
- `BOOT_MODE_STATUS`
- `BOOT_MODE_SET <STORY|MEDIA_MANAGER>`
- `BOOT_MODE_CLEAR`

Reponse:
- 200: `{ "ok": true, "action": "<input>" }`
- 400: `{ "ok": false, "action": "<input>", "error": "<reason>" }`

## 7) Spec d integration frontend
Le frontend doit:
1. Activer la vue Media Hub si `story.screen == SCENE_MEDIA_MANAGER` (et tolerer `story.step == STEP_MEDIA_MANAGER`).
2. Ne pas supposer que le target final du FSM est toujours un `STEP_*` (alias scene autorise en entree bundle).
3. Utiliser `/api/media/files` puis `/api/media/play` sans parser d extension en dur.
4. Afficher les erreurs `ok=false` et conserver la derniere valeur de `media.last_error`.
5. Afficher l indicateur `record_simulated` pour eviter une confusion "enregistrement reel" vs placeholder WAV.

## 8) Criteres d acceptation (QA)
1. Enchainement scenario:
   - `STEP_FINAL_WIN` atteint,
   - transition vers media manager effective,
   - `story.screen == SCENE_MEDIA_MANAGER`.
2. API:
   - `/api/media/files` OK pour `music`, `picture`, `recorder`,
   - `kind=video` retourne 400 + `invalid_kind`.
3. Playback:
   - `/api/media/play` met `media.playing=true`,
   - `/api/media/stop` remet `media.playing=false`.
4. Recording:
   - `/api/media/record/start` cree un `.wav` dans `/recorder`,
   - auto-stop quand `record_elapsed_seconds >= record_limit_seconds`.
5. Boot mode:
   - etat lock actuel documente dans le rapport (mode persistant bloque si lock actif),
   - commandes `BOOT_MODE_SET` et `BOOT_MODE_CLEAR` verifiees via `/api/control`.

## 9) Handoff attendu pour agents/devs
- Firmware: fournir evidences serial sur action boot mode et routage boot.
- Web: fournir capture payload `/api/status` + parcours UI media.
- Scenario: fournir note de normalisation `SCENE_MEDIA_MANAGER -> STEP_MEDIA_MANAGER` et delta des triggers.
