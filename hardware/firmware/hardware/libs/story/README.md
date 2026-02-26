---
# Zacus Firmware – STORY portable (génération + runtime)

---

## 📝 Description

Permet d’ajouter ou modifier un scénario STORY sans toucher au moteur C++.
// TODO NO DEV TESTING ? (need KILL_LIFE ?)

---

## 📦 Fonctionnement

Étapes pour ajouter un scénario :
1. Écrire un fichier `../docs/protocols/story_specs/scenarios/*.yaml`
2. Valider le spec
3. Générer le code C++
4. Compiler/flasher

Flux par défaut migré :
```
UNLOCK → U_SON_PROTO → WAIT_ETAPE2 → ETAPE2 → DONE
```
Tous les nouveaux scénarios doivent suivre ce flux (ou l’étendre, jamais le modifier).

---

## 🚀 Installation & usage

Sources de vérité :
- Schéma logique : `../docs/protocols/story_specs/schema/story_spec_v1.yaml`
- Template auteur : `../docs/protocols/story_specs/templates/scenario.template.yaml`
- Scénarios exemples : `../docs/protocols/story_specs/scenarios/`

Génération :
- `tools/story_gen/story_gen.py`
- Code généré : `src/story/generated/`

Mini Apps FSM :
- Interface commune : `src/story/apps/story_app.h`
- Apps : `LaDetectorApp`, `AudioPackApp`, `ScreenSceneApp`, `Mp3GateApp`

---

## 🤝 Contribuer

Merci de lire [../../../../../../CONTRIBUTING.md](../../../../../../CONTRIBUTING.md) avant toute PR.

---

## 👤 Contact

Pour toute question ou suggestion, ouvre une issue GitHub ou contacte l’auteur principal :
- Clément SAILLANT — [github.com/electron-rare](https://github.com/electron-rare)
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
