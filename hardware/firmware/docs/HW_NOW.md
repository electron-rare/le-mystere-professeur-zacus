Run now
=======

1. Vérifiez que les deux boards sont câblées : ESP32 Audio Kit A252 et ESP8266 OLED (NodeMCU).
2. Lancez le script de validation canonique :  
   `bash tools/test/hw_now.sh` ou `bash tools/test/hw_now_esp32_esp8266.sh` si vous êtes déjà dans `hardware/firmware`.  
   Le script :
   - détecte automatiquement les ports série (avec `tools/dev/serial_smoke.py` pour la reconnaissance)
   - compile et upload les firmwares PlatformIO (`esp32dev` + `esp8266_oled`)
   - exécute des smokes `serial_smoke.py --role auto` puis AP fallback et collecte le verdict
   - stocke les traces dans `artifacts/rc_live/<TIMESTAMP>/` (avec `steps.tsv`) et `logs/hw_now_<TIMESTAMP>.log`.
3. À la fin, lisez les derniers blocs `[PASS]` / `[SKIP]` / `[fail]` dans le log pour détecter les échecs, puis consultez `steps.tsv` si nécessaire.
4. Si quelque chose échoue, relancez `pio device monitor -e <env>` sur le port correspondant (ESP32 ou ESP8266) pour récupérer les consoles : `pio device monitor -e <env> --port <PORT> --monitor-speed 19200`.

Câblage minimal
----------------

- Reliez les GND ensembles (ESP32 + ESP8266 + PC). Sans masse commune, les UARTs ne fonctionnent pas.
- Pour les UARTs de debug, utilisez la paire GPIO1 (TX0) / GPIO3 (RX0) de l’ESP32 et connectez-la au RX/TX du NodeMCU (cross TX/RX).
- Si vous préférez monitorer le lien UI avec GPIO22/19, gardez les fils séparés du bus de l’écran et connectez-les à un adaptateur USB-TTL.
- Une seule connexion par board suffit, le script n’a besoin que des port `/dev/cu.*` affectés automatiquement.

Commandes sans script
----------------------

1. Build + upload manuel :
   ```
   pio run -e esp32dev
   pio run -e esp8266_oled
   pio run -e esp32dev -t upload --upload-port <PORT_ESP32>
   pio run -e esp8266_oled -t upload --upload-port <PORT_ESP8266>
   ```
2. Smokes série :
   ```
   python3 tools/dev/serial_smoke.py --role esp32 --port <PORT_ESP32> --baud 19200 --wait-port 20
   python3 tools/dev/serial_smoke.py --role esp8266 --port <PORT_ESP8266> --baud 19200 --wait-port 20
   python3 tools/dev/serial_smoke.py --role auto --baud 19200 --wait-port 20
   ```
3. Indicateur UI link : ouvrez `pio device monitor` sur l’ESP32 et cherchez une ligne `UI_LINK` ou `connected=1`.
4. Status AP fallback : `curl --max-time 5 -sS http://192.168.4.1/api/status` (timeout court pour ne pas bloquer l’exécution).

Logs et artefacts
------------------

- Les runs `tools/test/hw_now.sh` et `hw_now_esp32_esp8266.sh` déposent les sorties dans `logs/hw_now_<TIMESTAMP>.log`.
- Le dossier `artifacts/rc_live/<TIMESTAMP>/` contient un `steps.tsv` et un `ports_resolve.json` détaillant les ports détectés.
- Conservez ces fichiers pour la traçabilité ou pour transmettre des logs à la QA / au RC gate.

Interpréter PASS / FAIL / SKIP
-------------------------------

- **PASS** : chaque étape attendue s’est terminée avec un `[ok]` dans `artifacts/hw_now/<TIMESTAMP>/hw_now.log`, la board répond au `PING`, l’AP fallback répond.
- **FAIL** : un `pio run`, un `upload`, ou l’un des `serial_smoke` retourne `[fail]`; l’étape correspondant remonte la ligne d’erreur dans le log.
- **SKIP** : seules les vérifications non critiques (ex. AP fallback absent) émettent un `SKIP`; cela ne bloque pas mais indique que la vérif n’a pas pu être faite. Toutes les étapes critiques échouent avec `FAIL`.
