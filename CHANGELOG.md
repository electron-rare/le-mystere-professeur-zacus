# Changelog

## [Unreleased]
- Correction extraction: suppression des binaires versionnés (MP3/PDF générés) et passage en génération locale ignorée par Git (`audio/generated`, `printables/export/pdf/zacus_v1`).
- Génération locale des assets restants: `audio/generated/*` via script utilitaire + PDF placeholders dans `printables/export/pdf/zacus_v1/`.
- Ajout des utilitaires `tools/audio/generate_local_assets.py` et `tools/printables/generate_local_pdf_placeholders.py`.
- Ajout du canon scénario `game/scenarios/zacus_v1.yaml` (zones, suspects, étapes, hotline, solution unique, anti-chaos).
- Remplissage complet du kit MJ (`script-minute-par-minute`, `solution-complete`, `checklist`, `plan stations`, `distribution`, `guide anti-chaos`).
- Ajout d’une structure IA-friendly pour printables (`printables/src/prompts/*`, `printables/WORKFLOW.md`, placeholders).
- Ajout pipeline audio (`audio/README.md`, `audio/manifests/zacus_v1_audio.yaml`, prompts audio, validateur manifest).
- Ajout docs d’intégration hardware scénario + story spec `zacus_v1_unlock_and_etape2.yaml`.
- Harmonisation licence vers MIT (code) + CC BY-NC 4.0 (contenu/docs) et déplacement des anciennes licences dans `LICENSES/legacy/`.
- Ajout validateur scénario `tools/scenario/validate_scenario.py`.
- Ajout docs `docs/STYLEGUIDE.md`, `docs/QUICKSTART.md`, `docs/GLOSSARY.md`, `docs/repo-status.md`.

## [0.2.0] - 2026-02-12
- Workflow de validation audio au boot (touches + commandes série) avec timeout et limite de relecture.
- Outils de diagnostic clavier analogique : `KEY_STATUS`, `KEY_SET`, `KEY_SET_ALL`, `KEY_RAW_ON/OFF`, auto-test `KEY_TEST_*`.
- Calibration micro série et logs de santé micro (`[MIC_CAL] ...`).
- Makefile pour standardiser build/flash/monitor ESP32 + écran ESP8266.

## [0.1.0] - 2026-02-03
- Initialisation du dépôt et fichiers de gouvernance.
