# Spécification — Local_IA + Source de vérité (Zacus)

## 1) Finalité

Produire **toute la génération de contenu story/printable en local** pour le projet Zacus, sans appel cloud, en garantissant qu’un seul flux de données fait foi :

- **source de vérité principale** : `scenario-ai-coherence/zacus_conversation_bundle_v3/scenario_runtime.json`
- **contrainte de conformité FSM** : `scenario-ai-coherence/zacus_conversation_bundle_v3/fsm_mermaid.md`
- **format final attendu** : `game/scenarios/zacus_v2.yaml` (et futures variantes V2)

Le firmware reste **intouché** par cette spécification.

## 2) Sources de vérité (hiérarchie)

1. `scenario_runtime.json` : transitions canonique, étapes, médias, politiques de LED/QR, durée, bindings.
2. `fsm_mermaid.md` : vérification du graphe d’états autorisé.
3. `zacus_v2.yaml` : gabarit opérationnel de sortie, versionnage, conventions de champs, validations.

### Règle de non-régression

- Une génération IA ne peut jamais sortir des noms de steps, d’événements, d’app bindings ou de noms de scènes déjà validés dans les deux premiers fichiers, sauf demande explicite dans le ticket.

## 3) Périmètre de la spécification local_IA

- Entrées utilisateur depuis le Studio frontend :
  - prompt de scénario (titre, durée, niveau, objectif, options de joueurs)
  - contraintes éditoriales
  - configuration médias/printables (formats demandés)
- Sorties attendues :
  - `YAML` conforme **Story V2** pour `game/scenarios/*.yaml`
  - `manifest_yaml` + `markdown` pour le bundle imprimables
  - objet `diagnostic` (`rationale`, `source`, `warnings`)
- Services inclus :
  - Docker LLM local (Ollama)
  - Gateway HTTP locale (script Python)
  - Frontend web Studio (React/Svelte selon stack du dépôt)

## 4) Architecture cible

### 4.1 Service Docker

- `ollama` (port `11434`) local.
- `sdxl` (profil `with-sdxl`, port `7860`) optionnel pour génération visuelle.
- Gateway HTTP (port `8787`) avec endpoints :
  - `POST /story_generate`
  - `POST /printables_plan`
  - `POST /visual_generate` (alias `image_generate`)
  - `GET /health`, `GET /ping`
- La gateway lit `tools/dev/docker-studio-ai/.env` (variables transmises en CLI) :
  - `LLM_URL`, `LLM_MODEL`
  - `SDXL_URL`, `SDXL_MODEL`, `SDXL_PROVIDER`, `SDXL_TIMEOUT_SEC`

### 4.2 Frontend

- variable `VITE_ZACUS_STUDIO_AI_URL` :
  - active le mode IA locale.
  - fallback automatique local si timeout/API error.

### 4.3 Stockage source

- Aucun stockage propriétaire dans le frontend.
- Toute donnée persistée par l’utilisateur passe toujours par export du YAML / manifest généré.

## 5) Contrat d’API Gateway

### 5.1 `POST /story_generate`

Request :

```json
{
  "mode": "story_generate",
  "scenario": {
    "scenarioId": "zacus_DEFAULT",
    "title": "Le mystère du professeur Zacus",
    "missionSummary": "...",
    "durationMinutes": 90,
    "minPlayers": 4,
    "maxPlayers": 12,
    "difficulty": "standard",
    "includeMediaManager": true,
    "customPrompt": "",
    "aiHint": ""
  },
  "strictMode": true
}
```

Réponse :

```json
{
  "yaml": "...yaml",
  "rationale": "...",
  "source": "ai_local",
  "diagnostic": {
    "checks": ["runtime_order_ok", "events_whitelisted", "steps_reachable"],
    "warnings": []
  }
}
```

### 5.2 `POST /printables_plan`

Request :

```json
{
  "mode": "printables_plan",
  "scenarioId": "zacus_DEFAULT",
  "title": "Le mystère du professeur Zacus",
  "selected": ["invitation_a6_recto", "fiche_enquete_a4"]
}
```

Réponse :

```json
{
  "manifest_yaml": "...yaml",
  "markdown": "...",
  "items": 2,
  "source": "local",
  "diagnostic": {"checks": ["printable_types_ok", "schema_min_fields_ok"]}
}
```

### 5.3 `POST /visual_generate` (alias `image_generate`)

Request :

```json
{
  "mode": "visual_generate",
  "prompt": "Affiche rétro pour le scénario Zacus, ambiance années 80",
  "negativePrompt": "watermark, blur",
  "width": 1024,
  "height": 1024,
  "steps": 25,
  "cfgScale": 7.5,
  "seed": -1,
  "count": 1,
  "provider": "auto",
  "model": "stabilityai/stable-diffusion-xl-base-1.0"
}
```

Réponse :

```json
{
  "images": [
    {
      "filename": "sdxl_1719890000_0.png",
      "mime": "image/png",
      "base64": "<payload base64>"
    }
  ],
  "count": 1,
  "provider": "sd_webui",
  "source": "sd"
}
```

### 5.4 Gestion d’erreurs

- Si LLM indisponible ou génération invalide :
  - `--no-fallback` => erreur explicite HTTP 500.
  - fallback par défaut => retour d’un scénario canonic basé sur `scenario_runtime.json`.

## 6) Contrat Story V2 (obligatoire)

- `id` string, `version: 2`, `title`, `description`.
- `initial_step == STEP_U_SON_PROTO`.
- `app_bindings` obligatoire :
  - au minimum `APP_AUDIO`, `APP_SCREEN`, `APP_GATE`, `APP_WIFI`, `APP_ESPNOW`, `APP_QR`, `APP_SERIAL`, `APP_TIMER`, `APP_UNLOCK`, `APP_ACTION`.
- `steps` :
  - au moins les 10 étapes attendues,
  - transitions avec :
    - `trigger` dans `{on_event|after_ms|immediate}`,
    - `event_type`, `event_name`, `target_step_id`, `after_ms`, `priority`.
- Validation obligatoire : `python3 tools/scenario/validate_scenario.py <scenario>`.

## 7) Référence FSM obligatoire (obligation haute)

Les transitions suivantes **doivent exister** ou être explicitement marquées `immutable_ref` dans la rationale :

1. `STEP_U_SON_PROTO` -> `STEP_U_SON_PROTO` via `audio_done:loop`
2. `STEP_U_SON_PROTO` -> `STEP_LA_DETECTOR` via `BTN:ANY` et `serial:FORCE_ETAPE2`
3. `STEP_LA_DETECTOR` -> `STEP_U_SON_PROTO` via `timer:ETAPE2_DUE`
4. `STEP_LA_DETECTOR` -> `STEP_RTC_ESP_ETAPE1` via `serial:BTN_NEXT|unlock:UNLOCK|action:ACTION_FORCE_ETAPE2|serial:FORCE_WIN_ETAPE1`
5. `STEP_RTC_ESP_ETAPE1` -> `STEP_WIN_ETAPE1` via `esp_now:ACK_WIN1|serial:FORCE_DONE`
6. `STEP_WIN_ETAPE1` -> `STEP_WARNING` via `serial:BTN_NEXT|serial:FORCE_DONE|esp_now:ACK_WARNING`
7. `STEP_WARNING` -> `STEP_WARNING` via `audio_done:loop`
8. `STEP_WARNING` -> `STEP_LEFOU_DETECTOR` via `BTN:ANY|serial:FORCE_ETAPE2`
9. `STEP_LEFOU_DETECTOR` -> `STEP_WARNING` via `timer:ETAPE2_DUE`
10. `STEP_LEFOU_DETECTOR` -> `STEP_RTC_ESP_ETAPE2` via `serial:BTN_NEXT|unlock:UNLOCK|action:ACTION_FORCE_ETAPE2|serial:FORCE_WIN_ETAPE2`
11. `STEP_RTC_ESP_ETAPE2` -> `STEP_QR_DETECTOR` via `esp_now:ACK_WIN2|serial:FORCE_DONE`
12. `STEP_QR_DETECTOR` -> `STEP_RTC_ESP_ETAPE2` via `timer:ETAPE2_DUE|event:QR_TIMEOUT`
13. `STEP_QR_DETECTOR` -> `STEP_FINAL_WIN` via `serial:BTN_NEXT|unlock:UNLOCK_QR|action:ACTION_FORCE_ETAPE2|serial:FORCE_WIN_ETAPE2`
14. `STEP_FINAL_WIN` -> `SCENE_MEDIA_MANAGER` via `timer:WIN_DUE|serial:BTN_NEXT|unlock:UNLOCK|action:FORCE_WIN_ETAPE2|serial:FORCE_WIN_ETAPE2`

## 8) Gouvernance de la source de vérité

- Aucune création d’événement métier en dehors du vocabulaire runtime.
- Toute nouvelle scène/step/event doit être proposée d’abord dans :
  1) `scenario_runtime.json`
  2) puis importée dans `fsm_mermaid.md`
  3) puis approuvée dans changelog/story PR.
- Le manifest imprimables peut être enrichi localement mais doit conserver une référence stable au `scenarioId`/`version` du YAML généré.

## 9) Démarrage recommandé

```bash
cd tools/dev/docker-studio-ai
cp .env.example .env
docker compose up -d --build
docker exec -it zacus-ollama ollama pull qwen2.5-coder:14b
curl http://127.0.0.1:8787/health
```

```bash
cp "fronted dev web UI/.env.local.example" "fronted dev web UI/.env.local"
# VITE_ZACUS_STUDIO_AI_URL=http://127.0.0.1:8787/story_generate
npm --prefix "fronted dev web UI" run dev
```

### One-liner

```bash
cd tools/dev/docker-studio-ai && [ -f .env ] || cp .env.example .env && docker compose up -d --build && docker exec -it zacus-ollama ollama pull "${LLM_MODEL:-qwen2.5-coder:14b}" && docker compose --profile with-sdxl up -d --build
```

### Bascule de LLM

- Local Ollama (défaut) :
  - `LLM_URL=http://ollama:11434/v1/chat/completions`
  - `LLM_MODEL=qwen2.5-coder:14b`
- Provider OpenAI-compatible (vLLM/LM Studio/OpenAI API) :
  - définir `LLM_URL` vers l’endpoint `/v1/chat/completions`
  - définir `LLM_MODEL` vers un modèle valide du provider
- Frontend : garder `VITE_ZACUS_STUDIO_AI_URL` inchangé (`/story_generate`), seule la stack gateway change.

## 10) Validation minimale

1. `python3 tools/dev/local_studio_ai_gateway.py --help`
2. `curl /health` et `/ping` => 200.
3. `curl -X POST /story_generate` => YAML validable.
4. `python3 tools/scenario/validate_scenario.py game/scenarios/<fichier>.yaml` => OK.
5. `python3 tools/printables/validate_manifest.py <manifest>` => OK.
6. `curl -X POST /visual_generate` (si profil `with-sdxl`) => image(s) retournée(s).
7. Test frontend :
   - génération avec IA locale active,
   - génération sans IA locale (fallback),
   - mêmes contraintes FSM/références.
