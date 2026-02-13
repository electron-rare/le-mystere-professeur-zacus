# Checklist de validation

Cette checklist couvre le comportement attendu du couple ESP32 + ESP8266 OLED.

## Prerequis

- Firmware ESP32 flashe.
- Firmware ESP8266 OLED flashe.
- Si sons internes utilises: image LittleFS flashee (`make uploadfs-esp32 ...`).
- Liaison `GPIO22 -> D6` et `GND <-> GND` cablee.
- Moniteur serie disponible sur les deux cartes.
- Ports series identifies (`pio device list`).

## Upload live guide (2 cartes)

1. Brancher uniquement l'ESP32.
2. Uploader:
   - `pio run -e esp32dev -t upload --upload-port <PORT_ESP32>`
   - optionnel: `pio run -e esp32dev -t uploadfs --upload-port <PORT_ESP32>`
3. Brancher ensuite l'ESP8266 OLED.
4. Uploader:
   - `pio run -e esp8266_oled -t upload --upload-port <PORT_ESP8266>`
   - ou dans `screen_esp8266_hw630`: `pio run -e nodemcuv2 -t upload --upload-port <PORT_ESP8266>`
5. Verifier les 2 moniteurs:
   - `pio device monitor -e esp32dev --port <PORT_ESP32>`
   - `pio device monitor -e esp8266_oled --port <PORT_ESP8266>`

Runbook live semi-automatique disponible:

- `tools/qa/live_story_v2_runbook.md`
- `tools/qa/live_story_v2_smoke.sh` (smoke debut sprint)
- `tools/qa/live_story_v2_rc_runbook.md` (release candidate)
- `tools/qa/mp3_rc_smoke.sh` (smoke RC MP3)
- `tools/qa/mp3_rc_runbook.md` (runbook RC MP3)

## 0) Tooling STORY V2 (hors carte)

1. Validation stricte:
   - `make story-validate`
2. Generation C++ stricte:
   - `make story-gen`
3. Verifier idempotence + build matrix:
   - `make qa-story-v2`
   - `pio run -e esp32_release`
4. Attendu:
   - aucun diff apres generation repetee
   - hash spec visible dans `src/story/generated/*.h` et `src/story/generated/*.cpp`

## 0b) Smoke debut sprint (optionnel mais recommande)

Commande standard:

- `make qa-story-v2-smoke ESP32_PORT=<PORT_ESP32> SCREEN_PORT=<PORT_ESP8266>`

Sans reflash (controle rapide runtime):

- `make qa-story-v2-smoke-fast ESP32_PORT=<PORT_ESP32>`

## 1) Boot sans SD

1. Demarrer l'ESP32 sans carte SD.
2. Verifier les logs ESP32:
   - `[MODE] U_LOCK (appuyer touche pour detecter LA)`
   - pas de montage SD immediat
   - `[BOOT_PROTO] START ...`
   - intro audio LittleFS (par defaut `uson_boot_arcade_lowmono.mp3`) puis scan radio I2S
   - `[KEYMAP][BOOT_PROTO] K1..K6=NEXT ...`
3. Verifier l'OLED:
   - pictogramme casse + attente appui touche

## 1b) Validation audio boot (touches + serial)

1. Pendant la fenetre boot (attente touche):
   - verifier que le scan radio I2S tourne en continu
   - appuyer `K1..K6`: verifier `[BOOT_PROTO] DONE status=VALIDATED ...`
2. Validation serial:
   - note: aliases legacy desactives, utiliser uniquement les commandes canoniques `PREFIXE_ACTION`
   - envoyer `BOOT_STATUS` puis verifier `waiting_key=1 ...`
   - envoyer `BOOT_REPLAY` pour relire intro + relancer scan
   - envoyer `BOOT_TEST_TONE` puis `BOOT_TEST_DIAG` pour test audio
   - envoyer `BOOT_PA_STATUS` (et si besoin `BOOT_PA_ON`)
   - si silence audio: envoyer `BOOT_PA_INV` puis re-tester `BOOT_TEST_TONE`
   - envoyer `BOOT_FS_INFO` puis `BOOT_FS_LIST`
   - optionnel: envoyer `BOOT_FS_TEST` pour lire le FX boot LittleFS
   - envoyer `BOOT_NEXT` pour passer a l'etape suivante
   - envoyer `BOOT_REOPEN` pour relancer le protocole sans reset carte
   - si `kBootAudioValidationTimeoutMs > 0`, verifier aussi le passage auto au timeout

## 1c) Validation codec I2C (ES8388) pas a pas

1. Verifier la presence codec:
   - envoyer `CODEC_STATUS`
   - attendu: `ready=1` et `addr=0x10` (ou `0x11` selon carte)
2. Verifier les registres audio clefs:
   - envoyer `CODEC_DUMP`
   - verifier que les registres sortent sans `<ERR>`
3. Verifier lecture/ecriture registre brute:
   - envoyer `CODEC_RD 0x2E`
   - envoyer `CODEC_WR 0x2E 0x10`
   - envoyer `CODEC_RD 0x2E` et verifier la nouvelle valeur
4. Verifier volume codec pilote:
   - envoyer `CODEC_VOL 30` puis `BOOT_TEST_TONE`
   - envoyer `CODEC_VOL 80` puis `BOOT_TEST_TONE`
   - le niveau casque doit monter clairement
5. Si besoin, forcer brut:
   - envoyer `CODEC_VOL_RAW 0x08`
   - puis `CODEC_VOL_RAW 0x1C`
   - comparer le niveau audio
6. Si aucun son:
   - verifier `BOOT_PA_STATUS`
   - envoyer `BOOT_PA_INV` puis re-tester `BOOT_TEST_TONE`

## 2) Unlock LA

1. Appuyer sur une touche (`K1..K6`) pendant le boot pour sortir du scan radio et lancer la detection LA.
2. Verifier l'OLED:
   - ecran `MODE U_LOCK` en detection
   - bargraphe volume + bargraphe accordage
3. Produire un LA stable (440 Hz) vers le micro.
4. Verifier les logs ESP32:
   - `[MODE] MODULE U-SON Fonctionnel (LA detecte)`
   - `[SD] Detection SD activee.`
   - La detection LA doit cumuler 3 secondes (continue ou repetee) avant deverrouillage.
5. Verifier l'OLED:
   - pictogramme de validation
   - puis ecran `U-SON FONCTIONNEL`

## 2b) Scenario STORY legacy (forcer V2 OFF)

Objectif: valider la logique STORY sans attendre 15 minutes.

Prerequis:

- Etre en `MODULE U-SON Fonctionnel` (unlock fait).
- Moniteur serie ESP32 ouvert.
- Envoyer `STORY_V2_ENABLE OFF` avant le test legacy.

Procedure rapide:

1. Envoyer `STORY_STATUS` pour lire l'etat initial.
2. Envoyer `STORY_TEST_ON`.
3. Envoyer `STORY_TEST_DELAY 5000` (5 s).
4. Envoyer `STORY_ARM` pour armer la timeline.
5. Verifier `STORY_STATUS`:
   - `stage=WAIT_ETAPE2` (ou `WIN_PENDING` si audio WIN en cours)
   - `armed=1`
   - `win=1` (ou passe a 1 apres fin du WIN)
   - `etape2=0`
   - `test=1`
   - `left~5s`
6. Deux options de declenchement `ETAPE_2`:
   - attendre 5 s
   - ou envoyer `STORY_FORCE_ETAPE2`
7. Verifier les logs:
   - `[STORY] ETAPE_2 trigger.`
   - `[STORY] ETAPE_2 done ...`
8. Verifier `STORY_STATUS` final:
   - `stage=ETAPE2_DONE`
   - `etape2=1`
9. Envoyer `STORY_TEST_OFF` pour revenir aux delais de prod.

## 2c) Scenario STORY V2 (flag V2 ON)

Objectif: valider le moteur data-driven et les commandes V2.

Prerequis:

- `kStoryV2EnabledDefault=true` au compile-time (defaut actuel) ou activation runtime `STORY_V2_ENABLE ON`.
- Etre en `MODULE U-SON Fonctionnel`.
- Moniteur serie ESP32 ouvert.
- Scenario YAML genere a jour:
  - `make story-validate`
  - `make story-gen`
  - `make qa-story-v2` (recommande avant test live)

Procedure rapide:

1. Envoyer `STORY_V2_STATUS` et verifier `enabled=1`.
   - sinon envoyer `STORY_V2_ENABLE ON`
2. Envoyer `STORY_V2_TRACE_LEVEL STATUS`, puis `STORY_V2_TRACE_LEVEL DEBUG` pour la session.
3. Envoyer `STORY_V2_LIST` puis verifier `DEFAULT` et `SPECTRE_RADIO_LAB`.
4. (optionnel RC2) Basculer de scenario:
   - `STORY_V2_SCENARIO SPECTRE_RADIO_LAB`
   - verifier `STORY_V2_STATUS` (`scenario=SPECTRE_RADIO_LAB`)
5. Envoyer `STORY_V2_VALIDATE` et verifier `OK valid`.
6. Envoyer `STORY_V2_HEALTH`:
   - attendu initial: `OK` ou `BUSY`
7. Envoyer `STORY_TEST_ON`.
8. Envoyer `STORY_TEST_DELAY 5000`.
9. Envoyer `STORY_ARM`.
10. Verifier `STORY_STATUS` ou `STORY_V2_STATUS`:
   - progression `STEP_WAIT_UNLOCK -> STEP_WIN -> STEP_WAIT_ETAPE2`
11. Forcer un event timer si besoin:
   - `STORY_V2_EVENT ETAPE2_DUE`
   - ou `STORY_FORCE_ETAPE2`
12. Verifier transition vers `STEP_ETAPE2` puis `STEP_DONE`.
13. Envoyer `STORY_V2_HEALTH`:
   - attendu final: `OK`
14. Envoyer `STORY_V2_METRICS` et verifier transitions/events > 0.
15. Verifier que le gate MP3 est ouvert en fin de flux.
16. Envoyer `STORY_V2_TRACE_LEVEL OFF`.
17. Envoyer `STORY_V2_SCENARIO DEFAULT` (retour nominal).
18. Envoyer `STORY_V2_METRICS_RESET` puis `STORY_TEST_OFF`.

Rollback (si anomalie en live):

1. Envoyer `STORY_V2_ENABLE OFF` pour revenir sur le controleur legacy sans reflash.
2. Verifier `STORY_STATUS` et `STORY_V2_STATUS` (attendu: V2 desactive).
3. Pour un rollback durable, remettre `kStoryV2EnabledDefault=false`, recompiler et reflasher l'ESP32.

## 3) SD et lecteur audio

1. Inserer une SD avec au moins un fichier audio supporte a la racine (`.mp3`, `.wav`, `.aac`, `.flac`, `.opus`, `.ogg`).
2. Verifier les logs ESP32:
   - `[MP3] SD_MMC mounted.`
   - `[MP3] x track(s) loaded.`
   - `[MODE] LECTEUR U-SON (SD detectee)`
3. Verifier l'OLED:
   - ecran `LECTEUR U-SON`
   - piste + volume visibles

## 3b) LittleFS (sons internes)

1. Flasher la partition LittleFS:
   - `make uploadfs-esp32 ESP32_PORT=...`
2. Redemarrer l'ESP32.
3. Verifier les logs:
   - `[FS] ... LittleFS mounted ...`
   - `[FS] Boot FX ready: ...` (si fichier boot present)
4. En serie:
   - `BOOT_FS_INFO`
   - `BOOT_FS_LIST`
   - `BOOT_FS_TEST` (lecture du FX boot detecte)

## 4) Touches

1. En `U_LOCK`:
   - verifier que `K1..K5` sont ignorees
   - verifier que `K6` lance la calibration micro
2. En `U-SON FONCTIONNEL`:
   - `K1`: toggle detection LA
   - `K2`: FX FM sweep I2S (asynchrone)
   - `K3`: FX sonar I2S (asynchrone)
   - `K4`: replay FX boot I2S
   - `K5`: refresh SD (rescan immediat)
   - `K6`: calibration micro
   - verifier en log que les actions K2/K3/K4 declenchent bien l'audio I2S.
3. En mode lecteur:
   - `K1`: play/pause
   - `K2/K3`: prev/next
   - `K4/K5`: volume -/+
   - `K6`: repeat ALL/ONE

## 4b) Reglage live seuils clavier (K4/K6)

1. Ouvrir le moniteur serie ESP32.
2. Envoyer `KEY_STATUS` pour lire les seuils actifs.
3. Envoyer `KEY_RAW_ON`, appuyer plusieurs fois `K4` puis `K6`, relever les valeurs `raw`.
4. Ajuster les bornes:
   - `KEY_SET K4 <valeur>`
   - `KEY_SET K6 <valeur>`
   - si besoin `KEY_SET REL <valeur>`
5. Verifier:
   - chaque appui K4 log bien `[KEY] K4 raw=...`
   - chaque appui K6 log bien `[KEY] K6 raw=...`
6. Couper le flux live avec `KEY_RAW_OFF`.
7. Si besoin, revenir config compile-time avec `KEY_RESET`.

## 4c) Validation brique K1..K6

1. Envoyer `KEY_TEST_START`.
2. Appuyer une fois sur chaque touche `K1` a `K6` (ordre libre).
3. Verifier les logs:
   - `[KEY_TEST] HIT Kx raw=...`
   - `[KEY_TEST] SUCCESS: K1..K6 valides.`
4. En cas de doute, envoyer `KEY_TEST_STATUS` pour voir `seen=.../6` et les min/max raw.

## 5) Robustesse lien UART

1. Deconnecter temporairement le fil `GPIO22 -> D6`.
2. Verifier l'OLED:
   - etat `RECONNEXION MODULE` d'abord (grace transitoire)
   - puis `LINK DOWN` seulement si la liaison ne revient pas (grace ~30 s)
3. Reconnecter le fil.
4. Verifier retour automatique vers un ecran de mode.

## 6) Retrait SD en lecture

1. Demarrer une lecture audio.
2. Retirer la SD.
3. Verifier les logs ESP32:
   - `[MP3] SD removed/unmounted.`
4. Verifier retour mode signal/U_LOCK selon l'etat LA.

## 7) Scan MP3 non bloquant (serie)

1. Envoyer `MP3_SCAN STATUS` et verifier `state=IDLE|DONE`.
2. Envoyer `MP3_SCAN START` puis, pendant le scan:
   - appuyer sur les touches (logs toujours reactifs)
   - verifier l'ecran (pas de freeze)
   - verifier la serie (`MP3_SCAN STATUS` retourne `REQUESTED` ou `RUNNING`)
3. Envoyer `MP3_SCAN CANCEL` et verifier la reponse:
   - `OK canceled` si un scan etait actif
   - `OUT_OF_CONTEXT idle` sinon
4. Envoyer `MP3_SCAN REBUILD` et verifier la fin:
   - `state=DONE`
   - `tracks>0` si des fichiers supportes sont presents
5. Envoyer `MP3_SCAN_PROGRESS`:
   - verifier `state/pending/reason/depth/stack/folders/files/tracks/ticks/budget`
6. Envoyer `MP3_BACKEND_STATUS`:
   - verifier compteurs `attempts/fail/retries/fallback`

## 7b) UI MP3 NOW/BROWSE/QUEUE/SET

1. Envoyer `MP3_UI_STATUS`:
   - verifier `page`, `cursor`, `offset`, `browse`, `queue_off`, `set_idx`
2. Envoyer successivement:
   - `MP3_UI PAGE NOW`
   - `MP3_UI PAGE BROWSE`
   - `MP3_UI PAGE QUEUE`
   - `MP3_UI PAGE SET`
3. Envoyer `MP3_QUEUE_PREVIEW 5` et verifier la cohérence avec `queue_off`.
4. En mode MP3, verifier les touches:
   - `K6` change de page
   - `K2/K3` naviguent en BROWSE/QUEUE/SET
   - `K1` applique l'action settings en page `SET` (`REPEAT`, `BACKEND`, `SCAN`)

## 8) Diagnostics runtime/screen (serie)

1. Envoyer `SYS_LOOP_BUDGET STATUS`:
   - verifier `max/avg/samples/warn`
2. Envoyer `SCREEN_LINK_STATUS`:
   - verifier `seq`, `tx_ok`, `tx_drop`, `resync`
3. Envoyer `SCREEN_LINK_RESET_STATS` puis `SCREEN_LINK_STATUS`:
   - verifier remise a zero des compteurs
4. Envoyer `SYS_LOOP_BUDGET RESET` puis `SYS_LOOP_BUDGET STATUS`:
   - verifier remise a zero des compteurs loop budget

## 9) Release candidate (S6)

1. Lancer la matrice RC:
   - `make qa-story-v2-rc ESP32_PORT=<PORT_ESP32> SOAK_MINUTES=20 POLL_SECONDS=15 TRACE_LEVEL=INFO`
2. Verifier le rapport:
   - fichier `reports/story_v2_rc_matrix_*.log`
3. Executer ensuite les resets croises manuels:
   - reset ESP8266 seul
   - reset ESP32 seul
   - coupure/reprise liaison UART ecran
4. Consigner la decision flag default ON/OFF dans `RELEASE_STORY_V2.md`.
