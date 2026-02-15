# Hardware Firmware

Workspace PlatformIO unique pour 3 firmwares:

- `esp32_audio/`: firmware principal audio
- `ui/esp8266_oled/`: UI OLED legere
- `ui/rp2040_tft/`: UI TFT tactile (LVGL)
- `protocol/`: contrat UART partage (`ui_link_v2.md`, `ui_link_v2.h`)

## 📚 Documentation

**Nouvelle documentation complète disponible :**

- **[Index Documentation](docs/INDEX.md)** - Point d'entrée navigation complète
- **[Architecture UML](docs/ARCHITECTURE_UML.md)** - Diagrammes classes, séquence, composants
- **[État des lieux](docs/STATE_ANALYSIS.md)** - Analyse complète du firmware
- **[Recommandations Sprint](docs/SPRINT_RECOMMENDATIONS.md)** - Actions prioritaires

Pour débuter : [docs/QUICKSTART.md](docs/QUICKSTART.md)

## Build

```sh
pio run
```

Build cible:

```sh
pio run -e esp32dev
pio run -e esp32_release
pio run -e esp8266_oled
pio run -e ui_rp2040_ili9488
pio run -e ui_rp2040_ili9486
```

Script global:

```sh
./build_all.sh
```

Bootstrap local tools once:

```sh
./tools/dev/bootstrap_local.sh
```

## Boucle dev rapide

```sh
make fast-esp32 ESP32_PORT=<PORT_ESP32>
make fast-ui-oled UI_OLED_PORT=<PORT_ESP8266>
make fast-ui-tft UI_TFT_PORT=<PORT_RP2040>
```

Smoke série (manuel):

```sh
python3 tools/dev/serial_smoke.py --role auto --baud 115200 --wait-port 3 --allow-no-hardware
```

MacOS CP2102 duplicates share VID/PID=10C4:EA60/0001; the LOCATION (20-6.1.1=ESP32, 20-6.1.2=ESP8266) drives the detector. `tools/dev/ports_map.json` now uses `location -> role` and `vidpid -> role` mappings.
USB console monitoring uses `115200`. ESP8266 internal UI link SoftwareSerial stays at `19200` (internal link only).

## Serial smoke commands

- baseline smoke (auto handles already connected boards): `python3 tools/dev/serial_smoke.py --role auto --baud 115200 --wait-port 3 --allow-no-hardware`
- run every detected role: `python3 tools/dev/serial_smoke.py --role all --baud 115200 --wait-port 3 --allow-no-hardware`
- force hardware detection: `ZACUS_REQUIRE_HW=1 python3 tools/dev/serial_smoke.py --role auto --baud 115200 --wait-port 180`
- skip PlatformIO builds and just run smoke (useful when downloads are impossible): `ZACUS_SKIP_PIO=1 ./tools/dev/run_matrix_and_smoke.sh`

## Build + smoke combo

```sh
./tools/dev/run_matrix_and_smoke.sh
# or from repo root:
./hw_now.sh
```

`run_matrix_and_smoke.sh` ensures PlatformIO caches land under `$HOME/.platformio` (via `PLATFORMIO_CORE_DIR`) rather than inside the repo.
Before smoke it shows `⚠️ BRANCHE L’USB MAINTENANT ⚠️` three times, then waits for Enter while listing ports every 15s.
Each run writes deterministic artifacts under `artifacts/rc_live/<timestamp>/` (`summary.json`, `summary.md`, `ports_resolve.json`, `ui_link.log`, per-step logs).
The runner resolves macOS CP2102 by LOCATION (`20-6.1.1` ESP32, `20-6.1.2` ESP8266 USB), then enforces a dedicated `UI_LINK_STATUS connected=1` gate on ESP32.

Environment overrides:

- `ZACUS_REQUIRE_HW=1 ./tools/dev/run_matrix_and_smoke.sh` (fail when no hardware).
- `ZACUS_WAIT_PORT=3 ./tools/dev/run_matrix_and_smoke.sh` (override serial wait window for smoke).
- `ZACUS_NO_COUNTDOWN=1 ./tools/dev/run_matrix_and_smoke.sh` (skip the USB wait gate).
- `ZACUS_SKIP_SMOKE=1 ./tools/dev/run_matrix_and_smoke.sh` (build only, no serial smoke step).
- `ZACUS_ENV="esp32dev esp8266_oled" ./tools/dev/run_matrix_and_smoke.sh` (custom env subset).
- `ZACUS_FORCE_BUILD=1 ./tools/dev/run_matrix_and_smoke.sh` (force rebuild even when artifacts exist).

By default the smoke step exits 0 when no serial hardware is present; use `ZACUS_REQUIRE_HW=1` to enforce detection.

## Docs

- Cablage ESP32/UI: `esp32_audio/WIRING.md`
- Cablage TFT: `ui/rp2040_tft/WIRING.md`
- Quickstart flash: `docs/QUICKSTART.md`
- RC board execution: `docs/RC_FINAL_BOARD.md`
- Protocole: `protocol/ui_link_v2.md`

## Codex prompts

Prompt files live under `tools/dev/codex_prompts/*.prompt.md` and are designed to be consumed by the automation-friendly `codex exec` command.
Run `./tools/dev/codex_prompt_menu.sh` to see a numbered menu, pick a prompt, and send it to `codex exec --sandbox workspace-write --output-last-message artifacts/rc_live/_codex_last_message.md`.
You can also launch this helper directly from the firmware cockpit (`./tools/dev/cockpit.sh` option 6) so the existing workflow keeps a single entry point.
