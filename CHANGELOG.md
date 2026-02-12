# Changelog

## [Unreleased]
- Documentation : revue de cohérence étendue sur les Markdown/README et mise à jour de `docs/repo-audit.md`.
- Structure : ajout du dossier `kit-maitre-du-jeu/export/pdf/` pour aligner la documentation avec l'arborescence.
- Printables : clarification de l'état actuel et du niveau de préparation des dossiers `src/` et `export/{pdf,png}/`.

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
