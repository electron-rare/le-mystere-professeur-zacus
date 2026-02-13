# STORY V2 (YAML + Mini Apps FSM)

## Objectif

Permettre d'ajouter/modifier un scenario STORY sans toucher au moteur C++:

1. ecrire un fichier `story_specs/scenarios/*.yaml`
2. valider le spec
3. generer le code C++
4. compiler/flasher

Le flux par defaut migre est:

`UNLOCK -> WIN -> WAIT_ETAPE2 -> ETAPE2 -> DONE`

## Source de verite

- schema logique: `story_specs/schema/story_spec_v1.yaml`
- template auteur: `story_specs/templates/scenario.template.yaml`
- scenario migre PR1: `story_specs/scenarios/default_unlock_win_etape2.yaml`

Le runtime V2 charge uniquement le code genere:

- `src/story/generated/scenarios_gen.h`
- `src/story/generated/scenarios_gen.cpp`
- `src/story/generated/apps_gen.h`
- `src/story/generated/apps_gen.cpp`

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

Host:

- `src/story/apps/story_app_host.h`
- `src/story/apps/story_app_host.cpp`

`StoryControllerV2` pilote le moteur de transitions, et le `StoryAppHost` applique les effets metier (audio/screen/gate/actions) par etape.

## Commandes de generation

Depuis `hardware/firmware/esp32`:

```bash
make story-validate
make story-gen
make qa-story-v2
```

Validation:

- mode strict (`--strict`) active par defaut via `Makefile`
- verifie structure, IDs, transitions, bindings app
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

- default compile-time: `config::kStoryV2EnabledDefault = false`
- rollback runtime: `STORY_V2_ENABLE OFF`
- rollback release: garder `kStoryV2EnabledDefault=false` puis recompiler/reflasher

## Creation d'un nouveau scenario

1. copier `story_specs/templates/scenario.template.yaml`
2. renseigner steps/transitions/apps
3. `make story-validate`
4. `make story-gen`
5. `make qa-story-v2`
6. `pio run -e esp32dev`

Aucune modification du moteur V2 n'est requise pour un nouveau flux tant que le scenario reste dans le contrat StorySpec V1.
