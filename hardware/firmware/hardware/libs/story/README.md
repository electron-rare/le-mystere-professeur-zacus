# STORY portable (generation + runtime)

## Objectif

Permettre d'ajouter/modifier un scenario STORY sans toucher au moteur C++:

1. ecrire un fichier `../docs/protocols/story_specs/scenarios/*.yaml`
2. valider le spec
3. generer le code C++
4. compiler/flasher

Le flux par defaut migre est:

```
UNLOCK → U_SON_PROTO → WAIT_ETAPE2 → ETAPE2 → DONE
```

**Tous les nouveaux scénarios doivent suivre ce flux par défaut** (ou l'étendre, jamais le modifier).

## Source de verite

- schema logique: `../docs/protocols/story_specs/schema/story_spec_v1.yaml`
- template auteur: `../docs/protocols/story_specs/templates/scenario.template.yaml`
- scenario migre PR1: `../docs/protocols/story_specs/scenarios/default_unlock_win_etape2.yaml`
- scenario additionnel RC2: `../docs/protocols/story_specs/scenarios/spectre_radio_lab.yaml`

Le runtime portable charge le code genere et/ou LittleFS:

- `src/story/generated/scenarios_gen.h`
- `src/story/generated/scenarios_gen.cpp`
- `src/story/generated/apps_gen.h`
- `src/story/generated/apps_gen.cpp`

Le generateur expose aussi la config app LA:

- `LaDetectorAppConfigDef`
- `generatedLaDetectorConfigByBindingId(const char* id)`

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

`StoryPortableRuntime` encapsule le moteur de transitions, et le `StoryAppHost` applique les effets metier (audio/screen/gate/actions) par etape.

## Commandes de generation

Depuis `hardware/firmware`:

```bash
./tools/dev/story-gen validate
./tools/dev/story-gen generate-cpp
./tools/dev/story-gen generate-bundle
./tools/dev/story-gen all
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

## Commandes serie STORY V3 (JSON-lines)

- `{"cmd":"story.status"}`
- `{"cmd":"story.list"}`
- `{"cmd":"story.load","data":{"scenario":"DEFAULT"}}`
- `{"cmd":"story.step","data":{"step":"STEP_WAIT_UNLOCK"}}`
- `{"cmd":"story.validate"}`
- `{"cmd":"story.event","data":{"event":"UNLOCK"}}`

Diagnostic:
- contrat réponse: `{"ok":bool,"code":"...","data":{...}}`
- référence protocole: `docs/protocols/story_v3_serial.md`

## Creation d'un nouveau scenario

1. copier `../docs/protocols/story_specs/templates/scenario.template.yaml`
2. renseigner steps/transitions/apps
3. optionnel: creer le prompt auteur associe dans `../docs/protocols/story_specs/prompts/*.prompt.md`
4. `make story-validate`
5. `./tools/dev/story-gen all`
6. smoke série V3 (status/load/step/validate)
7. `pio run -e esp32dev`

Aucune modification du moteur V2 n'est requise pour un nouveau flux tant que le scenario reste dans le contrat StorySpec V1.

## QA sprint (S5/S6)

- smoke debut sprint:
  - `make qa-story-v2-smoke ESP32_PORT=<PORT_ESP32> SCREEN_PORT=<PORT_ESP8266>`
  - ou `make qa-story-v2-smoke-fast ESP32_PORT=<PORT_ESP32>`
- runbook live complet fin sprint:
  - `tools/qa/live_story_v2_runbook.md`
- runbook release candidate:
  - `tools/qa/live_story_v2_rc_runbook.md`
- checklist review PR:
  - `tools/qa/story_v2_review_checklist.md`
- CI firmware:
  - `.github/workflows/firmware-ci.yml` (build + smoke gates)
  - Story-specific validation steps can be added to a future `firmware-story-v2.yml` workflow
