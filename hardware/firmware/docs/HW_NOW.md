Run now
=======

1. Depuis la racine du repo, lancez `./hw_now.sh`.
   - équivalent depuis `hardware/firmware`: `./tools/test/hw_now.sh`
2. Le runner canonique est `tools/dev/run_matrix_and_smoke.sh`:
   - build matrix PlatformIO (ou skip si artefacts déjà présents)
   - gate USB locale: `⚠️ BRANCHE L’USB MAINTENANT ⚠️` x3 + attente Enter + listing ports toutes les 15s
   - résolution de ports via `tools/test/resolve_ports.py` (macOS CP2102: `20-6.1.1=esp32`, `20-6.1.2=esp8266_usb`)
   - smoke strict USB à `115200` (ESP32 + ESP8266 monitor-only)
   - gate UI link dédiée: commande ESP32 `UI_LINK_STATUS`, attendu `connected=1`
3. Les logs/artifacts sont toujours produits, même en FAIL:
   - `artifacts/rc_live/<TIMESTAMP>/summary.json`
   - `artifacts/rc_live/<TIMESTAMP>/summary.md`
   - `artifacts/rc_live/<TIMESTAMP>/ports_resolve.json`
   - `artifacts/rc_live/<TIMESTAMP>/ui_link.log`
   - `logs/run_matrix_and_smoke_<TIMESTAMP>.log`
4. Politique de verdict:
   - `PASS`: toutes les étapes critiques passent
   - `FAIL`: port unresolved, panic/reboot/binary junk, smoke fail, ou `UI_LINK_STATUS connected=0`
   - `SKIP`: aucune carte détectée et mode non strict (pas de faux PASS)

Rappels baud
------------

- Console USB PlatformIO/monitor: `115200`
- Lien interne ESP8266 SoftwareSerial vers ESP32: `57600` (non utilisé pour le monitor USB)

Diagnostic rapide
-----------------

- Vérifier les consoles USB:
  - `pio device monitor -e esp32dev --port <PORT_ESP32> --monitor-speed 115200`
  - `pio device monitor -e esp8266_oled --port <PORT_ESP8266> --monitor-speed 115200`
- Forcer l'échec sans hardware (QA stricte):
  - `ZACUS_REQUIRE_HW=1 ./tools/dev/run_matrix_and_smoke.sh`
