# STORY V2 (YAML + Mini Apps FSM)

## Objectif

Permettre d'ajouter/modifier un scenario STORY sans toucher au moteur C++:

1. ecrire un fichier `docs/protocols/story_specs/scenarios/*.yaml`
2. valider le spec
3. generer le code C++
4. compiler/flasher

## Flux par défaut (Story V2)

Le flux par défaut migré et normalisé est:

```
UNLOCK → U_SON_PROTO → WAIT_ETAPE2 → ETAPE2 → DONE
```

**Tous les nouveaux scénarios doivent suivre ce flux par défaut** (ou l'étendre, jamais le modifier).

## Source de verite

- schema logique: `docs/protocols/story_specs/schema/story_spec_v1.yaml`
- template auteur: `docs/protocols/story_specs/templates/scenario.template.yaml`
- scenario migre PR1: `docs/protocols/story_specs/scenarios/default_unlock_win_etape2.yaml`
- scenario additionnel RC2: `docs/protocols/story_specs/scenarios/spectre_radio_lab.yaml`

Le runtime V2 charge uniquement le code genere:

- `src/story/generated/scenarios_gen.h`
- `src/story/generated/scenarios_gen.cpp`
- `src/story/generated/apps_gen.h`
- `src/story/generated/apps_gen.cpp`

Le generateur expose aussi la config app LA:

- `LaDetectorAppConfigDef`
- `generatedLaDetectorConfigByBindingId(const char* id)`

## Prompts d'authoring Story

Story authoring prompts sont **distincts des ops/debug prompts** et servent à assister la création de nouveaux scénarios YAML.

- **Localisation:** `docs/protocols/story_specs/prompts/*.prompt.md`
- **Exemples:** `spectre_radio_lab.prompt.md`
- **Usages:** Comme aides d'authoring, ou via outils Codex si nécessaire

Voir [Story Authoring Prompts README](story_specs/prompts/README.md) pour la taxonomie complète.

## Mini Apps FSM

Interface commune:

- `src/story/apps/story_app.h`
- `begin(context)`
- `start(stepContext)`
- `update(nowMs, eventSink)`
- `stop(reason)`
- `handleEvent(event, eventSink)`
- `snapshot()`

Apps PR1:

- `LaDetectorApp`
- `AudioPackApp`
- `ScreenSceneApp`
- `Mp3GateApp`

Evolution RC2:

- `LaDetectorApp` devient full-owned pour l'unlock V2 (hold + emission event).
- config par binding YAML via `app_bindings[].config` (LA uniquement):
	- `hold_ms`
	- `unlock_event`
	- `require_listening`

Host:

- `src/story/apps/story_app_host.h`
- `src/story/apps/story_app_host.cpp`

`StoryControllerV2` pilote le moteur de transitions, et le `StoryAppHost` applique les effets metier (audio/screen/gate/actions) par etape.

## Commandes de generation

- pipeline guide: docs/protocols/STORY_V2_PIPELINE.md

Depuis `hardware/firmware/esp32_audio`:

```bash
make story-validate
make story-gen
python3 tools/story_gen/story_gen.py deploy --strict
make qa-story-v2
```

Validation:

- mode strict (`--strict`) active par defaut via `Makefile`
- verifie structure, IDs, transitions, bindings app
- verifie la config `LA_DETECTOR` (`hold_ms` / `unlock_event` / `require_listening`)
- rejette aussi les champs inconnus
- retour erreurs stable `file + field + reason + code`

Generation:

- deterministe (sort par `scenario.id`)
- ajoute une banniere `spec_hash` dans les fichiers generes pour tracer la version spec en review
- genere les 4 fichiers `src/story/generated/*`

## Commandes serie STORY V2

- `STORY_V2_ENABLE [STATUS|ON|OFF]`
- `STORY_V2_TRACE [ON|OFF|STATUS]`
- `STORY_V2_TRACE_LEVEL [OFF|ERR|INFO|DEBUG|STATUS]`
- `STORY_V2_STATUS`
- `STORY_V2_HEALTH`
- `STORY_V2_METRICS`
- `STORY_V2_METRICS_RESET`
- `STORY_V2_LIST`
- `STORY_V2_VALIDATE`
- `STORY_V2_EVENT <name>`
- `STORY_V2_STEP <id>`
- `STORY_V2_SCENARIO <id>`

Diagnostic:

- `STORY_V2_HEALTH` retourne un snapshot court (`OK|BUSY|ERROR|OUT_OF_CONTEXT`)
- `STORY_V2_TRACE ON` active les logs transitions/events pour debug live
- `STORY_V2_TRACE_LEVEL` ajuste la verbosite (`OFF/ERR/INFO/DEBUG`)
- `STORY_V2_METRICS` expose les compteurs events/transitions/queue drops/storm drops

Compat legacy conservee pendant PR1 via feature flag:

- default compile-time: `config::kStoryV2EnabledDefault = true`
- rollback runtime: `STORY_V2_ENABLE OFF`
- rollback release: remettre `kStoryV2EnabledDefault=false` puis recompiler/reflasher

## Creation d'un nouveau scenario

1. copier `docs/protocols/story_specs/templates/scenario.template.yaml`
2. renseigner steps/transitions/apps
3. optionnel: creer le prompt auteur associe dans `docs/protocols/story_specs/prompts/*.prompt.md`
4. `make story-validate`
5. `make story-gen`
6. `make qa-story-v2`
7. `pio run -e esp32dev`

Les prompts Story sont une categorie distincte des prompts Codex (ops), mais peuvent etre utilises par les outils Codex si besoin.

Aucune modification du moteur V2 n'est requise pour un nouveau flux tant que le scenario reste dans le contrat StorySpec V1.

## QA sprint (S5/S6)

- smoke debut sprint:
	- `make qa-story-v2-smoke ESP32_PORT=<PORT_ESP32> SCREEN_PORT=<PORT_ESP8266>`
	- ou `make qa-story-v2-smoke-fast ESP32_PORT=<PORT_ESP32>`
- runbook live complet fin sprint:
	- `esp32_audio/tools/qa/live_story_v2_runbook.md`
- runbook release candidate:
	- `esp32_audio/tools/qa/live_story_v2_rc_runbook.md`
- checklist review PR:
	- `esp32_audio/tools/qa/story_v2_review_checklist.md`
- CI firmware:
	- `.github/workflows/firmware-ci.yml` (build + smoke gates)
	- Story-specific validation steps can be added to a future `firmware-story-v2.yml` workflow

## Mode auto (recommande)

Pour automatiser les gates (ports + smoke + UI link) en local:

```bash
cd hardware/firmware
./tools/dev/cockpit.sh rc-autofix
```

Ce mode auto:

- detecte les ports via la politique CP2102 (LOCATION)
- rejoue les checks smoke + UI link
- genere les artefacts dans `artifacts/rc_live/`

## Architecture Complète (FS + WebUI)

**Authoring → Generation → Filesystem → Runtime**

- **Authoring:** `docs/protocols/story_specs/scenarios/*.yaml`
- **Generation:** `story_gen.py validate + deploy` (converts YAML → JSON to FS)
- **Filesystem:** `/story/` on ESP (scenarios, apps, screens, actions)
- **Runtime:** StoryEngineV2 loads from FS, Story_n_CodePackApp orchestrates
- **WebUI:** Smartphone/browser interface (select, orchestrate, design stories)

**Related documentation:**
- [`STORY_V2_APP_STORAGE.md`](./STORY_V2_APP_STORAGE.md) — FS structure, data models, JSON storage
- [`STORY_V2_WEBUI.md`](./STORY_V2_WEBUI.md) — REST API, WebSocket, React/Vue architecture
