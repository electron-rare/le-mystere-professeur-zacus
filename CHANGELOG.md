# Changelog

## [Unreleased]
- Kit MJ : script minute-par-minute, solution complète, checklist matériel, plan des stations, guide anti-chaos et distribution des rôles prêts à l’usage pour 6–14 enfants pendant 60–90 minutes.
- Printables : structure `src/` + `export/{pdf,png}/` maintenue, prompts graphiques dans `src/prompts/`, workflow documenté dans `WORKFLOW.md`, et réorganisation des aperçus (`printables/export/png/{general,fiche-enquete,personnages,zones}`).
- Game & audio : scénario canon `game/scenarios/zacus_v1.yaml` (canon, solution unique), manifestes audio `audio/manifests/zacus_v1_audio.yaml` et prompts narratifs (`game/prompts/audio/intro.md`, `incident.md`, `accusation.md`, `solution.md`).
- Outils : scripts de validation `tools/scenario/validate_scenario.py` et `tools/audio/validate_manifest.py` pour sécuriser chaque nouvelle piste narrative ou audio.
- Documentation & guides : `docs/QUICKSTART.md`, `docs/STYLEGUIDE.md`, `docs/index.md`, `docs/repo-status.md`, et `printables/WORKFLOW.md` donnent un chemin clair pour déployer le kit, rédiger les scénarios et maintenir la cohérence.
- Licences : texte homogène CC BY-NC 4.0 pour les contenus créatifs et MIT pour les codes, avec les fichiers associés (`LICENSES/CC-BY-NC-4.0.txt`, `LICENSES/MIT.txt`), et `README.md` + `CONTRIBUTING.md` + `LICENSE.md` alignés sur cette répartition.
- Références annexes : `include-humain-IA/` (et `include-humain-IA/version-finale/`) renommés pour la portabilité, et la section `docs/repo-status.md` mise à jour pour documenter ces changements.

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
