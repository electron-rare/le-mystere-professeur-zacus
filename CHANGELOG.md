# Changelog

## [Unreleased]
- Workflow : nouvelle exportation `tools/scenario/export_md.py` et briefs Markdown (kit + `docs/_generated/SCENARIO_BRIEF.md`) alignés sur `game/scenarios/zacus_v1.yaml`.
- Printables : manifeste `printables/manifests/zacus_v1_printables.yaml`, prompts dédiés pour chaque asset et `tools/printables/validate_manifest.py` pour éviter les trous entre IDs et fichiers.
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
