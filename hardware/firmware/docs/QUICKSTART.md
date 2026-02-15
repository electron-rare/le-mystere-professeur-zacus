# QUICKSTART Firmware

## 1) Flasher ESP32 audio

```sh
pio run -e esp32dev -t upload --upload-port <PORT_ESP32>
pio device monitor -e esp32dev --port <PORT_ESP32>
```

## 2) Choisir une UI

### UI ESP8266 OLED

```sh
pio run -e esp8266_oled -t upload --upload-port <PORT_ESP8266>
pio device monitor -e esp8266_oled --port <PORT_ESP8266>
```

### UI RP2040 TFT

```sh
pio run -e ui_rp2040_ili9488 -t upload --upload-port <PORT_RP2040>
pio device monitor -e ui_rp2040_ili9488 --port <PORT_RP2040>
```

## 3) Verifier le lien v2

Sur le moniteur ESP32:

```text
UI_LINK_STATUS
```

Attendu:
- `connected=1`
- compteur `pong` qui augmente

## 4) Boucle dev rapide (credit-friendly)

```sh
make fast-esp32 ESP32_PORT=<PORT_ESP32>
make fast-ui-oled UI_OLED_PORT=<PORT_ESP8266>
make fast-ui-tft UI_TFT_PORT=<PORT_RP2040>
python3 tools/dev/serial_smoke.py --role auto --wait-port 3 --allow-no-hardware
python3 tools/dev/serial_smoke.py --role all --wait-port 3 --allow-no-hardware
```

Sur macOS les deux CP2102 partagent VID/PID=10C4:EA60/0001; utilisez `LOCATION=20-6.1.1` pour l’ESP32 et `20-6.1.2` pour l’ESP8266. `tools/dev/ports_map.json` suit le format `location -> role` + `vidpid -> role`.

## 4.5) Build + smoke runner

```sh
./tools/dev/run_matrix_and_smoke.sh
```

Le script force `PLATFORMIO_CORE_DIR=$HOME/.platformio` pour que les caches PlatformIO restent en dehors du repo.

Par défaut, la séquence smoke tolère l’absence de matériel et termine avec un code 0 quand rien n’est détecté.

Variantes d'environnement :

- `ZACUS_REQUIRE_HW=1 ./tools/dev/run_matrix_and_smoke.sh` — échoue si aucun hardware détecté.
- `ZACUS_WAIT_PORT=3 ./tools/dev/run_matrix_and_smoke.sh` — réduit/ajuste la fenêtre d’attente smoke.
- `ZACUS_USB_COUNTDOWN=60 ./tools/dev/run_matrix_and_smoke.sh` — prolonge la remise USB.
- `ZACUS_NO_COUNTDOWN=1 ./tools/dev/run_matrix_and_smoke.sh` — saute le compte à rebours et la cloche.
- `ZACUS_SKIP_PIO=1 ./tools/dev/run_matrix_and_smoke.sh` — saute l’étape PlatformIO et ne lance que la smoke (utile quand les downloads sont impossibles).
- `ZACUS_SKIP_SMOKE=1 ./tools/dev/run_matrix_and_smoke.sh` — ne lance que la build matrix.
- `ZACUS_ENV="esp32dev esp8266_oled" ./tools/dev/run_matrix_and_smoke.sh` — cible un sous-ensemble d’environnements.
- `ZACUS_FORCE_BUILD=1 ./tools/dev/run_matrix_and_smoke.sh` — force la rebuild même si les artefacts existent déjà.

Le script affiche un résumé final : `Build status` (OK ou SKIPPED) et `Smoke status` (OK/SKIP) avec la commande exacte.

## 4.6) Local dev cockpit

1. Bootstrap the workspace once:
   ```sh
   ./tools/dev/bootstrap_local.sh
   ```
   This creates `.venv`, installs `pyserial`, and reminds you to manually warm PlatformIO caches (`PLATFORMIO_CORE_DIR="$HOME/.platformio"` + `pio platform install espressif32`) when network access is available.
2. Open VS Code from the repo root and use **Run Task**:
   - `List ports (15s, venv strict)` to list serial ports every 15s (fails clearly before `.venv` exists).
   - `Serial smoke (auto/skip)` to run `tools/dev/serial_smoke.py --role auto --baud 19200 --wait-port 3 --allow-no-hardware`.
   - `Matrix + smoke (one-shot)` to run `./tools/dev/run_matrix_and_smoke.sh`.
3. Thanks to the VS Code cockpit, `.pio`, `.platformio`, and `.venv` are hidden from explorer/search via workspace settings.

## 4.7) Smoke sanity checks (doc-only)

- **Tout déjà branché** (baseline non vide): smoke reuses the initial port list and prints `Using existing ports (already connected)`.
- **Hotplug**: unplug a board, run the countdown, plug it back before timeout → new port triggers detection.
- **Aucun hardware**: run smoke without devices; default behavior logs the skip, `ZACUS_REQUIRE_HW=1` forces failure, and `ZACUS_SKIP_PIO=1` still runs the skip-only sequence.

## 5) Hot-swap manuel

1. Demarrer ESP32 + OLED, verifier affichage.
2. Debrancher OLED.
3. Brancher TFT sans reboot ESP32.
4. Verifier resync UI (< 2s) et commandes BTN.
