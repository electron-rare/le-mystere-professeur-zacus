# Quickstart

## En un coup d’œil
- Source de vérité : `game/scenarios/zacus_v1.yaml` (dérive tout le reste : kit MJ, audio, printables).
- Public : 6 à 14 enfants, 60–90 minutes.
- Matériel : `kit-maitre-du-jeu/`, `printables/`, `game/scenarios/zacus_v1.yaml`, `audio/manifests/zacus_v1_audio.yaml`.
- Licences : contenus créatifs CC BY-NC 4.0 (`LICENSES/CC-BY-NC-4.0.txt`), code/script MIT (`LICENSES/MIT.txt`).

## Installer
1. Copie les imprimables depuis `printables/export/pdf/` ou généré via les prompts listés dans `printables/manifests/zacus_v1_printables.yaml` + `printables/src/prompts/`.
2. Prépare un lecteur audio avec `audio/manifests/zacus_v1_audio.yaml` et les fichiers de `game/prompts/audio/`.
3. Positionne les stations selon `kit-maitre-du-jeu/plan-stations-et-mise-en-place.md`, prépare les rôles (`distribution-des-roles.md`) et l’accueil rapide (`script-minute-par-minute.md`).

## Déroulé express
1. Accueil + immersion (0-10 min) : boucle audio d’`intro.md`, attribution des rôles, mise au courant sur les règles anti-chaos.
2. Support d’enquête (10-65 min) : station par station, fiche d’enquête, audio `incident.md`, indices prints.
3. Synthèse & final (65-90 min) : audio `accusation.md`, accusation finale, audio `solution.md`, lecture `solution-complete.md`.

## Outils de validation
- Scénario : `python tools/scenario/validate_scenario.py game/scenarios/zacus_v1.yaml`
- Audio : `python tools/audio/validate_manifest.py audio/manifests/zacus_v1_audio.yaml`
- Printables : `python tools/printables/validate_manifest.py printables/manifests/zacus_v1_printables.yaml`
- Export Markdown : `python3 tools/scenario/export_md.py game/scenarios/zacus_v1.yaml`

## Pour aller plus loin
- Crée une variante : duplique le YAML, modifie `canon` et `solution` et revalide avec le script.
- Ajoute des cartes imprimables en t’inspirant des prompts dans `printables/src/prompts/`.
- Mets à jour ce quickstart via un PR si le montage change (p. ex. nouvelle station ou nouvelle plage horaire).
