# Firmware ESP32 (Audio Kit A252)

Ce dossier contient le firmware principal pour **ESP32 Audio Kit V2.2 A252**.

## Licence firmware

- Code firmware: `GPL-3.0-or-later`
- Dependances open source: voir `OPEN_SOURCE.md`
- Politique de licence du depot: `../../../LICENSE.md`

## Gouvernance branches (double flux)

- Flux actifs: `main` et `codex/esp32-audio-mozzi-20260213`
- Regle de sync hebdomadaire:
  1. PR `sync/main-into-codex-<date>`
  2. validation build matrix firmware
  3. PR `sync/codex-into-main-<date>`
- Toute PR sprint doit indiquer explicitement sa base (`main` ou `codex/*`) et le plan de resynchronisation.

## Profil cible

- Carte: ESP32 Audio Kit V2.2 A252
- Flash interne: partition `no_ota.csv` + filesystem `LittleFS`
- SD: `SD_MMC` (slot microSD onboard, mode 1-bit)
- Audio:
  - boot: `MODE U_LOCK` avec pictogramme casse
  - en `U_LOCK`: appui sur une touche pour lancer la detection du LA (440 Hz, micro onboard)
  - affichage OLED pendant detection: bargraphe volume + bargraphe accordage + scope micro (optionnel si `kUseI2SMicInput=true`)
  - apres cumul de 3 secondes de detection LA (continue ou repetee): pictogramme de validation, puis passage en `MODULE U-SON Fonctionnel`
- scenario STORY:
    - fin `U_LOCK`: sequence story unifiee `unlock -> WIN` (random `*WIN*`, fallback effet I2S WIN)
    - puis `ETAPE_2` declenchee apres delai story (defaut 15 min)
    - mode lecteur SD autorise seulement apres `ETAPE_2`
  - ensuite: activation detection SD, puis passage auto en `MODE LECTEUR U-SON` si SD + fichiers audio supportes
- Touches: clavier analogique sur une seule entree ADC
- Ecran distant: ESP8266 NodeMCU OLED via UART

## Fichiers principaux

- Entree minimale Arduino: `src/main.cpp`
- Entree App minimale: `src/app.h`, `src/app.cpp`
- Orchestrateur applicatif: `src/app/app_orchestrator.h`, `src/app/app_orchestrator.cpp`
- Ordonnanceur des briques actives/inactives: `src/runtime/app_scheduler.h`, `src/runtime/app_scheduler.cpp`
- Etat runtime partage (objets + etats): `src/runtime/runtime_state.h`, `src/runtime/runtime_state.cpp`
- Compatibilite includes historiques: `src/app_state.h`
- Codec ES8388 (wrapper `arduino-audio-driver`): `src/audio/codec_es8388_driver.h`, `src/audio/codec_es8388_driver.cpp`
- Config hardware/audio: `src/config.h`
- Lecteur audio SD_MMC + I2S (multi-format): `src/audio/mp3_player.h`, `src/audio/mp3_player.cpp`
- Generation tonalite/jingle: `src/audio/sine_dac.h`, `src/audio/sine_dac.cpp`, `src/audio/i2s_jingle_player.h`, `src/audio/i2s_jingle_player.cpp`
- Touches analogiques: `src/input/keypad_analog.h`, `src/input/keypad_analog.cpp`
- UI LED: `src/ui/led_controller.h`, `src/ui/led_controller.cpp`
- Lien ecran ESP8266: `src/screen/screen_link.h`, `src/screen/screen_link.cpp`, `src/screen/screen_frame.h`
- Sync ecran (seq + envoi trames): `src/services/screen/screen_sync_service.h`, `src/services/screen/screen_sync_service.cpp`
- Runtime boot: `src/controllers/boot/boot_protocol_runtime.h`, `src/controllers/boot/boot_protocol_runtime.cpp`
- Runtime story legacy: `src/controllers/story/story_controller.h`, `src/controllers/story/story_controller.cpp`
- Runtime story V2: `src/controllers/story/story_controller_v2.h`, `src/controllers/story/story_controller_v2.cpp`
- Service runtime LA (hold/unlock non bloquant): `src/services/la/la_detector_runtime_service.h`, `src/services/la/la_detector_runtime_service.cpp`
- Moteur STORY legacy: `src/story/story_engine.h`, `src/story/story_engine.cpp`
- Moteur STORY V2: `src/story/core/story_engine_v2.h`, `src/story/core/story_engine_v2.cpp`
- Mini apps STORY V2: `src/story/apps/*`
- Scenarios/ressources V2: `src/story/scenarios/*`, `src/story/resources/*`, `src/story/core/scenario_def.h`
- Code STORY genere: `src/story/generated/*`
- Specs STORY auteur: `story_specs/schema/*`, `story_specs/templates/*`, `story_specs/scenarios/*`
- Prompts auteurs STORY: `story_specs/prompts/*`
- Generateur STORY: `tools/story_gen/story_gen.py`
- Guide scenario STORY: `src/story/README.md`
- Guide rapide auteur scenario V2: `GENERER_UN_SCENARIO_STORY_V2.md`
- Service scan catalogue SD non bloquant: `src/services/storage/catalog_scan_service.h`, `src/services/storage/catalog_scan_service.cpp`
- Memo board A252: `README_A252.md`
- Cablage: `WIRING.md`
- Validation terrain: `TESTING.md`
- Commandes de travail: `Makefile`
- Assets LittleFS (sons internes): `data/`
- Archives non actives: `old/` (ex: sources audio de travail)

## GPIO utilises (A252)

- I2S codec:
  - `BCLK GPIO27`
  - `LRCK GPIO25`
  - `DOUT GPIO26`
- Enable ampli: `GPIO21`
- UART vers ESP8266 (TX uniquement): `GPIO22`
- Touches analogiques (ADC): `GPIO36`
- Micro codec onboard (I2S DIN): `GPIO35`
- Fallback micro analogique externe (optionnel): `GPIO34` (si `kUseI2SMicInput=false`)

## Cablage ESP32 -> NodeMCU OLED

- `ESP32 GPIO22 (TX)` -> `NodeMCU D6 (RX)`
- `ESP32 GND` <-> `NodeMCU GND`
- `NodeMCU D5 (TX)` non utilise (laisser deconnecte)

## Actions des touches

### Protocole boot audio

- Au demarrage, le firmware lit un FX boot LittleFS (par defaut `uson_boot_arcade_lowmono.mp3`, cible ~20 s).
- Ensuite il lance un scan radio I2S continu (bruit FM/recherche) en boucle.
- Pendant cette phase, les commandes de mode normal sont bloquees.
- Le passage a l'app suivante se fait sur appui d'une touche `K1..K6`.
- Un timeout auto est possible si `kBootAudioValidationTimeoutMs > 0` dans `src/config.h`
  (par defaut `0`, donc timeout desactive).

Touches actives:

- `K1..K6` : passage a l'etape suivante (detection LA)

Commandes serie (moniteur ESP32):

- `BOOT_NEXT`
- `BOOT_REPLAY` (relit l'intro + relance le scan radio)
- `BOOT_STATUS`
- `BOOT_HELP`
- `BOOT_TEST_TONE`
- `BOOT_TEST_DIAG`
- `BOOT_PA_ON`
- `BOOT_PA_OFF`
- `BOOT_PA_STATUS`
- `BOOT_PA_INV` (inverse la polarite PA active high/active low)
- `BOOT_FS_INFO` (etat/capacite LittleFS)
- `BOOT_FS_LIST` (liste des fichiers LittleFS)
- `BOOT_FS_TEST` (joue le FX boot detecte depuis LittleFS)
- `BOOT_REOPEN` (relance intro + scan sans reset)
- `CODEC_STATUS` (etat codec I2C + volumes de sortie lus)
- `CODEC_DUMP` ou `CODEC_DUMP 0x00 0x31` (dump registres codec)
- `CODEC_RD 0x2E` / `CODEC_WR 0x2E 0x10` (lecture/ecriture registre brut)
- `CODEC_VOL 70` (volume codec + gain logiciel lecteur a 70%)
- `CODEC_VOL_RAW 0x12` (force volume brut registres 0x2E..0x31)
- Les aliases historiques sont desactives: utiliser uniquement les commandes canoniques `PREFIXE_ACTION`.

Le firmware publie un rappel periodique tant que l'attente touche est active.
Hors fenetre boot: les commandes BOOT qui declenchent de l'audio sont autorisees uniquement en `U_LOCK`.
En `U-SON`/`MP3`, seules les commandes de statut restent autorisees (`BOOT_STATUS`, `BOOT_HELP`, `BOOT_PA_STATUS`, `BOOT_FS_INFO`, `BOOT_FS_LIST`).

### Scenario STORY (serial)

Commandes scenario (moniteur ESP32):

- `STORY_STATUS`
- `STORY_HELP`
- `STORY_RESET`
- `STORY_ARM`
- `STORY_TEST_ON` / `STORY_TEST_OFF`
- `STORY_TEST_DELAY <ms>` (borne 100..300000)
- `STORY_FORCE_ETAPE2`
- `STORY_V2_STATUS`
- `STORY_V2_LIST`
- `STORY_V2_VALIDATE`
- `STORY_V2_HEALTH`
- `STORY_V2_METRICS`
- `STORY_V2_METRICS_RESET`
- `STORY_V2_ENABLE [STATUS|ON|OFF]`
- `STORY_V2_TRACE [ON|OFF|STATUS]`
- `STORY_V2_TRACE_LEVEL [OFF|ERR|INFO|DEBUG|STATUS]`
- `STORY_V2_EVENT <name>`
- `STORY_V2_STEP <id>`
- `STORY_V2_SCENARIO <id>`

Notes:

- Le mode test STORY est pilote uniquement par commandes serie (pas de raccourci clavier dedie).
- `STORY_ARM` lance maintenant le scenario complet: armement + tentative de lecture `WIN`.
- `STORY_STATUS` expose un `stage` explicite: `WAIT_UNLOCK`, `WIN_PENDING`, `WAIT_ETAPE2`, `ETAPE2_DONE`.
- Le moteur STORY V2 est protege par le flag `kStoryV2EnabledDefault` (default `true`).
- Si le flag V2 est OFF, `STORY_V2_EVENT/STEP/SCENARIO/VALIDATE/HEALTH/METRICS` repondent `OUT_OF_CONTEXT`.
- Rollback runtime immediat: `STORY_V2_ENABLE OFF` (retour controleur legacy sans reflash).
- Rollback release: remettre `kStoryV2EnabledDefault=false` puis recompiler/reflasher.
- Les delais par defaut sont configures dans `src/config.h`:
  - `kStoryEtape2DelayMs` (production, defaut 15 min)
  - `kStoryEtape2TestDelayMs` (test rapide)

Workflow auteur STORY V2:

- `make story-validate` (strict)
- `make story-gen` (strict + `spec_hash`)
- `make qa-story-v2`
- `make qa-story-v2-smoke` (debut sprint, flash + smoke serie)
- `make qa-story-v2-smoke-fast` (sans flash)
- checklist review sprint: `tools/qa/story_v2_review_checklist.md`
- `pio run -e esp32dev` (profil dev, Story V2 ON)
- `pio run -e esp32_release` (profil release, Story V2 OFF)

Un nouveau scenario est ajoute via `story_specs/scenarios/*.yaml`, puis generation C++ dans `src/story/generated/*`.

Scenarios compilés actuels (selection runtime):

- `DEFAULT`
- `EXAMPLE_UNLOCK_EXPRESS`
- `EXEMPLE_UNLOCK_EXPRESS_DONE`
- `SPECTRE_RADIO_LAB` (optionnel RC2)

Selection serie:

- `STORY_V2_SCENARIO SPECTRE_RADIO_LAB`

### Mode U_LOCK (au boot, SD active)

- Ecran initial: module casse avec effet glitch (sans texte)
- Apres appui touche: detection LA active + affichage accordage/volume/scope
- Le LA doit cumuler 3 secondes de detection (continue ou repetee) pour deverrouiller
- Les touches SIGNAL restent bloquees tant que le LA n'est pas detecte
- Le service SD (mount/scan) reste actif, mais le playback MP3 reste gate tant que `MODULE U-SON Fonctionnel` n'est pas atteint

### Module U-SON fonctionnel (apres detection du LA)

- `K1` : LA detect on/off
- `K2` : FX FM sweep I2S (asynchrone)
- `K3` : FX sonar I2S (asynchrone)
- `K4` : replay FX boot I2S
- `K5` : demande refresh SD
- `K6` : lance une calibration micro serie (30 s)
- Note: profil A252 en mode `I2S-only` (DAC analogique desactive).

### Mode lecteur (SD detectee)

- UI V3.1 (OLED 128x64): pages `LECTURE`, `LISTE`, `REGLAGES`
- `K1` : action contextuelle (`OK` / play-pause / selection / appliquer reglage)
- `K2` : `UP` (navigation liste/reglages)
- `K3` : `DOWN` (navigation liste/reglages)
- `K4` : `LEFT` (piste/station precedente, ou ajustement reglage)
- `K5` : `RIGHT` (piste/station suivante, ou ajustement reglage)
- `K6` : `MODE/BACK` (page suivante, appui long: page precedente, en `LECTURE`: bascule source SD<->RADIO)
- Les commandes serie `MP3_UI NAV ...` et `MP3_UI SOURCE ...` restent equivalentes aux touches.

Le firmware bascule automatiquement selon la SD:
- SD presente + pistes audio supportees: `MODE LECTEUR U-SON`
- SD absente: `MODE U_LOCK`, puis passage automatique en `MODULE U-SON Fonctionnel` apres detection du LA.
- Note: en `U_LOCK`, la SD peut etre montee/scannee pour garder un etat pret.

## Calibration micro (serial)

Le firmware lance automatiquement une calibration micro de 30 s a l'entree en mode SIGNAL.
Tu peux relancer la calibration a tout moment avec `K6` (en `U_LOCK` ou en SIGNAL).

Logs attendus:

- `"[MIC_CAL] START ..."`
- `"[MIC_CAL] left=... det=... off=... conf=... ratio=... rms=... p2p=... health=..."`
- `"[MIC_CAL] SUMMARY ..."` + `"[MIC_CAL] DIAG ..."`

## Build / Flash

Depuis la racine de ce dossier (`hardware/firmware/esp32`):

### Option 1 (recommandee): via Makefile

1. Afficher les commandes:
   - `make help`
2. Build complet:
   - `make build`
3. Flasher:
   - `make upload-esp32 ESP32_PORT=/dev/ttyUSB0`
   - `make uploadfs-esp32 ESP32_PORT=/dev/ttyUSB0`
   - `make erasefs-esp32 ESP32_PORT=/dev/ttyUSB0` (reset partition LittleFS)
   - `make upload-screen SCREEN_PORT=/dev/ttyUSB1`
4. Monitor:
   - `make monitor-esp32 ESP32_PORT=/dev/ttyUSB0`
   - `make monitor-screen SCREEN_PORT=/dev/ttyUSB1`

### Option 2: via PlatformIO direct

1. ESP32 principal:
   - `pio run -e esp32dev`
   - `pio run -e esp32_release`
   - `pio run -e esp32dev -t upload --upload-port /dev/ttyUSB0`
   - `pio run -e esp32_release -t upload --upload-port /dev/ttyUSB0`
   - `pio run -e esp32dev -t uploadfs --upload-port /dev/ttyUSB0`
   - `pio device monitor -e esp32dev --port /dev/ttyUSB0`
2. ESP8266 OLED:
   - `pio run -e esp8266_oled`
   - `pio run -e esp8266_oled -t upload --upload-port /dev/ttyUSB1`
   - `pio device monitor -e esp8266_oled --port /dev/ttyUSB1`
   - (sous-projet ecran) `cd screen_esp8266_hw630 && pio run -e nodemcuv2`

Sans variable `PORT`, PlatformIO choisit automatiquement le port serie.

### Test live (ordre recommande)

1. Brancher l'ESP32 en USB.
2. Uploader ESP32:
   - `pio run -e esp32dev -t upload --upload-port <PORT_ESP32>`
   - optionnel assets: `pio run -e esp32dev -t uploadfs --upload-port <PORT_ESP32>`
3. Brancher l'ESP8266 OLED en USB.
4. Uploader ESP8266:
   - `pio run -e esp8266_oled -t upload --upload-port <PORT_ESP8266>`
5. Ouvrir les moniteurs:
   - `pio device monitor -e esp32dev --port <PORT_ESP32>`
   - `pio device monitor -e esp8266_oled --port <PORT_ESP8266>`

Astuce detection ports:
- `pio device list`
- Runbook semi-auto Story V2: `tools/qa/live_story_v2_runbook.md`
- Smoke debut sprint: `tools/qa/live_story_v2_smoke.sh`
- Runbook release candidate: `tools/qa/live_story_v2_rc_runbook.md`
- Smoke RC MP3: `tools/qa/mp3_rc_smoke.sh`
- Runbook RC MP3: `tools/qa/mp3_rc_runbook.md`
- Smoke client MP3: `tools/qa/mp3_client_demo_smoke.sh`
- Checklist live client MP3: `tools/qa/mp3_client_live_checklist.md`
- Kit presentation client MP3: `docs/client/mp3/`
- Handbook release/rollback: `RELEASE_STORY_V2.md`

### CI / Review policy

- Workflow GitHub Actions: `.github/workflows/firmware-story-v2.yml`
- PR template avec gate STV2: `.github/PULL_REQUEST_TEMPLATE.md`
- Checklist review ticketisee: `tools/qa/story_v2_review_checklist.md`
- En CI:
  - `make story-validate`
  - `make story-gen`
  - `bash tools/qa/story_v2_ci.sh` (mode strict/idempotence)
  - builds firmware `esp32dev`, `esp32_release`, `esp8266_oled`, `screen:nodemcuv2`

## Lecteur audio evolue

Le lecteur:

- detecte/monte la SD automatiquement
- indexe les pistes supportees de facon recursive (profondeur max 4): `.mp3`, `.wav`, `.aac`, `.flac`, `.opus`, `.ogg`
- execute le scan en mode non bloquant (budget runtime par tick) pour garder clavier/ecran/serie reactifs
- force un rescan immediat sur `K5` (mode SIGNAL)
- gere une playlist triee
- enchaine automatiquement les pistes
- supporte repeat `ALL/ONE`
- expose piste courante + volume vers l'ecran ESP8266

Commandes MP3 utiles:

- `MP3_SCAN STATUS` : etat scan (`IDLE/REQUESTED/RUNNING/DONE/FAILED/CANCELED`)
- `MP3_SCAN START` : scan incremental (index prioritaire)
- `MP3_SCAN REBUILD` : rebuild force sans index
- `MP3_SCAN CANCEL` : annule un scan en cours
- `MP3_SCAN_PROGRESS` : progression scan live (state/pending/reason/depth/files/tracks/ticks/budget)
- `MP3_BACKEND_STATUS` : compteurs runtime backend (attempts/fail/retry/fallback)
- `MP3_UI_STATUS` : etat UI courant (`page/cursor/offset/browse/queue_off/set_idx`)
- `MP3_QUEUE_PREVIEW [n]` : projection des prochaines pistes
- `MP3_CAPS` : capacites codec/backend exposees au runtime

Kit presentation client MP3:

- `docs/client/mp3/01_pitch_executif.md`
- `docs/client/mp3/02_architecture_stack.md`
- `docs/client/mp3/03_demo_live_script.md`
- `docs/client/mp3/04_evidence_qa.md`
- `docs/client/mp3/05_risques_et_mitigations.md`
- `docs/client/mp3/06_qna_client.md`
- `docs/client/mp3/07_decisions_next_steps.md`
- `tools/qa/mp3_client_demo_smoke.sh`
- `tools/qa/mp3_client_live_checklist.md`
Commandes SD utiles:

- `SD_STATUS` : etat stockage (mount + scan)
- `SD_MOUNT` : demande de montage SD
- `SD_UNMOUNT` : demande de demontage SD
- `SD_RESCAN [FORCE]` : relance scan catalogue
- `SD_SCAN_PROGRESS` : progression scan (alias orientee stockage)

Commandes Radio/WiFi/Web utiles:

- `RADIO_STATUS`, `RADIO_LIST [offset limit]`, `RADIO_PLAY <id|url>`, `RADIO_STOP`, `RADIO_NEXT`, `RADIO_PREV`, `RADIO_META`
- `WIFI_STATUS`, `WIFI_SCAN`, `WIFI_CONNECT <ssid> <pass>`, `WIFI_AP_ON [ssid pass]`, `WIFI_AP_OFF`
- `WEB_STATUS`

## WebUI smartphone

La WebUI est exposee sur l'ESP32 (port configure dans `data/net/config.json`, defaut `80`).

- URL: `http://<ip_esp32>/`
- Vue mobile responsive: pilotage lecteur + radio + wifi + statut live
- Auth optionnelle:
  - `data/net/config.json > web.auth` (`true|false`)
  - `data/net/config.json > web.user`
  - `data/net/config.json > web.pass`

Sons internes:

- Le boot tente d'abord un FX depuis `LittleFS`:
  - priorite: chemin configure `kBootFxLittleFsPath` (defaut `/uson_boot_arcade_lowmono.mp3`)
  - puis auto-detection: `/boot.mp3`, `/boot.wav`, `/boot.aac`, `/boot.flac`, `/boot.opus`, `/boot.ogg`
  - fallback final: premier fichier audio supporte trouve dans la racine LittleFS
- Si absent/invalide, fallback automatique sur le bruit radio I2S genere.
- Profil recommande: `uson_boot_arcade_lowmono.mp3` narratif ~20 s en LittleFS.

## Calibration touches analogiques

Les seuils sont dans `src/config.h`:

- `kKey1Max` .. `kKey6Max`
- `kKeysReleaseThreshold`

Procedure:

1. Ouvrir le moniteur serie.
2. Appuyer chaque touche.
3. Relever les logs `[KEY] Kx raw=...`.
4. Ajuster les seuils pour separer les 6 zones.

Reglage live (sans reflash):

- `KEY_STATUS` : affiche les seuils actifs + valeur brute courante
- `KEY_RAW_ON` / `KEY_RAW_OFF` : flux live `[KEY_RAW] raw=... stable=K...`
- `KEY_SET K4 1500` : change la borne max de `K4`
- `KEY_SET K6 2200` : change la borne max de `K6`
- `KEY_SET REL 3920` : change le seuil de relache
- `KEY_SET_ALL k1 k2 k3 k4 k5 k6 rel` : applique tous les seuils d'un coup
- `KEY_RESET` : retour aux valeurs de `src/config.h`
- `KEY_TEST_START` : demarre un auto-test K1..K6 (sans actions metier)
- `KEY_TEST_STATUS` : etat `OK/KO` + min/max `raw` par touche
- `KEY_TEST_RESET` : remet l'auto-test a zero
- `KEY_TEST_STOP` : arrete l'auto-test
- `BOOT_FS_INFO` / `BOOT_FS_LIST` / `BOOT_FS_TEST` : debug LittleFS et test lecture FX boot
- `STORY_STATUS` / `STORY_TEST_ON` / `STORY_TEST_OFF` / `STORY_TEST_DELAY` / `STORY_ARM` / `STORY_FORCE_ETAPE2` : pilotage scenario STORY
- `STORY_V2_ENABLE` / `STORY_V2_TRACE` / `STORY_V2_TRACE_LEVEL` / `STORY_V2_STATUS` / `STORY_V2_LIST` / `STORY_V2_VALIDATE` / `STORY_V2_HEALTH` / `STORY_V2_METRICS` / `STORY_V2_METRICS_RESET` / `STORY_V2_EVENT` / `STORY_V2_STEP` / `STORY_V2_SCENARIO` : debug/migration STORY V2
- `MP3_SCAN_PROGRESS` / `MP3_BACKEND_STATUS` : observabilite lecteur MP3
- `SYS_LOOP_BUDGET STATUS|RESET` / `SCREEN_LINK_STATUS` / `SCREEN_LINK_RESET_STATS` : diagnostics runtime/screen link
- `CODEC_STATUS` / `CODEC_DUMP` : debug codec I2C ES8388
- `CODEC_RD reg` / `CODEC_WR reg val` : lecture/ecriture registre codec
- `CODEC_VOL pct` / `CODEC_VOL_RAW raw [out2]` : reglage volume sortie codec

Contraintes:

- ordre strict obligatoire: `K1 < K2 < K3 < K4 < K5 < K6 < REL`
