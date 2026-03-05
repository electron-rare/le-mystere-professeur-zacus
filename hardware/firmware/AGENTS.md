
# Firmware Agent Contract

Purpose: enforce reproducible PlatformIO builds and strict smoke validation.


Scope: all files under `hardware/firmware/**`

**Note : Le Test & Script Coordinator est responsable de la mise à jour de ce contrat d’agent (et des fichiers associés) à chaque évolution des scripts, des politiques de test, ou de la structure. Toute modification doit être répercutée ici et dans la documentation pour garantir la cohérence, la traçabilité et la reproductibilité.**

---

**📌 For tooling-specific rules**, see [Agent Contract (tools/dev)](tools/dev/AGENTS.md).

---

Les agents peuvent modifier la structure (dossiers/deplacements/suppressions) si necessaire.
Toute modification structurelle doit etre repercutee dans la documentation et l'onboarding.


## Structure et scripts principaux

- Tous les utilitaires shell (menus, i18n, gates, helpers) sont centralisés dans `tools/dev/agent_utils.sh`.
- Le point d'entrée TUI unique est `tools/dev/cockpit.sh` (menu interactif, logs, prompts, etc.).
- Les scripts batch/CLI (build, flash, rc, etc.) sont accessibles via `tools/dev/cockpit.sh <commande>`.
- Les scripts de seed/CI sont dans `tools/dev/ci/`.
- Onboarding et vérification d'environnement : `tools/dev/onboard.sh`, `tools/dev/check_env.sh`.
- Les anciens fichiers `menu_utils.sh` et `menu_strings.sh` sont supprimés (fusion dans agent_utils.sh).

### [2026] Ajout cible Freenove ESP32-S3
- Un environnement PlatformIO dédié à la carte Freenove ESP32-S3 (`[env:freenove_esp32s3]`) doit être maintenu dans `platformio.ini`.
- Le mapping hardware (pins, UART, SPI, etc.) doit être strictement aligné entre `platformio.ini`, `ui_freenove_config.h` et la documentation (`RC_FINAL_BOARD.md`).
- Toute évolution de la cible ou du mapping doit être répercutée dans la doc et les scripts de build/test.


Bootstrap + workspace:

- `cd hardware/firmware`
- `./tools/dev/bootstrap_local.sh`
- use `.venv` for local python tooling (`pyserial`)


Build gates:

- `./build_all.sh`
- or `pio run -e freenove_esp32s3 -e esp8266_oled`


Smoke gates:

- `./tools/dev/run_matrix_and_smoke.sh`
- `./tools/dev/run_smoke_tests.sh` (dual-board) or `./tools/dev/run_smoke_tests.sh --combined-board` (Freenove)
- strict FAIL on panic/reboot markers
- UI verdict from ESP32 side: `UI_LINK_STATUS connected==1`


Port + baud policy:

- CP2102 mapping by LOCATION: `20-6.1.1=esp32`, `20-6.1.2=esp8266_usb`
- USB monitor baud is `115200` (esp32 + esp8266_usb monitor-only)
- internal ESP8266 SoftwareSerial UI link is `57600` (not USB monitor baud)


Fast loop:

- `make fast-esp32`, `make fast-ui-oled`, `make fast-ui-tft`, `make fast-freenove`
- monitor optionnel pour les boucles non interactives via `FAST_MONITOR=0` (`FAST_MONITOR=auto` par defaut)


Logs/artifacts:

- logs: `hardware/firmware/logs/`
- artifacts: `hardware/firmware/artifacts/`

## Bonnes pratiques agent

- Toute logique de menu, i18n, helpers shell, gates (build/smoke/logs/artefacts) doit passer par `agent_utils.sh`.
- Un seul point d'entrée TUI : cockpit.sh (pas de duplication de menu ailleurs).
- Scripts batch/CLI : cockpit.sh (usage non interactif, scripting, CI).
- Les scripts de seed/CI doivent être rangés dans `tools/dev/ci/` et référencés dans la doc/CI.
- Toute modification de structure doit être répercutée dans la documentation et l'onboarding.
- Les grosses reviews sont autorisees (rapport detaille + recommandations).
- Les gates build/smoke sont recommandees, mais non obligatoires sauf demande explicite.
- Consult et mettez à jour `docs/AGENT_TODO.md` avant toute opération importante : c’est le tracker canonique des tâches d’agent, mentionnez-y les étapes réalisées, les artefacts produits et toute impasse matérielle pour guider les agents suivants.
- Les logs et artefacts (`logs/`, `artifacts/`, etc.) ne doivent pas être committés ; notez leur existence (chemins, horodatages) dans `docs/AGENT_TODO.md` ou dans le rapport final au lieu de les versionner.

---

## 🔒 Verrous et exigences critiques (2026)

- **Cleanup commit & gestion artefacts** :
	- Toute action git (add, commit, stash, push) doit passer par `cockpit.sh git <action>` qui appelle `git_cmd` dans `agent_utils.sh` pour loguer l’évidence.
	- Les artefacts et logs doivent être produits dans `hardware/firmware/logs/` et `hardware/firmware/artifacts/`.
	- Les artefacts/logs ne sont jamais versionnés : seule l’évidence (chemin, timestamp, verdict) est tracée dans la doc.

- **UI link verdict** :
	- Le verdict de connexion UI (`UI_LINK_STATUS connected==1`) doit être strictement vérifié dans tous les scripts de test/smoke.
	- Exception explicite en mode carte combinée (`ZACUS_ENV=freenove_esp32s3`) : la gate UI link peut être `SKIP` uniquement avec la justification `not needed for combined board`.
	- Toute évolution du protocole UI link doit être documentée et testée (voir `UI_LINK_DEBUG_REPORT.md`).

- **Scénario par défaut LittleFS** :
	- Un scénario Story par défaut doit toujours être présent sur LittleFS (auto-création si absent, fallback robuste).
	- Les scripts de flash/test doivent vérifier la présence et la validité du scénario par défaut.

- **Stress test I2S (panic)** :
	- Les scripts de stress test doivent détecter tout panic I2S et produire un log d’évidence.
	- Toute occurrence de panic/reboot doit entraîner un FAIL strict dans les gates.

- **WebSocket health** :
	- Les scripts doivent vérifier la santé des WebSockets (watchdog, auto-recover si possible).
	- Toute perte de WebSocket doit être loguée et entraîner un verdict FAIL si non récupérée.

---

## Centralisation, robustesse, traçabilité

- Toute évolution de structure, de workflow, ou de protocole doit être documentée et synchronisée avec la doc d’onboarding.
- Les scripts doivent être compatibles bash strict (`set -euo pipefail`, variables initialisées, etc.).
- Les ports série doivent être résolus dynamiquement (pas de chemins hardcodés).
- Les scripts de test doivent être robustes, autonomes, et produire des logs d’évidence.
- Toute évolution doit être traçable, testée, et documentée.

---

## Rappel

**Ce contrat doit être mis à jour à chaque évolution des scripts, des politiques de test, ou de la structure.**
Tout agent doit s’y référer avant toute opération majeure.
