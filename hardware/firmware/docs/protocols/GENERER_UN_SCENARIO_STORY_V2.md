# Générer un scénario STORY V2 (firmware ESP32)

Ce guide explique **comment créer/modifier un scénario** pour le firmware ESP32 sans toucher au moteur C++.

## Prérequis

Depuis ce dossier:

```bash
cd hardware/firmware/esp32_audio
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
make story-validate
```

Cette commande vérifie:

- la conformité de structure,
- les IDs / références,
- la cohérence des transitions,
- les bindings des mini-apps.

Corrige les erreurs jusqu’à obtenir une validation propre.

## 4) Générer le code C++

```bash
make story-gen
```

Le générateur produit les fichiers utilisés par le runtime V2:

- `src/story/generated/scenarios_gen.h`
- `src/story/generated/scenarios_gen.cpp`
- `src/story/generated/apps_gen.h`
- `src/story/generated/apps_gen.cpp`

## 5) Compiler et flasher

```bash
pio run -e esp32dev
```

Puis upload selon ton port série:

```bash
pio run -e esp32dev -t upload --upload-port /dev/ttyUSB0
```

## 6) Tester côté série

Dans le moniteur série, commandes utiles:

- `STORY_V2_ENABLE ON`
- `STORY_V2_LIST`
- `STORY_V2_SCENARIO <id>`
- `STORY_V2_STATUS`
- `STORY_V2_EVENT <name>`
- `STORY_V2_VALIDATE`

## Workflow court (copier-coller)

```bash
cd hardware/firmware/esp32_audio
make story-validate
make story-gen
pio run -e esp32dev
```

## Conseils pour éviter les blocages

- Commence simple: 3 à 4 étapes max au début.
- N’ajoute qu’un seul nouveau type d’événement à la fois.
- Teste chaque transition clé au moniteur série (`STORY_V2_EVENT ...`).
- Garde un scénario « de secours » valide dans `docs/protocols/story_specs/scenarios/`.
