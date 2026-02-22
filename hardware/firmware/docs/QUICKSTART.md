# QUICKSTART Firmware

## 1) Flasher ESP32 audio

```sh
pio run -e esp32dev -t upload --upload-port <PORT_ESP32>
pio device monitor -e esp32dev --port <PORT_ESP32>
```

## 1.5) Flash via cockpit (recommande)

Commande unique (auto-ports + logs + artefacts):

```sh
./tools/dev/cockpit.sh flash
```

Variables utiles (optionnelles):

- `ZACUS_FLASH_ESP32_ENVS="esp32dev esp32_release"`
- `ZACUS_FLASH_ESP8266_ENV="esp8266_oled"`
- `ZACUS_FLASH_RP2040_ENVS="ui_rp2040_ili9488 ui_rp2040_ili9486"`
- `ZACUS_REQUIRE_RP2040=1`
- `ZACUS_PORT_ESP32=/dev/cu.SLAB_USBtoUART`
- `ZACUS_PORT_ESP8266=/dev/cu.SLAB_USBtoUART7`
- `ZACUS_PORT_RP2040=/dev/cu.usbmodemXXXX`
- `ZACUS_PORT_WAIT=5`

Artefacts et logs:

- `artifacts/rc_live/flash-<timestamp>/ports_resolve.json`
- `logs/flash_<timestamp>.log`

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

## 3.5) Verification post-flash

- Le flash se termine sans erreur et PIO affiche "Success".
- Monitor ESP32 (115200) et verifier l'absence de `PANIC`/`REBOOT`.
- Verifier HTTP: `curl http://<ESP_IP>:8080/api/story/list`.
- Verifier UI link: `UI_LINK_STATUS connected=1`.

## 4) Boucle dev rapide (credit-friendly)

```sh
./tools/dev/bootstrap_local.sh
./tools/dev/run_matrix_and_smoke.sh
# depuis la racine du repo:
./hw_now.sh
make fast-esp32 ESP32_PORT=<PORT_ESP32>
make fast-ui-oled UI_OLED_PORT=<PORT_ESP8266>
make fast-ui-tft UI_TFT_PORT=<PORT_RP2040>
make fast-freenove FREENOVE_PORT=<PORT_FREENOVE> FAST_MONITOR=0
python3 tools/dev/serial_smoke.py --role auto --baud 115200 --wait-port 3 --allow-no-hardware
python3 tools/dev/serial_smoke.py --role all --baud 115200 --wait-port 3 --allow-no-hardware
```

Sur macOS les deux CP2102 partagent VID/PID=10C4:EA60/0001; utilisez `LOCATION=20-6.1.1` pour l’ESP32 et `20-6.1.2` pour l’ESP8266. `tools/dev/ports_map.json` suit le format `location -> role` + `vidpid -> role`.
Baud separation a retenir:
- console USB PlatformIO: `115200`
- lien interne ESP8266 SoftwareSerial vers ESP32: `57600`

## 4.5) Build + smoke runner

```sh
./tools/dev/run_matrix_and_smoke.sh
./tools/dev/run_smoke_tests.sh --combined-board
```

Cockpit equivalent:

```sh
./tools/dev/cockpit.sh rc
```

Le script force `PLATFORMIO_CORE_DIR=$HOME/.platformio` pour que les caches PlatformIO restent en dehors du repo.
Avant le smoke, il affiche `⚠️ BRANCHE L’USB MAINTENANT ⚠️` 3 fois, puis attend Enter en listant les ports toutes les 15s.
Chaque run dépose `meta.json`, `commands.txt`, `summary.json`, `summary.md`, `ports_resolve.json` et `ui_link.log` dans `artifacts/rc_live/<env_label>_<timestamp>/`.
Le log global est `logs/rc_live/<env_label>_<timestamp>.log`.
Le verdict UI link est strict: `UI_LINK_STATUS connected=1` attendu sur l'ESP32.
Pour `ZACUS_ENV="freenove_esp32s3"` (board combinée), les checks ESP8266/UI-link/story-screen sont marqués `SKIP` avec `not needed for combined board`.

Par défaut, la séquence smoke tolère l’absence de matériel et termine avec un code 0 quand rien n’est détecté.

Variantes d'environnement :

- `ZACUS_REQUIRE_HW=1 ./tools/dev/run_matrix_and_smoke.sh` — échoue si aucun hardware détecté.
- `ZACUS_WAIT_PORT=3 ./tools/dev/run_matrix_and_smoke.sh` — réduit/ajuste la fenêtre d’attente smoke.
- `ZACUS_NO_COUNTDOWN=1 ./tools/dev/run_matrix_and_smoke.sh` — saute la gate USB (alertes + attente Enter).
- `ZACUS_SKIP_PIO=1 ./tools/dev/run_matrix_and_smoke.sh` — saute l’étape PlatformIO et ne lance que la smoke (utile quand les downloads sont impossibles).
- `ZACUS_SKIP_SMOKE=1 ./tools/dev/run_matrix_and_smoke.sh` — ne lance que la build matrix.
- `ZACUS_ENV="esp32dev esp8266_oled" ./tools/dev/run_matrix_and_smoke.sh` — cible un sous-ensemble d’environnements.
- `ZACUS_ENV="freenove_esp32s3" ./tools/dev/run_matrix_and_smoke.sh` — workflow Freenove mono-carte.
- `ZACUS_FORCE_BUILD=1 ./tools/dev/run_matrix_and_smoke.sh` — force la rebuild même si les artefacts existent déjà.

Le script affiche un résumé final (`Build/Port/Smoke/UI link`) et écrit le même verdict dans `artifacts/rc_live/<env_label>_<timestamp>/summary.json`.

## 4.10) Story generation + protocol V3

Génération (host-side):

```sh
./tools/dev/story-gen validate
./tools/dev/story-gen generate-cpp
./tools/dev/story-gen generate-bundle
```

Protocole série Story V3 (JSON-lines):

```text
{"cmd":"story.status"}
{"cmd":"story.list"}
{"cmd":"story.load","data":{"scenario":"DEFAULT"}}
{"cmd":"story.validate"}
```

Référence protocole: `docs/protocols/story_v3_serial.md`.

Standard evidence layout (all gates):

- `artifacts/<phase>/<timestamp>/meta.json`
- `artifacts/<phase>/<timestamp>/git.txt`
- `artifacts/<phase>/<timestamp>/commands.txt`
- `artifacts/<phase>/<timestamp>/summary.md`

Override output:

- `--outdir <path>` or `ZACUS_OUTDIR=<path>`

## 4.6) Local dev cockpit

1. Bootstrap the workspace once:
  ```sh
  ./tools/dev/bootstrap_local.sh
  ```
   This creates `.venv`, installs `pyserial`, and reminds you to manually warm PlatformIO caches (`PLATFORMIO_CORE_DIR="$HOME/.platformio"` + `pio platform install espressif32`) when network access is available.
2. Open VS Code from the repo root and use **Run Task**:
   - `List ports (15s, venv strict)` to list serial ports every 15s (fails clearly before `.venv` exists).
   - `Serial smoke (auto/skip)` to run `tools/dev/serial_smoke.py --role auto --baud 115200 --wait-port 3 --allow-no-hardware`.
   - `Matrix + smoke (one-shot)` to run `./tools/dev/run_matrix_and_smoke.sh`.
3. Full cockpit registry lives in `docs/_generated/COCKPIT_COMMANDS.md`.
3. Thanks to the VS Code cockpit, `.pio`, `.platformio`, and `.venv` are hidden from explorer/search via workspace settings.

## 4.7) Smoke sanity checks (doc-only)

- **Tout déjà branché** (baseline non vide): smoke reuses the initial port list and prints `Using existing ports (already connected)`.
- **Hotplug**: unplug a board, run the USB wait gate, plug it back before confirmation → new port triggers detection.
- **Aucun hardware**: run smoke without devices; default behavior logs the skip, `ZACUS_REQUIRE_HW=1` forces failure, and `ZACUS_SKIP_PIO=1` still runs the skip-only sequence.

## 4.8) RC sprint gates

- Sprint replay helper:
  - `bash ../../tools/test/run_rc_gate.sh --sprint s1 --allow-no-hardware`
  - `bash ../../tools/test/run_rc_gate.sh --sprint s5 --esp32-port <PORT_ESP32> --ui-port <PORT_UI>`
- Board source:
  - `docs/RC_FINAL_BOARD.md`
- Final report template:
  - `docs/RC_FINAL_REPORT_TEMPLATE.md`
- Optional issue/label seed:
  - `bash tools/dev/ci/rc_execution_seed.sh`

## 4.9) RTOS + WiFi health

Snapshot HTTP + RTOS:

```sh
ESP_URL=http://<ip-esp32>:8080 ./tools/dev/rtos_wifi_health.sh
```

Artefact genere:

Commande serie:

```text
SYS_RTOS_STATUS
```

## 5) Hot-swap manuel

1. Demarrer ESP32 + OLED, verifier affichage.
2. Debrancher OLED.
3. Brancher TFT sans reboot ESP32.
4. Verifier resync UI (< 2s) et commandes BTN.

---
### Audio sur ESP32-S3 (Freenove)

- L’ESP32-S3 ne possède pas de DAC intégré: la sortie audio passe par l’I2S.
- Mapping I2S out Freenove: `BCK=42`, `WS=41`, `DOUT=1` (`FREENOVE_I2S_BCK`, `FREENOVE_I2S_WS`, `FREENOVE_I2S_DOUT`).
- Mapping micro I2S in: `SCK=3`, `WS=14`, `DIN=46` (`FREENOVE_I2S_IN_SCK`, `FREENOVE_I2S_IN_WS`, `FREENOVE_I2S_IN_DIN`).
- Tester avec `MIC_TUNER_STATUS ON 200` et `HW_STATUS_JSON` pour vérifier fréquence/confiance/niveau.
- En cas d’échec, consigner l’evidence (artefact + log + commande) dans `docs/AGENT_TODO.md`.
