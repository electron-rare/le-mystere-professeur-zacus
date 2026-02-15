# Firmware Agent Contract

Purpose: enforce reproducible PlatformIO builds and strict smoke validation.

Scope:
- all files under `hardware/firmware/**`
- `hardware/firmware/esp32/` remains read-only

## Structure et scripts principaux

- Tous les utilitaires shell (menus, i18n, gates, helpers) sont centralisés dans `tools/dev/agent_utils.sh`.
- Le point d'entrée TUI unique est `tools/dev/cockpit.sh` (menu interactif, logs, prompts, etc.).
- Les scripts batch/CLI (build, flash, rc, etc.) sont accessibles via `tools/dev/zacus.sh`.
- Les scripts de seed/CI sont dans `tools/dev/ci/`.
- Onboarding et vérification d'environnement : `tools/dev/onboard.sh`, `tools/dev/check_env.sh`.
- Les anciens fichiers `menu_utils.sh` et `menu_strings.sh` sont supprimés (fusion dans agent_utils.sh).

Bootstrap + workspace:
- `cd hardware/firmware`
- `./tools/dev/bootstrap_local.sh`
- use `.venv` for local python tooling (`pyserial`)

Build gates:
- `./build_all.sh`
- or `pio run -e esp32dev esp32_release esp8266_oled ui_rp2040_ili9488 ui_rp2040_ili9486`

Smoke gates:
- `./tools/dev/run_matrix_and_smoke.sh`
- strict FAIL on panic/reboot markers
- UI verdict from ESP32 side: `UI_LINK_STATUS connected==1`

Port + baud policy:
- CP2102 mapping by LOCATION: `20-6.1.1=esp32`, `20-6.1.2=esp8266_usb`
- USB monitor baud is `115200` (esp32 + esp8266_usb monitor-only)
- internal ESP8266 SoftwareSerial UI link is `19200` (not USB monitor baud)

Fast loop:
- `make fast-esp32`, `make fast-ui-oled`, `make fast-ui-tft`

Logs/artifacts:
- logs: `hardware/firmware/logs/`
- artifacts: `hardware/firmware/artifacts/`

## Bonnes pratiques agent

- Toute logique de menu, i18n, helpers shell, gates (build/smoke/logs/artefacts) doit passer par `agent_utils.sh`.
- Un seul point d'entrée TUI : cockpit.sh (pas de duplication de menu ailleurs).
- Scripts batch/CLI : zacus.sh (usage non interactif, scripting, CI).
- Les scripts de seed/CI doivent être rangés dans `tools/dev/ci/` et référencés dans la doc/CI.
- Toute modification de structure doit être répercutée dans la documentation et l'onboarding.
