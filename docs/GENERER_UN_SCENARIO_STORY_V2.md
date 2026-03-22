# Générer un scénario STORY V2

Ce guide explique **comment créer, compiler et tester un scénario** avec le Runtime 3 sans toucher au moteur C++.

## Prérequis

- Python 3.x avec PyYAML installé (`pip install pyyaml`)
- Le dépôt cloné à la racine du projet

## 1) Source canonique du scénario

Le fichier de référence est :

```
game/scenarios/zacus_v2.yaml
```

C'est la **single source of truth**. Toute modification de scénario doit passer par ce fichier YAML.

## 2) Édition visuelle (studio Blockly)

Pour éditer le scénario graphiquement, utiliser le studio React + Blockly :

```bash
cd frontend-scratch-v2
npm install
npm run dev
```

Le studio permet de manipuler les étapes, transitions et événements visuellement, puis d'exporter vers le format YAML V2.

## 3) Valider le scénario

```bash
make scenario-validate
```

Cette commande vérifie :

- la conformité de structure au schéma V2,
- les IDs / références croisées,
- la cohérence des transitions,
- les bindings des mini-apps.

Corriger les erreurs jusqu'à obtenir une validation propre.

## 4) Compiler avec le Runtime 3

```bash
make runtime3-compile
```

Cela exécute `tools/scenario/compile_runtime3.py` sur le scénario par défaut. Pour spécifier un fichier :

```bash
make runtime3-compile SCENARIO=game/scenarios/zacus_v2.yaml
```

## 5) Simuler le scénario

```bash
make runtime3-simulate
```

Cela exécute `tools/scenario/simulate_runtime3.py` et déroule le scénario hors firmware pour vérifier les transitions et les états finaux.

## 6) Vérifier les pivots et générer le bundle firmware

```bash
make runtime3-verify
make runtime3-firmware-bundle
```

- `runtime3-verify` valide les points pivots du graphe de scénario.
- `runtime3-firmware-bundle` produit le bundle prêt à flasher sur l'ESP32.

## 7) Lancer les tests unitaires Runtime 3

```bash
make runtime3-test
```

## 8) Compiler et flasher le firmware

```bash
cd ESP32_ZACUS
pio run -e esp32dev
pio run -e esp32dev -t upload --upload-port /dev/ttyUSB0
```

## 9) Tester côté série

Dans le moniteur série, commandes utiles :

- `STORY_V2_ENABLE ON`
- `STORY_V2_LIST`
- `STORY_V2_SCENARIO <id>`
- `STORY_V2_STATUS`
- `STORY_V2_EVENT <name>`
- `STORY_V2_VALIDATE`

## Workflow court (copier-coller)

```bash
make scenario-validate
make runtime3-compile
make runtime3-simulate
make runtime3-verify
```

## Toutes les cibles Makefile disponibles

| Cible | Description |
|---|---|
| `scenario-validate` | Validation YAML du scénario |
| `runtime3-compile` | Compilation Runtime 3 |
| `runtime3-simulate` | Simulation hors firmware |
| `runtime3-verify` | Vérification des pivots |
| `runtime3-test` | Tests unitaires Runtime 3 |
| `runtime3-firmware-bundle` | Export bundle firmware ESP32 |

## Conseils pour éviter les blocages

- Commencer simple : 3 à 4 étapes max au début.
- N'ajouter qu'un seul nouveau type d'événement à la fois.
- Toujours valider (`make scenario-validate`) avant de compiler.
- Tester chaque transition clé au moniteur série (`STORY_V2_EVENT ...`).
- Utiliser `make runtime3-simulate` pour déboguer sans flasher.
