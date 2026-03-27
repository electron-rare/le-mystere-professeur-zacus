# Changelog

## [Unreleased]

### Ajouté (2026-03-27)
- **Voice bridge rewrite** (f673822): pont audio OPUS ESP32-S3-BOX-3 vers mascarade (+441 lignes), routage hint `[HINT:puzzle:level]` vers le moteur de hints mascarade.
- **scenario_manager** (f673822): module `scenario_manager.cpp/.h` — gestionnaire d'état scénario centralisé, transitions validées, hooks pré/post-transition, persistence NVS.
- **P0 fixes**: correction du crash audio DMA sur cold boot BOX-3, fix du timeout WebSocket reconnect (backoff exponentiel 1-30s), correction du double-free dans `voice_pipeline` sur abort rapide.

### Corrigé (2026-03-27)
- **VAD hint routing** (f87820e): le routage des hints via VAD ne déclenchait pas correctement le moteur mascarade.
- **Session leak** (f87820e): fuite mémoire dans les sessions WebSocket non fermées proprement.
- **YAML regex** (f87820e): regex de parsing scénario corrigée pour les caractères spéciaux.
- **Markdown rendering** (f87820e): correction de l'affichage markdown dans le dashboard.
- **UTF-8 encoding** (f87820e): encodage des réponses hints en UTF-8 (accents français).

### Ajouté
- Runtime 3 : compilateur (`tools/scenario/compile_runtime3.py`) et simulateur (`tools/scenario/simulate_runtime3.py`) pour le moteur de scénarios V2.
- Studio visuel React + Blockly dans `frontend-scratch-v2/` pour l'édition graphique des scénarios.
- Schéma Scenario V2 : refonte du format YAML, validation renforcée et pivot vers `game/scenarios/zacus_v2.yaml` comme source canonique.
- Analyse d'intégration IA : `docs/AI_INTEGRATION_ANALYSIS.md` — specs, opportunités et roadmap.
- Documentation : cartes d'architecture (system-map, component-map, data-flow-map, feature-map, migration-map, agent-matrix, release-map), specs de déploiement, runbook opérationnel.
- Spécifications de durcissement sécurité.
- Cibles Makefile : `runtime3-compile`, `runtime3-simulate`, `runtime3-verify`, `runtime3-test`, `runtime3-firmware-bundle`.

### Modifié
- Consolidation du dépôt : suppression des doublons (`AGENTS 2.md`, `AGENT_TODO 2.md`).
- Sous-module `ESP32_ZACUS` mis à jour (23 commits).

### Précédemment non versionné
- Workflow : nouvelle exportation `tools/scenario/export_md.py` et briefs Markdown (kit + `docs/_generated/SCENARIO_BRIEF.md`) alignés sur `game/scenarios/zacus_v2.yaml`.
- Printables : manifeste `printables/manifests/zacus_v2_printables.yaml`, prompts dédiés pour chaque asset et `tools/printables/validate_manifest.py` pour éviter les trous entre IDs et fichiers.
- Documentation : AGENTS, WORKFLOWS, GLOSSARY, Quickstart, index et le Makefile rappellent que le YAML est la single source of truth et listent les commandes standard de validation/export.
- Tooling : `tools/scenario/validate_scenario.py`, `tools/audio/validate_manifest.py`, `tools/scenario/export_md.py` et `tools/printables/validate_manifest.py` accompagnés par `Makefile` facilitent la maintenance.
- Automation : workflow GitHub `.github/workflows/validate.yml` installe PyYAML puis lance les validations de scénario, audio et printables.

## [0.2.0] - 2026-02-12

### Ajouté
- Workflow de validation audio au boot (touches + commandes série) avec timeout et limite de relecture.
- Outils de diagnostic clavier analogique : `KEY_STATUS`, `KEY_SET`, `KEY_SET_ALL`, `KEY_RAW_ON/OFF`, auto-test `KEY_TEST_*`.
- Calibration micro série et logs de santé micro (`[MIC_CAL] ...`).
- Makefile pour standardiser build/flash/monitor ESP32 + écran ESP8266.

### Modifié
- UX `U_LOCK`/déverrouillage LA et transitions automatiques vers `MODULE U-SON` puis lecteur MP3.
- Amélioration de l'affichage OLED (séquences visuelles de déverrouillage, effet glitch adouci).
- Stabilisation du mapping clavier analogique et robustesse générale des interactions.

### Corrigé
- Robustesse du lien série ESP32 -> ESP8266 et gestion des états de reprise.

## [0.1.0] - 2026-02-03
- Initialisation du dépôt et fichiers de gouvernance
