# Générer un scénario STORY V2 (firmware ESP32)

Ce guide explique **comment créer/modifier un scénario** pour le firmware ESP32 sans toucher au moteur C++.

Important:
- **V2** ici = modèle de scénario + moteur runtime.
- Le pilotage série recommandé pour l'automatisation est le protocole JSON-lines **V3** (`story.*`).

## Prérequis

Depuis ce dossier:

```bash
cd hardware/firmware
```

## 1) Créer (ou dupliquer) un scénario YAML

Point de départ recommandé:

- template: `docs/protocols/story_specs/templates/scenario.template.yaml`
- exemple existant: `docs/protocols/story_specs/scenarios/default_unlock_win_etape2.yaml`
- schéma de référence: `docs/protocols/story_specs/schema/story_spec_v1.yaml`

Crée un nouveau fichier dans `docs/protocols/story_specs/scenarios/`, par exemple:

`docs/protocols/story_specs/scenarios/mon_scenario.yaml`

Exemple prêt à l'emploi dans le repo:

- `docs/protocols/story_specs/scenarios/example_unlock_express.yaml`
- `docs/protocols/story_specs/scenarios/example_unlock_express_done.yaml`

## 2) Définir la structure minimale

Dans le YAML, renseigne au minimum:

- `id` (identifiant unique)
- `version` (entier; le générateur accepte les versions `1..3` et ne bloque pas tant que c'est un entier)
- `initial_step`
- `app_bindings`
- `steps`
- `transitions`

Règle pratique:
- chaque `target_step_id` doit pointer vers une étape existante,
- les IDs doivent être uniques,
- les transitions doivent permettre d’atteindre une fin logique (`DONE` ou équivalent).

## 3) Valider le scénario

```bash
./tools/dev/story-gen validate
```

Cette commande vérifie:

- la conformité de structure,
- les IDs / références,
- la cohérence des transitions,
- les bindings des mini-apps.

Corrige les erreurs jusqu’à obtenir une validation propre.

## 4) Générer le code C++

```bash
./tools/dev/story-gen generate-cpp
```

Le générateur produit les fichiers utilisés par le runtime V2:

- `hardware/libs/story/src/generated/scenarios_gen.h`
- `hardware/libs/story/src/generated/scenarios_gen.cpp`
- `hardware/libs/story/src/generated/apps_gen.h`
- `hardware/libs/story/src/generated/apps_gen.cpp`

## 5) Compiler et flasher

```bash
pio run -e esp32dev
```

Puis upload selon ton port série:

```bash
pio run -e esp32dev -t upload --upload-port /dev/ttyUSB0
```

## 6) Tester côté série

Dans le moniteur série, commandes utiles (JSON-lines V3):

- `{"cmd":"story.status"}`
- `{"cmd":"story.list"}`
- `{"cmd":"story.load","data":{"scenario":"DEFAULT"}}`
- `{"cmd":"story.event","data":{"event":"UNLOCK"}}`
- `{"cmd":"story.step","data":{"step":"STEP_WAIT_UNLOCK"}}`
- `{"cmd":"story.validate"}`

Référence protocole série:
- `docs/protocols/story_v3_serial.md`

## Workflow court (copier-coller)

```bash
cd hardware/firmware
./tools/dev/story-gen validate
./tools/dev/story-gen generate-cpp
pio run -e esp32dev
```

## Conseils pour éviter les blocages

- Commence simple: 3 à 4 étapes max au début.
- N’ajoute qu’un seul nouveau type d’événement à la fois.
- Teste chaque transition clé au moniteur série (`{"cmd":"story.event","data":{"event":"..."}}`).
- Garde un scénario « de secours » valide dans `docs/protocols/story_specs/scenarios/`.
