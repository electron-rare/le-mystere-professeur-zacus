### Procédure smoke test Freenove

1. Vérifier la configuration du port série (résolution dynamique via cockpit.sh ports).
2. Flasher la cible Freenove avec cockpit.sh flash (ou build_all.sh).
3. Lancer le smoke test avec tools/dev/run_matrix_and_smoke.sh.
4. Vérifier le verdict UI_LINK_STATUS connected==1 (fail strict si absent).
5. Vérifier l’absence de panic/reboot dans les logs.
6. Vérifier la santé des WebSockets (logs, auto-recover).
7. Consigner les logs et artefacts produits (logs/rc_live/freenove_esp32s3_YYYYMMDD.log, artifacts/rc_live/freenove_esp32s3_YYYYMMDD.html).
8. Documenter toute anomalie ou fail dans AGENT_TODO.md.

## [2026-02-21] Firmware workflow hardening (Codex)

- PR de correction et merge: `feat/fix-firmware-story-workflow` → PR #105 → squash merge (main).
- Fichier touché: `.github/workflows/firmware-story-v2.yml`
- Correctifs:
  - indentation YAML corrigée sur `concurrency.cancel-in-progress` (anciennement bloquant l’exécution du workflow).
  - correction du répertoire de travail du job `story-toolchain` (`hardware/firmware` au lieu de `hardware/firmware/esp32_audio` inexistant).
  - alignement des artefacts de job `story-toolchain` vers les chemins existants.
- Validation:
  - PR checks: `story toolchain`, `validate`, `build env` (esp32dev/esp32_release/esp8266_oled/ui_rp2040_*) pass.
  - PR #105 mergeée (commit `04e4e50`) avec workflow OK.

## [2026-02-21] Coordination Orchestrator binôme (Kill_LIFE + Zacus + RTC)

- PR Kill_LIFE `#11` a été vérifiée et validée manuellement (`scope`):
  - scripts/tools d'orchestration (`tools/ai/zeroclaw_*`, y compris `zeroclaw_stack_up.sh`, `zeroclaw_hw_firmware_loop.sh`, `zeroclaw_watch_1min.sh`);
  - docs techniques `specs/*`.
- Vérification stricte:
  - `gh pr checks 11 --repo electron-rare/Kill_LIFE` : tous ✅ (`api_contract`, `lint-and-contract`, `ZeroClaw Dual Orchestrator`, etc.).
  - aucun artefact hors-scope ajouté dans le PR.
- Merge status:
  - PR fermé via squash (commit `e8e44048a8b36b7debcb12788183b34045dde7f2`) sur `main`.
  - commentaire de clôture codex publié (PASS) dans PR #11.
- Correctif suite: PR Kill_LIFE `#12` créé pour durcir le fallback mismatch:
  - `tools/ai/zeroclaw_hw_firmware_loop.sh` vérifie maintenant la stabilité du port USB avant fallback env.
  - merge PR #12 (squash) validé, commit `5f517ae82b95c1988d6c07e888194dcc28e02ff3`.
- Coordination inter-repos:
  - PR Zacus déjà alignée côté AP/ESP-NOW/WebUI (PRs/faits et mergeés précédemment dans ce chantier).
  - `RTC_BL_PHONE`: aucun PR ouvert en cours au moment du contrôle (`gh pr list --state open` vide).
- Actions de traçabilité:
  - pas de dette bloquante détectée.
  - prochaine étape: continuer surveillance post-merge (runbook ZeroClaw/CI + vérification liaison `watcher`).

## [2026-02-21] Freenove AP fallback dédié (anti-oscillation hors Les cils) (Codex)

- Contexte coordination merge train: PR Zacus #101 mergée, puis patch dédié AP fallback.
- Patch runtime réseau:
  - séparation SSID local vs AP fallback (`local_ssid=Les cils`, `ap_default_ssid=Freenove-Setup`)
  - nouveau flag config: `pause_local_retry_when_ap_client`
  - retry STA vers `Les cils` mis en pause si client connecté à l'AP fallback
  - nouveaux indicateurs exposés:
    - série `NET_STATUS`: `ap_clients`, `local_retry_paused`
    - WebUI `/api/status` + `/api/network/wifi`: `ap_clients`, `local_retry_paused`
- Fichiers touchés:
  - `data/story/apps/APP_WIFI.json`
  - `hardware/firmware/ui_freenove_allinone/include/network_manager.h`
  - `hardware/firmware/ui_freenove_allinone/src/network_manager.cpp`
  - `hardware/firmware/ui_freenove_allinone/src/main.cpp`
- Notes:
  - pas de constantes hardcodées nouvelles pour credentials AP en dehors de la config runtime/littlefs
  - comportement historique conservé si `pause_local_retry_when_ap_client=false`.
  - validations exécutées (PIO only):
    - `pio run -e freenove_esp32s3_full_with_ui` ✅
    - `pio run -e freenove_esp32s3_full_with_ui -t buildfs` ✅
    - `pio run -e freenove_esp32s3_full_with_ui -t uploadfs --upload-port /dev/cu.usbmodem5AB90753301` ✅
    - `pio run -e freenove_esp32s3_full_with_ui -t upload --upload-port /dev/cu.usbmodem5AB90753301` ✅
    - série `NET_STATUS`, `SC_REVALIDATE`, `SC_REVALIDATE_ALL`, `ESPNOW_STATUS_JSON` ✅
    - WebUI `GET /api/status`, `GET /api/network/wifi` (champs `ap_clients`, `local_retry_paused`) ✅
  - limite de validation:
    - le cas `local_retry_paused=1` nécessite un client connecté sur l'AP fallback pendant l'absence de `Les cils` (non reproduit sur ce run).

## [2026-02-21] Freenove story/ui/network pass final + revalidate step(x) (Codex)

- Checkpoint sécurité exécuté:
  - branche: `feat/freenove-webui-network-ops-parity`
  - patch/status: `/tmp/zacus_checkpoint/20260221_021417_wip.patch`, `/tmp/zacus_checkpoint/20260221_021417_status.txt`
  - scan artefacts trackés (`.pio`, `.platformio`, `logs`, `dist`, `build`, `node_modules`, `.venv`): aucun.
- Correctif incohérence revalidate:
  - `SC_REVALIDATE_STEP2` renommé en `SC_REVALIDATE_STEPX`
  - logs enrichis avec `anchor_step=<step courant>` pour éviter l'ambiguïté multi-scénarios.
- Validations PIO:
  - `pio run -e freenove_esp32s3_full_with_ui` PASS (x2)
  - `pio run -e freenove_esp32s3_full_with_ui -t buildfs` PASS
  - `pio run -e freenove_esp32s3_full_with_ui -t uploadfs --upload-port /dev/cu.usbmodem5AB90753301` PASS
  - `pio run -e freenove_esp32s3_full_with_ui -t upload --upload-port /dev/cu.usbmodem5AB90753301` PASS (x2)
- Evidence série (`pio device monitor --port /dev/cu.usbmodem5AB90753301 --baud 115200`):
  - `SC_COVERAGE scenario=DEFAULT unlock=1 audio_done=1 timer=1 serial=1 action=1`
  - `SC_REVALIDATE` OK avec `SC_REVALIDATE_STEPX ... anchor_step=STEP_WAIT_ETAPE2 ... step_after=STEP_ETAPE2`
  - `ESPNOW_STATUS_JSON` OK (`ready=true`, compteurs cohérents)
  - `ESPNOW_SEND broadcast ping` OK (`ACK ... ok=1`)
  - `NET_STATUS` local connecté validé (`sta_ssid=Les cils`, `local_match=1`, `fallback_ap=0`).
- Evidence WebUI:
  - `GET /api/status`, `GET /api/network/wifi`, `GET /api/network/espnow` OK
  - `POST /api/control` avec `SC_EVENT unlock UNLOCK` OK (`ok=true` + transition observée en série).

## [2026-02-20] GH binôme Freenove AP local + alignement ESP-NOW RTC (Codex)

- Branche de travail: `feat/freenove-ap-local-espnow-rtc-sync`
- Coordination GitHub:
  - issue firmware: https://github.com/electron-rare/le-mystere-professeur-zacus/issues/91
  - issue RTC (binôme, branche `audit/telephony-webserver`): https://github.com/electron-rare/RTC_BL_PHONE/issues/6
  - PR firmware: https://github.com/electron-rare/le-mystere-professeur-zacus/pull/92
- Runtime Freenove mis à jour:
  - AP fallback piloté par cible locale (`Les cils`) + retry périodique (`local_retry_ms`)
  - règle appliquée: AP actif si aucun WiFi connu n'est connecté (fallback), AP coupé quand la STA est reconnectée à `Les cils`
  - indicateurs réseau série ajoutés: `local_target`, `local_match`
  - commande série ajoutée: `ESPNOW_STATUS_JSON` (format RTC: ready/peer_count/tx_ok/tx_fail/rx_count/last_rx_mac/peers)
  - bootstrap peers ESP-NOW au boot via `APP_ESPNOW.config.peers`
  - WebUI embarquée (`http://<ip>/`) avec endpoints:
    - `GET /api/status`
    - `POST /api/wifi/connect`, `POST /api/wifi/disconnect`
    - `POST /api/espnow/send`
    - `POST /api/scenario/unlock`, `POST /api/scenario/next`
    - endpoints alignés RTC:
      - `GET /api/network/wifi`
      - `GET /api/network/espnow`
      - `GET/POST/DELETE /api/network/espnow/peer`
      - `POST /api/network/espnow/send`
      - `POST /api/network/wifi/connect`, `POST /api/network/wifi/disconnect`
      - `POST /api/control` (dispatch d'actions)
  - correction WebUI:
    - `WIFI_DISCONNECT` est maintenant différé (~250 ms) pour laisser la réponse HTTP sortir avant la coupure STA
  - Data story apps mises à jour:
    - `data/story/apps/APP_WIFI.json`: `local_ssid`, `local_password`, `ap_policy=if_no_known_wifi`, `local_retry_ms`
    - `data/story/apps/APP_ESPNOW.json`: `peers` + contrat payload enrichi
- Validations exécutées (PIO only):
  - `pio run -e freenove_esp32s3_full_with_ui` PASS
  - `pio run -e freenove_esp32s3_full_with_ui -t buildfs` PASS
  - `pio run -e freenove_esp32s3_full_with_ui -t uploadfs --upload-port /dev/cu.usbmodem5AB90753301` PASS
  - `pio run -e freenove_esp32s3_full_with_ui -t upload --upload-port /dev/cu.usbmodem5AB90753301` PASS
  - monitor série (`pio device monitor --port /dev/cu.usbmodem5AB90753301 --baud 115200`):
    - boot config: `local=Les cils ... ap_policy=0 retry_ms=15000`
    - `NET_STATUS ... local_target=Les cils local_match=1 ... fallback_ap=0` (local connecté)
    - `ESPNOW_STATUS_JSON` OK
    - `WIFI_DISCONNECT` => `fallback_ap=1` puis retry local
    - après reconnect WiFi: `ESPNOW_SEND broadcast ping` => recovery auto ESP-NOW + `ACK ... ok=1`
  - WebUI:
    - `GET /api/status` OK (`network/local_match`, `espnow`, `story`, `audio`)
    - `POST /api/scenario/unlock` et `POST /api/scenario/next` OK (transitions observées)
    - `POST /api/wifi/connect` OK
    - `POST /api/network/wifi/disconnect` => réponse HTTP `200` reçue avant coupure STA (plus de timeout systématique)
    - `POST /api/network/espnow/send` (payload JSON) OK
    - `POST /api/control` (`SC_EVENT unlock`, `WIFI_DISCONNECT`) OK
- Note d'incohérence traitée:
  - si AP fallback et cible locale partagent le même SSID (`Les cils`), le retry local coupe brièvement l'AP fallback pour éviter l'auto-association.

## [2026-02-21] Freenove WebUI parity réseau avec RTC (Codex)

- Branche de travail: `feat/freenove-webui-network-ops-parity`
- Tracking GitHub:
  - issue firmware: https://github.com/electron-rare/le-mystere-professeur-zacus/issues/93
  - coordination RTC: https://github.com/electron-rare/RTC_BL_PHONE/issues/6
  - PR RTC liée (ops web): https://github.com/electron-rare/RTC_BL_PHONE/pull/8
- Runtime WebUI enrichi (parité endpoints réseau):
  - `POST /api/network/wifi/reconnect`
  - `POST /api/network/espnow/on`
  - `POST /api/network/espnow/off`
  - routes existantes conservées (`/api/status`, `/api/network/wifi`, `/api/network/espnow`, `/api/network/espnow/peer`, `/api/network/espnow/send`, `/api/control`)
- Validation exécutée:
  - `pio run -e freenove_esp32s3_full_with_ui` PASS
  - `pio run -e freenove_esp32s3_full_with_ui -t upload --upload-port /dev/cu.usbmodem5AB90753301` PASS
  - HTTP (`curl`):
    - `POST /api/network/espnow/off` => `{"action":"ESPNOW_OFF","ok":true}`
    - `POST /api/network/espnow/on` => `{"action":"ESPNOW_ON","ok":true}`
    - `POST /api/network/wifi/reconnect` => `{"action":"WIFI_RECONNECT","ok":true}`
    - `POST /api/control` (`ESPNOW_OFF`, `ESPNOW_ON`, `WIFI_RECONNECT`) => `ok=true`
  - monitor série (`pio device monitor --port /dev/cu.usbmodem5AB90753301 --baud 115200`):
    - `NET_STATUS` cohérent (`sta_ssid=Les cils`, `fallback_ap=0`)
    - `ESPNOW_STATUS_JSON` cohérent (`ready=1`)
    - logs observés: `[NET] ESP-NOW off` puis `[NET] ESP-NOW ready`

## [2026-02-20] Freenove WiFi/AP fallback local + ESP-NOW bridge/story + UI FX (Codex)

- Exigence appliquée: l'AP ne sert qu'en fallback si la carte n'est pas sur le WiFi local (`Les cils`).
- Runtime modifié:
  - boot auto-connect vers `Les cils`/`mascarade`
  - AP fallback auto uniquement en absence de connexion STA (et stop auto quand STA connecté)
  - commandes série enrichies: `WIFI_CONNECT`, `WIFI_DISCONNECT`, `ESPNOW_PEER_ADD/DEL/LIST`, `ESPNOW_SEND <mac|broadcast>`
  - bridge ESP-NOW -> events scénario durci (texte + JSON `cmd/raw/event/event_type/event_name`)
- Story/UI:
  - intégration `APP_WIFI` + `APP_ESPNOW` dans les scénarios/story specs YAML
  - génération C++ story régénérée (`spec_hash=1834bcdab734`)
  - écran Freenove en mode effect-first (symboles/effets, titres cachés par défaut, vitesses d'effet pilotables par payload JSON)
- Vérifications exécutées:
  - `./tools/dev/story-gen validate` PASS
  - `./tools/dev/story-gen generate-cpp` PASS
  - `pio run -e freenove_esp32s3_full_with_ui` PASS
  - `pio run -e esp32dev -e esp32_release -e esp8266_oled -e ui_rp2040_ili9488 -e ui_rp2040_ili9486` PASS
  - `pio run -e freenove_esp32s3_full_with_ui -t buildfs` PASS
  - `pio run -e freenove_esp32s3_full_with_ui -t uploadfs --upload-port /dev/cu.usbmodem5AB90753301` PASS
  - `pio run -e freenove_esp32s3_full_with_ui -t upload --upload-port /dev/cu.usbmodem5AB90753301` PASS
  - contenu repo root via `.venv/bin/python`: validate/export scenario + audio manifest + printables manifest PASS
- Evidence série (115200):
  - boot: `[NET] wifi connect requested ssid=Les cils` + `[NET] boot wifi target=Les cils started=1`
  - statut validé: `NET_STATUS state=connected mode=STA ... sta_ssid=Les cils ... ap=0 fallback_ap=0`

## [2026-02-20] Freenove audio + écran + chaîne scénario/écran (Codex)

- Checkpoint sécurité exécuté: branche `main`, `git diff --stat` capturé, patch/status sauvegardés:
  - `/tmp/zacus_checkpoint/20260220_203024_wip.patch`
  - `/tmp/zacus_checkpoint/20260220_203024_status.txt`
- Scan artefacts trackés (`.pio`, `.platformio`, `logs`, `dist`, `build`, `node_modules`, `.venv`): aucun chemin tracké.
- Build/flash Freenove validés (`/dev/cu.usbmodem5AB90753301`):
  - `pio run -e freenove_esp32s3_full_with_ui` PASS
  - `pio run -e freenove_esp32s3_full_with_ui -t buildfs` PASS
  - `pio run -e freenove_esp32s3_full_with_ui -t uploadfs --upload-port /dev/cu.usbmodem5AB90753301` PASS
  - `pio run -e freenove_esp32s3_full_with_ui -t upload --upload-port /dev/cu.usbmodem5AB90753301` PASS
- Validation série (`pio device monitor --port /dev/cu.usbmodem5AB90753301 --baud 115200`) :
  - `AUDIO_TEST` PASS (tonalité intégrée RTTTL, sans dépendance LittleFS)
  - `AUDIO_TEST_FS` PASS (`/music/boot_radio.mp3`)
  - `AUDIO_PROFILE 0/1/2` PASS (switch runtime pinout I2S)
  - `UNLOCK` -> transition `STEP_U_SON_PROTO`, audio pack `PACK_BOOT_RADIO`, puis `audio_done` -> transition `STEP_WAIT_ETAPE2` PASS.
- Incohérence restante observée au boot: warning HAL SPI `spiAttachMISO(): HSPI Does not have default pins on ESP32S3!` (non bloquant en exécution actuelle).

## [2026-02-20] Freenove story step2 hardware + YAML GitHub + stack WiFi/AP + ESP-NOW (Codex)

- Checkpoint sécurité exécuté: branche `main`, `git diff --stat` capturé, patch/status sauvegardés:
  - `/tmp/zacus_checkpoint/20260220_211024_wip.patch`
  - `/tmp/zacus_checkpoint/20260220_211024_status.txt`
- Scan artefacts trackés (`.pio`, `.platformio`, `logs`, `dist`, `build`, `node_modules`, `.venv`): aucun chemin tracké.
- Story specs YAML mis à jour (source of truth):
  - `docs/protocols/story_specs/scenarios/default_unlock_win_etape2.yaml` enrichi avec transitions hardware pour l’étape 2:
    - `serial:BTN_NEXT`
    - `unlock:UNLOCK`
    - `action:ACTION_FORCE_ETAPE2`
  - génération revalidée:
    - `./tools/dev/story-gen validate` PASS
    - `./tools/dev/story-gen generate-cpp` PASS
    - `./tools/dev/story-gen generate-bundle` PASS
  - fichiers générés synchronisés (`hardware/libs/story/src/generated/*`), spec hash: `6edd9141d750`.
- GitHub pipeline Story restauré:
  - `.github/workflows/firmware-story-v2.yml` remplacé par un workflow fonctionnel (validate + generate-cpp + generate-bundle + check diff + artifact bundle).
- Runtime Freenove enrichi:
  - stack réseau ajoutée (`network_manager.{h,cpp}`) avec:
    - WiFi STA + AP management
    - ESP-NOW init/send/receive counters
    - bridge ESP-NOW payload -> événement scénario (`SERIAL:<event>`, `TIMER:<event>`, `ACTION:<event>`, `UNLOCK`, `AUDIO_DONE`)
  - commandes série ajoutées:
    - `NET_STATUS`, `WIFI_STATUS`, `WIFI_TEST`, `WIFI_STA`, `WIFI_AP_ON`, `WIFI_AP_OFF`
    - `ESPNOW_ON`, `ESPNOW_OFF`, `ESPNOW_STATUS`, `ESPNOW_SEND`
    - `SC_LIST`, `SC_LOAD`, `SC_REVALIDATE_ALL`, `SC_EVENT_RAW`
  - credentials test appliqués:
    - SSID test/AP par défaut: `Les cils`
    - mot de passe: `mascarade`
- Revalidation transitions via ScenarioSnapshot:
  - `SC_REVALIDATE` couvre events + hardware probes + vérifs ciblées step2:
    - `SC_REVALIDATE_STEP2 event=timer ... changed=1`
    - `SC_REVALIDATE_STEP2 event=action name=ACTION_FORCE_ETAPE2 ... changed=1`
    - `SC_REVALIDATE_STEP2 event=button label=BTN5_SHORT ... changed=1`
  - `SC_REVALIDATE_ALL` exécuté sur tous les scénarios built-in.
- Build/flash Freenove validés (`/dev/cu.usbmodem5AB90753301`):
  - `pio run -e freenove_esp32s3_full_with_ui` PASS
  - `pio run -e freenove_esp32s3_full_with_ui -t buildfs` PASS
  - `pio run -e freenove_esp32s3_full_with_ui -t uploadfs --upload-port /dev/cu.usbmodem5AB90753301` PASS
  - `pio run -e freenove_esp32s3_full_with_ui -t upload --upload-port /dev/cu.usbmodem5AB90753301` PASS (x2 après patch final)
- Validation série réseau:
  - `WIFI_AP_ON` PASS (`ssid=Les cils`, `mode=AP_STA`, `ip=192.168.4.1`)
  - `WIFI_AP_OFF` PASS
  - `WIFI_TEST` PASS (requête de connexion STA vers `Les cils`)
  - `ESPNOW_SEND` PASS (émission OK), pas de loopback RX local observé (`rx=0`) sur test mono-carte.

## TODO Freenove ESP32-S3 (contrat agent)

### [2026-02-17] Log détaillé des étapes réalisées – Freenove ESP32-S3

- Audit mapping hardware exécuté (artifacts/audit/firmware_20260217/validate_mapping_report.txt)
- Suppression des macros UART TX/RX pour UI Freenove all-in-one (platformio.ini, ui_freenove_config.h)
- Documentation détaillée des périphériques I2C, LED, Buzzer, DHT11, MPU6050 dans RC_FINAL_BOARD.md
- Synchronisation des macros techniques SPI/Serial/Driver dans ui_freenove_config.h (SPI_FREQUENCY, UI_LCD_SPI_HOST, etc.)
- Ajout d’une section multiplexage dans RC_FINAL_BOARD.md (partage BL/BTN3, CS TFT/BTN4)
- Ajout d’un commentaire explicite multiplexage dans ui_freenove_config.h
- Evidence synchronisée : tous les artefacts et logs sont tracés (artifacts/audit/firmware_20260217, logs/rc_live/freenove_esp32s3_YYYYMMDD.log)
- Traçabilité complète : chaque étape, correction, mapping, doc et evidence sont référencés dans AGENT_TODO.md

### [2026-02-17] Rapport d’audit mapping hardware Freenove ESP32-S3

**Correspondances parfaites** :
  - TFT SPI : SCK=18, MOSI=23, MISO=19, CS=5, DC=16, RESET=17, BL=4
  - Touch SPI : CS=21, IRQ=22
  - Boutons : 2, 3, 4, 5
  - Audio I2S : WS=25, BCK=26, DOUT=27
  - LCD : WIDTH=480, HEIGHT=320, ROTATION=1

**Écarts ou incohérences** :
  - UART : platformio.ini (TX=43, RX=44), ui_freenove_config.h (TX=1, RX=3), RC_FINAL_BOARD.md (non documenté)
  - I2C, LED, Buzzer, DHT11, MPU6050 : présents dans ui_freenove_config.h, absents ou non alignés ailleurs
  - SPI Host, Serial Baud, Driver : macros techniques non synchronisées
  - Multiplexage : BL/Bouton3 et CS TFT/Bouton4 partagent la broche, à clarifier

**Recommandations** :
  - Aligner UART TX/RX sur une valeur unique et documenter
  - Documenter I2C, LED, Buzzer, DHT11, MPU6050 dans RC_FINAL_BOARD.md
  - Synchroniser macros techniques dans ui_freenove_config.h et platformio.ini
  - Ajouter une note sur le multiplexage dans RC_FINAL_BOARD.md et ui_freenove_config.h
  - Reporter toute évolution dans AGENT_TODO.md

**Evidence** :
  - Rapport complet : artifacts/audit/firmware_20260217/validate_mapping_report.txt
  [2026-02-17] Synthèse technique – Détection dynamique des ports USB
    - Tous les ports USB série sont scannés dynamiquement (glob /dev/cu.*).
    - Attribution des rôles (esp32, esp8266, rp2040) selon mapping, fingerprint, VID:PID, fallback CP2102.
    - Esp32-S3 : peut apparaître en SLAB_USBtoUART (CP2102) ou usbmodem (mode bootloader, DFU, flash initial).
    - RP2040 : typiquement usbmodem.
    - Si un seul CP2102 détecté, fallback mono-port (esp32+esp8266 sur le même port).
    - Si plusieurs ports, mapping par location, fingerprint, ou VID:PID.
    - Aucun port n’est hardcodé : la détection s’adapte à chaque run (reset, flash, bootloader).
    - Les scripts cockpit.sh, resolve_ports.py, run_matrix_and_smoke.sh exploitent cette logique.
    - Traçabilité : logs/ports_debug.json, logs/ports_resolve.json, artifacts/rc_live/...
    - Documentation onboarding mise à jour pour refléter la procédure.
    - Recommandation : toujours utiliser la détection dynamique, ne jamais forcer un port sauf cas extrême (override manuel).

  [2026-02-17] Validation RC Live Freenove ESP32-S3
    - Port USB corrigé : /dev/cu.SLAB_USBtoUART
    - Flash ciblé relancé : pio run -e freenove_esp32s3 -t upload --upload-port /dev/cu.SLAB_USBtoUART
    - Evidence synchronisée : summary.md, ui_link.log, logs/rc_live/20260217-211606
    - Résultat : UI link FAIL (connected=1 mais status unavailable)
    - Artefacts : artifacts/rc_live/20260217-211606/summary.md, ui_link.log
    - Log détaillé ajouté dans AGENT_TODO.md

- [ ] Vérifier et compléter le mapping hardware dans platformio.ini ([env:freenove_esp32s3])
  - Pins, UART, SPI, I2C, etc. : vérifier cohérence avec la doc.
  - Exemple : GPIO22=I2S_WS, GPIO23=I2S_DATA, UART1=TX/RX.
- [ ] Aligner ui_freenove_config.h avec platformio.ini et la documentation RC_FINAL_BOARD.md
  - Vérifier que chaque pin, bus, périphérique est documenté et codé.
  - Ajouter commentaires explicites pour chaque mapping.
- [ ] Mettre à jour docs/RC_FINAL_BOARD.md
  - Décrire le mapping complet, schéma, photo, table des pins.
  - Ajouter procédure de flash/test spécifique Freenove.
  - Mentionner les différences avec ESP32Dev.
- [ ] Adapter build_all.sh, cockpit.sh, run_matrix_and_smoke.sh, etc.
  - Ajouter/valider la cible freenove_esp32s3 dans les scripts.
  - Vérifier que le build, flash, smoke, logs fonctionnent sur Freenove.
  - Exemple : ./build_all.sh doit inclure freenove_esp32s3.
- [ ] Vérifier la production de logs et artefacts lors des tests sur Freenove
  - Chemin : logs/rc_live/freenove_esp32s3_YYYYMMDD.log
  - Artefacts : artifacts/rc_live/freenove_esp32s3_YYYYMMDD.html
  - Référencer leur existence (chemin, timestamp, verdict) dans docs/AGENT_TODO.md.
- [ ] Mettre à jour l’onboarding (docs/QUICKSTART.md, docs/AGENTS_COPILOT_PLAYBOOK.md)
  - Ajouter section « Flash Freenove », « Test Freenove ».
  - Préciser les ports série, baud, procédure de résolution dynamique.
- [ ] Vérifier que les gates smoke et stress test sont compatibles et valident strictement la cible Freenove
  - Fail sur panic/reboot, verdict UI_LINK_STATUS connected==1.
  - Tester WebSocket health, stress I2S, scénario LittleFS.
- [ ] Documenter toute évolution ou correction dans docs/AGENT_TODO.md
  - Détailler les étapes réalisées, artefacts produits, impasses matérielles.
  - Exemple : « Test flash Freenove OK, logs produits dans logs/rc_live/freenove_esp32s3_20260217.log ».
## [2026-02-17] ESP32 pin remap test kickoff (Codex)

- Safety checkpoint re-run via cockpit wrappers: `git status`, `git diff --stat`, `git branch`.
- Checkpoint files saved: `/tmp/zacus_checkpoint/20260217-185336_wip.patch` and `/tmp/zacus_checkpoint/20260217-185336_status.txt`.
- Tracked artifact scan (`.pio`, `.platformio`, `logs`, `dist`, `build`, `node_modules`, `.venv`) returned no tracked paths.
- Runtime status at kickoff: UI Link handshake still FAIL (`connected=0`) on latest cross-monitor evidence (`artifacts/ui_link_diag/20260217-174822/`), LittleFS fallback status unchanged, I2S stress status unchanged (last 30 min PASS).

## [2026-02-17] ESP32 UI pin remap execution (Codex)

- Applied ESP32 UI UART remap in firmware config: `TX=GPIO23`, `RX=GPIO18` (OLED side stays `D4/D5`).
- Rebuilt and reflashed ESP32 (`pio run -e esp32dev`, then `pio run -e esp32dev -t upload --upload-port /dev/cu.SLAB_USBtoUART9`).
- Cross-monitor rerun: `python3 tools/dev/capture_ui_link_boot_diag.py --seconds 22`.
- Evidence: `artifacts/ui_link_diag/20260217-175606/`.
- Result moved from FAIL to WARN: ESP32 now receives HELLO frames (`esp32 tx/rx=48/20`, `UI_LINK_STATUS connected=1` seen), but ESP8266 still receives nothing from ESP32 (`esp8266 tx/rx=19/0`), so handshake remains incomplete.

## [2026-02-17] UI link diagnostic patch kickoff (Codex)

- Safety checkpoint re-run via cockpit wrappers: `git status`, `git diff --stat`, `git branch`.
- Checkpoint files saved: `/tmp/zacus_checkpoint/20260217-181730_wip.patch` and `/tmp/zacus_checkpoint/20260217-181730_status.txt`.
- Tracked artifact scan (`.pio`, `.platformio`, `logs`, `dist`, `build`, `node_modules`, `.venv`) returned no tracked paths.
- Runtime status at kickoff: UI Link strict gate still FAIL (`connected=0`), LittleFS scenario load now mitigated by V2 fallback, I2S stress status currently PASS on last 30 min run.

## [2026-02-17] Cross-monitor boot capture (Codex)

- Built and flashed diagnostics on both boards: `pio run -e esp32dev -e esp8266_oled`, then upload on `/dev/cu.SLAB_USBtoUART9` (ESP32) and `/dev/cu.SLAB_USBtoUART` (ESP8266).
- Captured synchronized boot monitors with `python3 tools/dev/capture_ui_link_boot_diag.py --seconds 22`.
- Evidence: `artifacts/ui_link_diag/20260217-173005/` (`esp32.log`, `esp8266.log`, `merged.log`, `summary.md`, `ports_resolve.json`, `meta.json`).
- Verdict remains FAIL: no discriminant RX observed on either side (`ESP32 tx/rx=48/0`, `ESP8266 tx/rx=19/0`, no `connected=1`), indicating traffic still not seen on D4/D5 at runtime.

## [2026-02-17] Cross-monitor rerun after pin inversion (Codex)

- Re-ran `python3 tools/dev/capture_ui_link_boot_diag.py --seconds 22` after manual pin inversion test.
- Evidence: `artifacts/ui_link_diag/20260217-174330/`.
- Verdict unchanged: FAIL with `ESP32 tx/rx=48/0`, `ESP8266 tx/rx=19/0`, no `connected=1`.

## [2026-02-17] Cross-monitor rerun after inverted wiring confirmation (Codex)

- Re-ran `python3 tools/dev/capture_ui_link_boot_diag.py --seconds 22` after updated wiring check.
- Evidence: `artifacts/ui_link_diag/20260217-174822/`.
- Verdict unchanged: FAIL with `ESP32 tx/rx=48/0`, `ESP8266 tx/rx=19/0`, no `connected=1`.

## [2026-02-17] D4/D5 handshake + strict firmware_tests rerun (Codex)

- Safety checkpoint re-run via cockpit wrappers: `git status`, `git diff --stat`, `git branch`.
- Checkpoint files saved: `/tmp/zacus_checkpoint/20260217-173556_wip.patch` and `/tmp/zacus_checkpoint/20260217-173556_status.txt`.
- Tracked artifact scan (`.pio`, `.platformio`, `logs`, `dist`, `build`, `node_modules`, `.venv`) returned no tracked paths.
- Build gate rerun: `./build_all.sh` PASS (5/5 envs) after firmware updates.
- Reflash completed: `pio run -e esp32dev -t upload --upload-port /dev/cu.SLAB_USBtoUART9` and `pio run -e esp8266_oled -t upload --upload-port /dev/cu.SLAB_USBtoUART`.
- `firmware_tooling` rerun PASS (`plan-only` + full run); all `--help` paths stay non-destructive.
- `firmware_tests` strict rerun (`ZACUS_REQUIRE_HW=1`) still FAIL at gate 1: `artifacts/rc_live/20260217-164154/summary.md` (`UI_LINK_STATUS connected=0`, esp8266 monitor FAIL).
- Strict smoke rerun uses repo-root resolver (`tools/test/resolve_ports.py`) and strict ESP32+ESP8266 mapping; gate FAIL remains UI link only (`artifacts/smoke_tests/20260217-164237/smoke_tests.log`), while `STORY_LOAD_SCENARIO DEFAULT` now succeeds through fallback (`STORY_LOAD_SCENARIO_FALLBACK V2 DEFAULT` + `STORY_LOAD_SCENARIO_OK`).
- `audit_coherence.py` rerun PASS (`artifacts/audit/20260217-164243/summary.md`).
- Content validators rerun PASS from repo root: scenario validate/export + audio manifest + printables manifest.
- Stress rerun completed PASS (`python3 tools/dev/run_stress_tests.py --hours 0.5`): `artifacts/stress_test/20260217-164248/summary.md` (`87` iterations, success rate `100.0%`, no panic/reboot markers in log).
- Runtime status after reruns: UI Link = FAIL (`connected=0`), LittleFS default scenario = mitigated by V2 fallback in serial path, I2S stability = PASS over 30 min stress run (no panic/reboot markers).

## [2026-02-17] Fix handshake/UI smoke strict kickoff (Codex)

- Safety checkpoint executed via cockpit wrappers: branch `story-V2`, `git status`, `git diff --stat`, `git branch`.
- Checkpoint files saved: `/tmp/zacus_checkpoint/20260217-155753_wip.patch` and `/tmp/zacus_checkpoint/20260217-155753_status.txt`.
- Tracked artifact scan (`.pio`, `.platformio`, `logs`, `dist`, `build`, `node_modules`, `.venv`) returned no tracked paths; no untrack action required.
- Runtime status at kickoff: UI Link = FAIL (`connected=0`), LittleFS default scenario = missing (`/story/scenarios/DEFAULT.json`), I2S stability = FAIL/intermittent panic in stress recovery path.

## [2026-02-17] Copilot sequence checkpoint (firmware_tooling -> firmware_tests)

- Safety checkpoint executed before edits via cockpit wrappers: branch `story-V2`, `git status`, `git diff --stat`, `git branch`.
- Checkpoint files saved: `/tmp/zacus_checkpoint/20260217-162509_wip.patch` and `/tmp/zacus_checkpoint/20260217-162509_status.txt`.
- Tracked artifact scan (`.pio`, `.platformio`, `logs`, `dist`, `build`, `node_modules`, `.venv`) returned no tracked paths; no untrack action required.
- Kickoff runtime status: UI Link still failing in prior smoke evidence, default LittleFS scenario still missing in prior QA notes, I2S panic already known in stress path.

## [2026-02-17] Copilot sequence execution + alignment fixes

- Plan/runner alignment applied: root `tools/dev/plan_runner.sh` now delegates to `hardware/firmware/tools/dev/plan_runner.sh`; firmware runner now executes from repo root and resolves active agent IDs recursively under `.github/agents/` (excluding `archive/`).
- `run_matrix_and_smoke.sh` now supports `--help`/`-h` without side effects and returns explicit non-zero on unknown args (`--bad-arg` -> rc=2).
- Agent briefs synchronized in root + firmware copies (`domains/firmware-tooling.md`, `domains/firmware-tests.md`) with repo-root commands and venv-aware PATH (`PATH=$(pwd)/hardware/firmware/.venv/bin:$PATH`).
- `firmware_tooling` sequence: PASS (`bash hardware/firmware/tools/dev/plan_runner.sh --agent firmware_tooling`).
- `firmware_tests` strict sequence (`ZACUS_REQUIRE_HW=1`): blocked at step 1 because `run_matrix_and_smoke` reports UI link failure; evidence `artifacts/rc_live/20260217-153129/summary.md` (`UI_LINK_STATUS connected=0`).
- Remaining test gates executed manually after the blocked step:
  - `run_smoke_tests.sh` strict: FAIL (port resolution), evidence `artifacts/smoke_tests/20260217-153214/summary.md`.
  - `run_stress_tests.py --hours 0.5`: FAIL, scenario does not complete (`DEFAULT`), evidence `artifacts/stress_test/20260217-153220/summary.md`; earlier run also captured I2S panic evidence `artifacts/stress_test/20260217-153037/stress_test.log`.
  - `audit_coherence.py`: initially FAIL on missing runbook refs, then PASS after `cockpit_commands.yaml` runbook fixes + regenerated commands doc; evidence `artifacts/audit/20260217-153246/summary.md` (latest PASS).
- RP2040 docs/config audit: no env naming drift detected (`ui_rp2040_ili9488`, `ui_rp2040_ili9486`) across `platformio.ini`, `build_all.sh`, `run_matrix_and_smoke.sh`, `docs/QUICKSTART.md`, and `docs/TEST_SCRIPT_COORDINATOR.md`.
- Runtime status after this pass: UI Link = FAIL (`connected=0`), LittleFS default scenario = missing (`/story/scenarios/DEFAULT.json`), I2S panic = intermittent (observed in stress evidence above).

## [2026-02-17] Audit kickoff checkpoint (Codex)

- Safety checkpoint run from `hardware/firmware`: branch `story-V2`, working tree dirty (pre-existing changes), `git diff --stat` captured before edits.
- Checkpoint files saved: `/tmp/zacus_checkpoint/20260217-141814_wip.patch` and `/tmp/zacus_checkpoint/20260217-141814_status.txt`.
- Tracked artifact scan (`.pio`, `.platformio`, `logs`, `dist`, `build`, `node_modules`, `.venv`) returned no tracked paths, so no untrack action was needed.
- Runtime status at kickoff: UI Link `connected=0` in previous smoke evidence, LittleFS default scenario still flagged as missing by prior QA notes, I2S panic previously reported in stress recovery path.

## [2026-02-17] Audit + coherence pass (Codex)

- Tooling/scripts corrected: `tools/dev/run_matrix_and_smoke.sh` (broken preamble/functions restored, non-interactive USB path now resolves ports, accepted `learned-map` reasons, story screen skip evidence), `tools/dev/serial_smoke.py` (ports map loading re-enabled, dead duplicate role code removed), `tools/dev/cockpit.sh` (`rc` no longer forces `ZACUS_REQUIRE_HW=1`, debug banner removed), `tools/dev/plan_runner.sh` (portable paths/date and safer arg parsing), UI OLED HELLO/PONG now built with protocol CRC via `uiLinkBuildLine`.
- Command registry/doc sync fixed: `tools/dev/cockpit_commands.yaml` normalized under one `commands:` key (including `wifi-debug`) and regenerated `docs/_generated/COCKPIT_COMMANDS.md` now lists full evidence columns without blank rows.
- Protocol/runtime check: `protocol/ui_link_v2.md` expects CRC frames; OLED runtime now emits CRC on HELLO/PONG. Remaining runtime discrepancy logged: Story V2 controller still boots from generated scenario IDs (`DEFAULT`) rather than directly from `game/scenarios/zacus_v1.yaml`; root YAML remains validated/exported as canonical content source.
- Build gate result: `./build_all.sh` PASS (5/5 envs) with log `logs/run_matrix_and_smoke_20260217-143000.log` and build artifacts in `.pio/build/`.
- Smoke gate result: `./tools/dev/run_matrix_and_smoke.sh` FAIL (`artifacts/rc_live/20260217-143000/summary.json`) — ports resolved, then `smoke_esp8266_usb` and `ui_link` failed (`UI_LINK_STATUS missing`, ESP32 log in bootloader/download mode). Evidence: `artifacts/rc_live/20260217-143000/ui_link.log`, `artifacts/rc_live/20260217-143000/smoke_esp8266_usb.log`.
- Content gates result (run from repo root): scenario validate/export PASS (`game/scenarios/zacus_v1.yaml`), audio manifest PASS (`audio/manifests/zacus_v1_audio.yaml`), printables manifest PASS (`printables/manifests/zacus_v1_printables.yaml`).
- Runtime status after this pass: UI Link = FAIL on latest smoke evidence (`connected` unavailable), LittleFS default scenario = not revalidated in this run (known previous blocker remains), I2S panic = not retested in this pass (stress gate pending).

## [2026-02-17] Rapport d’erreur automatisé – Story V2

- Correction du bug shell (heredoc Python → script temporaire).
- Échec de la vérification finale : build FAIL, ports USB OK, smoke/UI/artefacts SKIPPED.
- Rapport d’erreur généré : voir artifacts/rapport_erreur_story_v2_20260217.md
- Recommandation : analyser le log de build, corriger, relancer la vérification.

# Agent TODO & governance

## 1. Structural sweep & merge
- [x] Commit the pending cleanup described in `docs/SPRINT_RECOMMENDATIONS.md:18-80` (structure/tree fixes + PR #86 merge/tag) so the repo is back on main.
- [x] Consolidation 2026-02-21 exécutée: `feat/fix-firmware-story-workflow`, `feat/freenove-ap-fallback-stable`, `feat/freenove-webui-network-ops-parity` intégrés dans l’historique de `main`; CI GitHub Actions désormais limité à `main` seulement.
- [x] Branches de travail supprimées (local + remote): `feat/freenove-ap-local-espnow-rtc-sync`, `origin/feat/fix-firmware-story-workflow`, `origin/feat/freenove-ap-fallback-stable`, `origin/feat/freenove-webui-network-ops-parity`.
- [x] Tags de sauvegarde créés avant suppression: `backup/20260221_212813/feat-fix-firmware-story-workflow-1b4c328`, `backup/20260221_212813/feat-fix-firmware-story-workflow-b530be3`, `backup/20260221_212813/feat-freenove-ap-fallback-stable`, `backup/20260221_212813/feat-freenove-webui-network-ops-parity`, `backup/20260221_212813/feat-freenove-ap-local-espnow-rtc-sync`.

## 2. Build/test gates
- [x] Re-run `./build_all.sh` (`build_all.sh:6`); artifacts landed under `artifacts/build/` and logs live in `logs/run_matrix_and_smoke_*.log` if rerun again via the smoke gate.
- [x] Re-launch `./tools/dev/run_matrix_and_smoke.sh` (`tools/dev/run_matrix_and_smoke.sh:9-200`) – run completed 2026-02-16 14:35 (artifact `artifacts/rc_live/20260216-143539/`), smoke scripts and serial monitors succeeded but UI link still reports `connected=0` (no UI handshake). Need to plug in/validate UI firmware before closing gate.
- [x] Capture evidence for HTTP API, WebSocket, and WiFi/Health steps noted as blocked or TODO in `docs/TEST_SCRIPT_COORDINATOR.md:13-20` – `tools/dev/healthcheck_wifi.sh` created `artifacts/rc_live/healthcheck_20260216-154709.log` (ping+HTTP fail) and the HTTP API script logged connection failures under `artifacts/http_ws/20260216-154945/http_api.log` (ESP_URL=127.0.0.1:9). WebSocket skipped (wscat missing). All failures logged to share evidence.

## 3. QA + automation hygiene
- [x] Execute the manual serial smoke path (`python3 tools/dev/serial_smoke.py --role auto --baud 115200 --wait-port 3 --allow-no-hardware`) – passed on /dev/cu.SLAB_USBtoUART + /dev/cu.SLAB_USBtoUART9, reporting UI link still down (same failure as matrix run).
- [ ] Run the story QA suite (`tools/dev/run_smoke_tests.sh`, `python3 tools/dev/run_stress_tests.py ...`, `make fast-*` loops) documented in `esp32_audio/TESTING.md:36-138` and capture logs (smoke_tests failed: DEFAULT scenario missing `/story/scenarios/DEFAULT.json`; run_stress_tests failed with I2S panic during recovery; `make fast-esp32` / `fast-ui-oled` built & flashed but monitor commands quit in non-interactive mode, `fast-ui-tft` not run because no RP2040 board connected). Need scenario files/UI recovery to unblock.
- [x] Ensure any generated artifacts remain untracked per agent contract (no logs/artifacts added to git).

## 4. Documentation & agent contracts
- [ ] Update `AGENTS.md` and `tools/dev/AGENTS.md` whenever scripts/gates change, per their own instructions (`AGENTS.md`, `tools/dev/AGENTS.md`).
- [ ] Keep `tools/dev/cockpit_commands.yaml` in sync with `docs/_generated/COCKPIT_COMMANDS.md` via `python3 tools/dev/gen_cockpit_docs.py` after edits, and confirm the command registry is reflected in `docs/TEST_SCRIPT_COORDINATOR.md` guidance.
- [ ] Review `docs/INDEX.md`, `docs/ARCHITECTURE_UML.md`, and `docs/QUICKSTART.md` after significant changes so the onboarding picture matches the agent constraints.

## 5. Reporting & evidence
- [ ] When publishing smoke/baseline runs, include the required artifacts (`meta.json`, `commands.txt`, `summary.md`, per-step logs) under `artifacts/…` as demanded by `docs/TEST_SCRIPT_COORDINATOR.md:160-199`.
- [ ] Document any pipeline/test regressions in `docs/RC_AUTOFIX_CICD.md` or similar briefing docs and flag them for the Test & Script Coordinator.

## Traçabilité build/smoke 17/02/2026

- Succès : esp32dev, esp32_release, esp8266_oled
- Échec : ui_rp2040_ili9488, ui_rp2040_ili9486
- Evidence : logs et artefacts dans hardware/firmware/artifacts/build, logs/
- Actions tentées : correction du filtre build_src_filter, création de placeholder, relance build/smoke
- Problème persistant : échec RP2040 (sources/configuration à investiguer)
- Prochaine étape : escalade à un agent expert RP2040 ou hand-off

## [2026-02-18] Story portable + V3 serial migration

- [x] Added host-side story generation library `lib/zacus_story_gen_ai` (Yamale + Jinja2) with CLI:
  - `story-gen validate`
  - `story-gen generate-cpp`
  - `story-gen generate-bundle`
  - `story-gen all`
- [x] Replaced legacy generator entrypoint `hardware/libs/story/tools/story_gen/story_gen.py` with compatibility wrapper delegating to `zacus_story_gen_ai`.
- [x] Migrated portable runtime internals to tinyfsm-style state handling in `lib/zacus_story_portable/src/story_portable_runtime.cpp` while keeping `StoryPortableRuntime` facade.
- [x] Introduced Story serial V3 JSON-lines handlers (`story.status`, `story.list`, `story.load`, `story.step`, `story.validate`, `story.event`) and removed `STORY_V2_*` routing from canonical command list.
- [x] Updated run matrix/log naming to include environment label:
  - log: `logs/rc_live/<env_label>_<ts>.log`
  - artifacts: `artifacts/rc_live/<env_label>_<ts>/summary.md`
- [x] `run_matrix_and_smoke.sh` now supports single-board mode (`ZACUS_ENV="freenove_esp32s3"`): ESP8266/UI-link/story-screen checks are emitted as `SKIP` with detail `not needed for combined board`.
- [x] `tools/test/resolve_ports.py` now honors `--need-esp32` and `--need-esp8266` explicitly via required roles, while still emitting both `esp32` and `esp8266` fields in JSON output.
[20260218-020041] Run artefacts: /Users/cils/Documents/Enfants/anniv isaac 10a/le-mystere-professeur-zacus/hardware/firmware/artifacts/rc_live/freenove_esp32s3_20260218-020041, logs: /Users/cils/Documents/Enfants/anniv isaac 10a/le-mystere-professeur-zacus/hardware/firmware/logs/rc_live, summary: /Users/cils/Documents/Enfants/anniv isaac 10a/le-mystere-professeur-zacus/hardware/firmware/artifacts/rc_live/freenove_esp32s3_20260218-020041/summary.md
[20260218-021042] Run artefacts: /Users/cils/Documents/Enfants/anniv isaac 10a/le-mystere-professeur-zacus/hardware/firmware/artifacts/rc_live/freenove_esp32s3_20260218-021042, logs: /Users/cils/Documents/Enfants/anniv isaac 10a/le-mystere-professeur-zacus/hardware/firmware/logs/rc_live, summary: /Users/cils/Documents/Enfants/anniv isaac 10a/le-mystere-professeur-zacus/hardware/firmware/artifacts/rc_live/freenove_esp32s3_20260218-021042/summary.md
[20260221-222358] Run artefacts: /Users/cils/Documents/Enfants/anniv isaac 10a/le-mystere-professeur-zacus/hardware/firmware/artifacts/rc_live/freenove_esp32s3_20260221-222358, logs: /Users/cils/Documents/Enfants/anniv isaac 10a/le-mystere-professeur-zacus/hardware/firmware/logs/rc_live, summary: /Users/cils/Documents/Enfants/anniv isaac 10a/le-mystere-professeur-zacus/hardware/firmware/artifacts/rc_live/freenove_esp32s3_20260221-222358/summary.md
[20260221-223450] Run artefacts: /Users/cils/Documents/Enfants/anniv isaac 10a/le-mystere-professeur-zacus/hardware/firmware/artifacts/rc_live/freenove_esp32s3_20260221-223450, logs: /Users/cils/Documents/Enfants/anniv isaac 10a/le-mystere-professeur-zacus/hardware/firmware/logs/rc_live, summary: /Users/cils/Documents/Enfants/anniv isaac 10a/le-mystere-professeur-zacus/hardware/firmware/artifacts/rc_live/freenove_esp32s3_20260221-223450/summary.md

## [2026-02-22] Freenove story modular + SD + ESP-NOW v1 envelope + WebUI push

- [x] Story storage rendu modulaire avec fallback intégré:
  - bundle Story par défaut provisionné automatiquement en LittleFS (`/story/{scenarios,screens,audio,apps,actions}`),
  - support SD_MMC Freenove activé (`/sd/story/...`) avec sync SD -> LittleFS au boot et via commande.
- [x] Commandes/runtime ajoutés pour opérabilité story:
  - serial: `STORY_SD_STATUS`, `STORY_REFRESH_SD`,
  - WebUI/API: bouton + endpoint `POST /api/story/refresh-sd`.
- [x] Timeline JSON enrichie sur les scènes Story (`data/story/screens/SCENE_*.json`) avec keyframes `at_ms/effect/speed_ms/theme`.
- [x] Audio crossfade consolidé:
  - correction callback fin de piste (track réel reporté),
  - lecture des packs audio depuis LittleFS **ou SD** (`/sd/...`).
- [x] ESP-NOW aligné avec `docs/espnow_api_v1.md`:
  - enveloppe v1 supportée (`msg_id`, `seq`, `type`, `payload`, `ack`),
  - extraction metadata + statut exposé,
  - trames `type=command` exécutées côté runtime et réponse corrélée `type=ack` renvoyée si `ack=true`.
- [x] WebUI passée en push temps réel:
  - endpoint SSE `GET /api/stream` (statut Story/WiFi/ESP-NOW),
  - fallback refresh conservé côté front.
- [x] Générateur Story (`zacus_story_gen_ai`) amélioré:
  - bundle Story injecte désormais le contenu réel des ressources JSON (`data/story/*`) au lieu de placeholders minimalistes.

### Vérifications exécutées

- `pio run -e freenove_esp32s3` ✅
- `pio run -e freenove_esp32s3_full_with_ui -t buildfs` ✅
- `pio run -e freenove_esp32s3_full_with_ui -t uploadfs --upload-port /dev/cu.usbmodem5AB90753301` ✅
- `pio run -e freenove_esp32s3_full_with_ui -t upload --upload-port /dev/cu.usbmodem5AB90753301` ✅
- Vérification série post-flash (`STORY_SD_STATUS`, `WIFI_STATUS`, `ESPNOW_STATUS_JSON`) ✅, IP observée: `192.168.0.91`
- `ZACUS_ENV=freenove_esp32s3 ZACUS_PORT_ESP32=/dev/cu.usbmodem5AB90753301 ZACUS_REQUIRE_HW=1 ZACUS_NO_COUNTDOWN=1 ./tools/dev/run_matrix_and_smoke.sh` ✅
- `./tools/dev/story-gen validate` ✅
- `./tools/dev/story-gen generate-bundle --out-dir /tmp/story_bundle_test` ✅
- `.venv/bin/python3 -m pytest lib/zacus_story_gen_ai/tests/test_generator.py` ✅
- `curl http://192.168.0.91/api/status` + `curl http://192.168.0.91/api/stream` ✅ (payloads UTF-8 validés après fix snapshot JSON)
- Frontend dev UI: `npm run build` ✅ (fix TS no-return sur `src/lib/deviceApi.ts` pour débloquer le flux front)
[20260221-234147] Run artefacts: /Users/cils/Documents/Enfants/anniv isaac 10a/le-mystere-professeur-zacus/hardware/firmware/artifacts/rc_live/freenove_esp32s3_20260221-234147, logs: /Users/cils/Documents/Enfants/anniv isaac 10a/le-mystere-professeur-zacus/hardware/firmware/logs/rc_live, summary: /Users/cils/Documents/Enfants/anniv isaac 10a/le-mystere-professeur-zacus/hardware/firmware/artifacts/rc_live/freenove_esp32s3_20260221-234147/summary.md
[20260221-235139] Run artefacts: /Users/cils/Documents/Enfants/anniv isaac 10a/le-mystere-professeur-zacus/hardware/firmware/artifacts/rc_live/freenove_esp32s3_20260221-235139, logs: /Users/cils/Documents/Enfants/anniv isaac 10a/le-mystere-professeur-zacus/hardware/firmware/logs/rc_live, summary: /Users/cils/Documents/Enfants/anniv isaac 10a/le-mystere-professeur-zacus/hardware/firmware/artifacts/rc_live/freenove_esp32s3_20260221-235139/summary.md
[20260222-000305] Run artefacts: /Users/cils/Documents/Enfants/anniv isaac 10a/le-mystere-professeur-zacus/hardware/firmware/artifacts/rc_live/freenove_esp32s3_20260222-000305, logs: /Users/cils/Documents/Enfants/anniv isaac 10a/le-mystere-professeur-zacus/hardware/firmware/logs/rc_live, summary: /Users/cils/Documents/Enfants/anniv isaac 10a/le-mystere-professeur-zacus/hardware/firmware/artifacts/rc_live/freenove_esp32s3_20260222-000305/summary.md

## [2026-02-22] Suite autonome — installations + vérifications complètes

- [x] Préflight sécurité refait avant action:
  - branch/diff/status affichés,
  - checkpoint enregistré: `/tmp/zacus_checkpoint/20260222-005615_wip.patch` et `/tmp/zacus_checkpoint/20260222-005615_status.txt`,
  - scan artefacts trackés (`.pio`, `.platformio`, `logs`, `dist`, `build`, `node_modules`, `.venv`) = aucun trouvé.
- [x] Installations/outillage:
  - `./tools/dev/bootstrap_local.sh` exécuté (venv + dépendances Python + `zacus_story_gen_ai` editable),
  - validation outillage: `pio --version`, `python3 -m serial.tools.list_ports -v`,
  - `tools/dev/check_env.sh` durci pour fallback `pip3` et dépendances optionnelles en `WARN` (exécution validée).
- [x] Vérifications story/content:
  - `.venv/bin/python3 tools/scenario/validate_scenario.py game/scenarios/zacus_v1.yaml` ✅
  - `.venv/bin/python3 tools/audio/validate_manifest.py audio/manifests/zacus_v1_audio.yaml` ✅
  - `.venv/bin/python3 tools/printables/validate_manifest.py printables/manifests/zacus_v1_printables.yaml` ✅
  - `./tools/dev/story-gen validate` ✅
  - `.venv/bin/python3 -m pytest lib/zacus_story_gen_ai/tests/test_generator.py` ✅
- [x] Vérifications firmware/hardware Freenove USB modem:
  - `pio run -e freenove_esp32s3` ✅
  - `pio run -e freenove_esp32s3_full_with_ui -t buildfs` ✅
  - `pio run -e freenove_esp32s3_full_with_ui -t uploadfs --upload-port /dev/cu.usbmodem5AB90753301` ✅
  - `pio run -e freenove_esp32s3_full_with_ui -t upload --upload-port /dev/cu.usbmodem5AB90753301` ✅
  - `ZACUS_ENV=freenove_esp32s3 ZACUS_PORT_ESP32=/dev/cu.usbmodem5AB90753301 ZACUS_REQUIRE_HW=1 ZACUS_NO_COUNTDOWN=1 ./tools/dev/run_matrix_and_smoke.sh` ✅ (`UI_LINK_STATUS=SKIP` justifié combined board).
- [x] Gates build contractuels:
  - `pio run -e esp32dev -e esp32_release -e esp8266_oled -e ui_rp2040_ili9488 -e ui_rp2040_ili9486` ✅ (5/5).
- [x] Cohérence doc/registry:
  - `.venv/bin/python3 tools/dev/gen_cockpit_docs.py` ✅
  - `.venv/bin/python3 tools/test/audit_coherence.py` ✅ (`RESULT=PASS`).
- [x] Statut réseau et API runtime Freenove:
  - série `WIFI_STATUS`/`ESPNOW_STATUS_JSON`/`STORY_SD_STATUS` ✅,
  - IP actuelle observée: `192.168.0.91`,
  - `curl /api/status` et `curl /api/stream` ✅ (payloads Story/WiFi/ESP-NOW valides).

## [2026-02-22] Clarification doc Story V2 vs protocole série V3

- [x] Alignement documentaire réalisé pour éviter l'ambiguïté "V2/V3":
  - `docs/protocols/GENERER_UN_SCENARIO_STORY_V2.md`: V2 explicitée (moteur/spec), commandes de test série basculées en JSON-lines V3 (`story.*`), chemins outillage alignés (`./tools/dev/story-gen`).
  - `docs/protocols/story_v3_serial.md`: section `Scope` ajoutée (V3 = interface série, V2 = génération/runtime).
  - `docs/protocols/story_README.md`: section commandes réorganisée avec recommandation V3 et rappel legacy `STORY_V2_*` pour debug uniquement.

## [2026-02-22] Story UI — génération écrans/effets/transitions (suite)

- [x] Générateur `zacus_story_gen_ai` renforcé pour les ressources écran:
  - normalisation automatique d'un profil écran (fallback par `SCENE_*`),
  - timeline normalisée en objet `timeline = {loop, duration_ms, keyframes[]}`,
  - keyframes consolidées (`at_ms`, `effect`, `speed_ms`, `theme`) avec garde-fous (ordre, bornes, minimum 2 keyframes),
  - transition normalisée (`transition.effect`, `transition.duration_ms`).
- [x] Runtime UI Freenove enrichi:
  - parse `timeline.keyframes` (et compat ancien format array),
  - support `timeline.loop` + `timeline.duration_ms`,
  - transitions de scène ajoutées (`fade`, `slide_left/right/up/down`, `zoom`, `glitch`) avec durée pilotable.
- [x] Données Story alignées:
  - `data/story/screens/SCENE_*.json` migrés vers format timeline keyframes + transition,
  - fallback embarqué (`storage_manager.cpp`) synchronisé sur le même contrat JSON.
- [x] Vérifications exécutées:
  - `.venv/bin/python3 -m pytest lib/zacus_story_gen_ai/tests/test_generator.py` ✅ (4 tests),
  - `./tools/dev/story-gen validate` ✅,
  - `./tools/dev/story-gen generate-bundle --out-dir /tmp/story_bundle_fx_<ts>` ✅ (payloads écran keyframes+transition vérifiés),
  - `pio run -e freenove_esp32s3` ✅,
  - `pio run -e freenove_esp32s3_full_with_ui -t buildfs` ✅,
  - `pio run -e freenove_esp32s3_full_with_ui -t uploadfs --upload-port /dev/cu.usbmodem5AB90753301` ✅,
  - `pio run -e freenove_esp32s3_full_with_ui -t upload --upload-port /dev/cu.usbmodem5AB90753301` ✅,
  - `ZACUS_ENV=freenove_esp32s3 ... ./tools/dev/run_matrix_and_smoke.sh` ✅ (artefact: `artifacts/rc_live/freenove_esp32s3_20260222-002556/`),
  - validation série live (`SC_LOAD/SC_EVENT`) avec logs UI montrant `transition=<type>:<ms>` sur changements de scène.
[20260222-002556] Run artefacts: /Users/cils/Documents/Enfants/anniv isaac 10a/le-mystere-professeur-zacus/hardware/firmware/artifacts/rc_live/freenove_esp32s3_20260222-002556, logs: /Users/cils/Documents/Enfants/anniv isaac 10a/le-mystere-professeur-zacus/hardware/firmware/logs/rc_live, summary: /Users/cils/Documents/Enfants/anniv isaac 10a/le-mystere-professeur-zacus/hardware/firmware/artifacts/rc_live/freenove_esp32s3_20260222-002556/summary.md
[20260222-025923] Run artefacts: `artifacts/rc_live/freenove_esp32s3_20260222-025923/`, logs: `logs/rc_live/`, summary: `artifacts/rc_live/freenove_esp32s3_20260222-025923/summary.md`

## [2026-02-22] Intégration complète Story + options Freenove (phases 1→3)

- [x] Runtime Freenove branché en modules:
  - `HardwareManager` intégré au cycle principal (`init/update/noteButton/setSceneHint`),
  - nouveaux modules `CameraManager` et `MediaManager` ajoutés et initialisés au boot.
- [x] Options Story (`data/story/apps`) ajoutées + fallback embarqué synchronisé:
  - `APP_HARDWARE.json`, `APP_CAMERA.json`, `APP_MEDIA.json`,
  - fallback LittleFS mis à jour dans `storage_manager.cpp` (bundle embarqué complet).
- [x] Interfaces publiques ajoutées:
  - série: `HW_*`, `CAM_*`, `MEDIA_*`, `REC_*`,
  - API WebUI: `/api/hardware*`, `/api/camera*`, `/api/media*`,
  - `/api/status` enrichi (`hardware`, `camera`, `media`),
  - commandes ESP-NOW `type=command` dispatchées sur les nouveaux contrôles.
- [x] Couplage Story actions au changement d'étape:
  - exécuteur d'`action_ids` branché,
  - nouvelles actions supportées (`ACTION_HW_LED_*`, `ACTION_CAMERA_SNAPSHOT`, `ACTION_MEDIA_PLAY_FILE`, `ACTION_REC_*`, etc.),
  - snapshot action `SERIAL:CAMERA_CAPTURED` émis en succès.
- [x] Évolution écran/effets Story:
  - effets ajoutés: `radar`, `wave`,
  - aliases transition ajoutés: `wipe -> slide_left`, `camera_flash -> glitch`,
  - scènes ajoutées: `SCENE_CAMERA_SCAN`, `SCENE_SIGNAL_SPIKE`, `SCENE_MEDIA_ARCHIVE`,
  - docs alignées: `docs/protocols/story_screen_palette_v2.md`, `docs/protocols/story_README.md`.
- [x] Validation réalisée (session courante):
  - builds: `pio run -e esp32dev -e esp32_release -e esp8266_oled -e ui_rp2040_ili9488 -e ui_rp2040_ili9486` ✅,
  - build Freenove: `pio run -e freenove_esp32s3` ✅,
  - FS Freenove: `pio run -e freenove_esp32s3_full_with_ui -t buildfs` ✅,
  - smoke orchestré: `ZACUS_ENV=freenove_esp32s3 ./tools/dev/run_matrix_and_smoke.sh` ✅ (run sans hardware -> smoke/UI link `SKIPPED`, justif. combined board + port non résolu; artefact `artifacts/rc_live/freenove_esp32s3_20260222-025923/`),
  - Story tooling: `./tools/dev/story-gen validate` ✅, `./tools/dev/story-gen generate-bundle --out-dir /tmp/story_bundle_options_20260222T035914` ✅,
  - tests Python: `.venv/bin/python3 -m pytest lib/zacus_story_gen_ai/tests/test_generator.py` ✅ (5 passed),
  - scénario canonique: `.venv/bin/python3 ../../tools/scenario/validate_scenario.py ../../game/scenarios/zacus_v1.yaml` ✅,
  - export brief: `.venv/bin/python3 ../../tools/scenario/export_md.py ../../game/scenarios/zacus_v1.yaml` ✅.
- [!] Limites constatées hors scope firmware:
  - `.venv/bin/python3 ../../tools/audio/validate_manifest.py ../../audio/manifests/zacus_v1_audio.yaml` ❌ (`game/prompts/audio/intro.md` manquant),
  - `.venv/bin/python3 ../../tools/printables/validate_manifest.py ../../printables/manifests/zacus_v1_printables.yaml` ❌ (prompts `printables/src/prompts/*.md` manquants),
  - aucune validation live `curl /api/*` ou cycle série ACK HW/CAM/MEDIA possible sans carte connectée pendant cette session.

## [2026-02-22] Correctif validateurs contenu + checks intégration API (local/GH/Codex/ChatGPT)

- [x] Correctif robustesse chemins pour exécution depuis `hardware/firmware/` **et** repo root:
  - `tools/audio/validate_manifest.py`: résolution `source` maintenant tolérante (`cwd`, dossier manifeste, repo root).
  - `tools/printables/validate_manifest.py`: résolution manifeste/prompt indépendante du `cwd` (fallbacks repo root + dossier manifeste).
  - `hardware/firmware/tools/dev/check_env.sh`: check env enrichi avec vérifications d'intégration API (`gh` auth, `codex login status`, reachability OpenAI) et diagnostic `MCP_DOCKER` (daemon Docker).
- [x] Revalidation post-correctif:
  - depuis `hardware/firmware`:  
    `.venv/bin/python3 ../../tools/audio/validate_manifest.py ../../audio/manifests/zacus_v1_audio.yaml` ✅  
    `.venv/bin/python3 ../../tools/printables/validate_manifest.py ../../printables/manifests/zacus_v1_printables.yaml` ✅
  - depuis repo root:  
    `hardware/firmware/.venv/bin/python3 tools/audio/validate_manifest.py audio/manifests/zacus_v1_audio.yaml` ✅  
    `hardware/firmware/.venv/bin/python3 tools/printables/validate_manifest.py printables/manifests/zacus_v1_printables.yaml` ✅
- [x] Cohérence firmware/story toolchain recheck:
  - `python ../../tools/scenario/validate_scenario.py ../../game/scenarios/zacus_v1.yaml` ✅
  - `./tools/dev/story-gen validate` ✅
  - `./tools/dev/story-gen generate-bundle --out-dir /tmp/story_bundle_api_check_<ts>` ✅
  - `.venv/bin/python3 -m pytest lib/zacus_story_gen_ai/tests/test_generator.py` ✅ (`5 passed`)
- [x] Vérification accès API outillage:
  - GitHub CLI: `gh auth status` ✅ (account `electron-rare`), `gh api user` ✅, `gh api rate_limit` ✅.
  - Codex CLI: `codex login status` ✅ (`Logged in using ChatGPT`), `codex exec --ephemeral` ✅ (réponse reçue).
  - OpenAI endpoint reachability: `curl https://api.openai.com/v1/models` => `401 Missing bearer` (connectivité OK, clé API absente côté shell).
- [!] Point d'intégration à traiter côté poste:
  - `codex mcp list` signale `MCP_DOCKER` activé mais non opérationnel si daemon Docker arrêté (`Cannot connect to the Docker daemon ...`).

## [2026-02-22] Réparation workspace — déplacement involontaire `esp8266_oled/src`

- [x] Analyse de l'anomalie:
  - suppressions trackées massives sous `hardware/firmware/ui/esp8266_oled/src/**`,
  - dossier non tracké détecté: `hardware/firmware/ui/esp8266_oled/src 2/`.
- [x] Correctif appliqué:
  - restauration de l'arborescence par renommage du dossier `src 2` vers `src`.
  - résultat: suppressions annulées, structure source ESP8266 OLED rétablie.
- [x] Validation:
  - `pio run -e esp8266_oled` ✅ (build complet OK après réparation).
