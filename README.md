# Le Mystère du Professeur Zacus — kit d’enquête (printables + guide MJ + audio)

Kit d’enquête pour anniversaire (6–14 enfants, 60–90 minutes), ambiance laboratoire / campus scientifique.

## Aperçu visuel
![Vue d'ensemble du dépôt](docs/assets/repo-map.svg)

## Contenu
### 1) Kit Maître du jeu
Dossier : `kit-maitre-du-jeu/`
- script minute-par-minute (déroulé prêt à jouer)
- solution complète (coupable, mobile, méthode, chronologie)
- checklist matériel + mise en place 4 stations
- distribution des rôles (modulable 6–14 enfants)
- guide anti-chaos + plan stations
- générer un scénario + validation

### 2) Printables (imprimables)
Dossier : `printables/`
- structure `src/` + `export/{pdf,png}/`
- prompts graphiques dans `src/prompts/` + `WORKFLOW.md`
- ordres d’impression standardisés
- badges détective, fiches d’enquête, cartes personnages, zones

### 3) Game & audio
- `game/scenarios/zacus_v1.yaml` (canon, solution unique, timeline + preuves)
- `audio/manifests/zacus_v1_audio.yaml` + `game/prompts/audio/` (intro, incident, accusation, solution)
- scénario validé par `tools/scenario/validate_scenario.py`
- manifestes audio validés par `tools/audio/validate_manifest.py`

### 4) Outils & documentation
- `docs/QUICKSTART.md` (mise en place express, licences)
- `docs/STYLEGUIDE.md` (ton, structure, prompts)
- `docs/index.md` + `docs/repo-status.md` pour la navigation et l’état du dépôt
- `include-humain-IA/` (contenus annexes, renommé pour la portabilité)
- `tools/dev/zacus.sh codex --prompt tools/dev/codex_prompts/zacus_overhaul_one_shot.md` lance le prompt de Codex directement depuis le dépôt.
- `tools/dev/zacus.sh zeroclaw-preflight --require-port` lance le préflight USB ZeroClaw avant actions hardware.
- Les artifacts de résolution de ports sont écrits sous `artifacts/ports/<timestamp>/ports_resolve.json` avec un pointeur `artifacts/ports/latest_ports_resolve.json`. Utilisez `ZACUS_MOCK_PORTS=1` (et `ZACUS_PORTS_FIXTURE=tools/test/fixtures/ports_list_macos.txt`) pour tester la boucle sans matériel.
- `tools/dev/zacus.sh codex --prompt tools/dev/codex_prompts/zacus_overhaul_one_shot.md` lance le prompt de Codex directement depuis le dépôt.

## État du projet
- **Kit MJ** : complet (script, plan, antic-chaos, solution, checklists, export PDF).
- **Printables** : prompts prêts, workflow documenté (`printables/WORKFLOW.md`), exports par thème organisés.
- **Game/audio** : scénario canon `zacus_v1`, manifestes, audio narratif et scripts de validation.
- **Outils** : scripts Python ded validation scenario/audio + guide rapide.

## Licence / Contribution
- Contenus créatifs (documents, PDFs, PNG, SVG, assets, prompts) : **CC BY-NC 4.0** (`LICENSES/CC-BY-NC-4.0.txt`).
- Code et scripts (firmware, outils, validation) : **MIT** (`LICENSES/MIT.txt`).
- Voir `CONTRIBUTING.md` pour les règles d’ajout et d’examen des contributions.

## Disclaimer
Projet indépendant, non affilié à aucune marque ou éditeur. Voir `DISCLAIMER.md`.

## Maintenance du dépôt (hors firmware)
- Plan : `docs/maintenance-repo.md`
- Audit : `docs/repo-audit.md`
- Repo status : `docs/repo-status.md`

## Contribuer
Voir `CONTRIBUTING.md`.

## Historique
Voir `CHANGELOG.md`.

## Mainteneur
L’électron rare

## Exemples transverses
- Index exemples: `examples/README.md`
