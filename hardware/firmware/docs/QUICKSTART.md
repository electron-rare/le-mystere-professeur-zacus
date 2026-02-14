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
python3 tools/dev/serial_smoke.py --role auto --wait-port 180
```

Sur macOS les deux CP2102 partagent VID/PID=10C4:EA60/0001; utilisez `LOCATION=20-6.1.1` pour l’ESP32 et `20-6.1.2` pour l’ESP8266. Mettez à jour `tools/dev/ports_map.json` si votre configuration USB change et `/dev/cu.SLAB_*` sera privilégié automatiquement.

## 4.5) Build + smoke runner

```sh
./tools/dev/run_matrix_and_smoke.sh
```

Le script force `PLATFORMIO_CORE_DIR=$HOME/.platformio` pour que les caches PlatformIO restent en dehors du repo.

Par défaut, la séquence smoke tolère l’absence de matériel et termine avec un code 0 quand rien n’est détecté.

Variantes d'environnement :

- `ZACUS_REQUIRE_HW=1 ./tools/dev/run_matrix_and_smoke.sh` — échoue si aucun hardware détecté.
- `ZACUS_USB_COUNTDOWN=60 ./tools/dev/run_matrix_and_smoke.sh` — prolonge la remise USB.
- `ZACUS_NO_COUNTDOWN=1 ./tools/dev/run_matrix_and_smoke.sh` — saute le compte à rebours et la cloche.

## 4.6) Local dev cockpit

1. Bootstrap the workspace once:
   ```sh
   ./tools/dev/bootstrap_local.sh
   ```
   This creates `.venv`, installs `pyserial`, and reminds you to manually warm PlatformIO caches (`PLATFORMIO_CORE_DIR="$HOME/.platformio"` + `pio platform install espressif32`) when network access is available.
2. Open VS Code from the repo root and use **Run Task**:
   - `Ports watch (venv)` to list serial ports every 15s (fails clearly before `.venv` exists).
   - `Git watch` to keep `git status -sb` + `git diff --stat=25` refreshed.
   - `Build firmware (one-shot)` for a single `./build_all.sh` run (exports `PLATFORMIO_CORE_DIR="$HOME/.platformio"` first).
   - `Serial smoke (one-shot)` to run `tools/dev/serial_smoke.py --role auto --baud 19200 --wait-port 3 --allow-no-hardware` (set `ZACUS_REQUIRE_HW=1` and `--wait-port 180` manually if you need strict hardware detection).
3. Thanks to the VS Code cockpit, `.pio`, `.platformio`, and `.venv` are hidden from explorer/search via workspace settings.

## 5) Hot-swap manuel

1. Demarrer ESP32 + OLED, verifier affichage.
2. Debrancher OLED.
3. Brancher TFT sans reboot ESP32.
4. Verifier resync UI (< 2s) et commandes BTN.
