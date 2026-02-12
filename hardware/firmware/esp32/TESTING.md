# Checklist de validation

Cette checklist couvre le comportement attendu du couple ESP32 + ESP8266 OLED.

## Prerequis

- Firmware ESP32 flashe.
- Firmware ESP8266 OLED flashe.
- Si sons internes utilises: image LittleFS flashee (`make uploadfs-esp32 ...`).
- Liaison `GPIO22 -> D6` et `GND <-> GND` cablee.
- Moniteur serie disponible sur les deux cartes.

## 1) Boot sans SD

1. Demarrer l'ESP32 sans carte SD.
2. Verifier les logs ESP32:
   - `[MODE] U_LOCK (appuyer touche pour detecter LA)`
   - pas de montage SD immediat
   - `[BOOT_PROTO] START ...`
   - intro audio `boot.mp3` puis scan radio I2S
   - `[KEYMAP][BOOT_PROTO] K1..K6=NEXT ...`
3. Verifier l'OLED:
   - pictogramme casse + attente appui touche

## 1b) Validation audio boot (touches + serial)

1. Pendant la fenetre boot (attente touche):
   - verifier que le scan radio I2S tourne en continu
   - appuyer `K1..K6`: verifier `[BOOT_PROTO] DONE status=VALIDATED ...`
2. Validation serial:
   - envoyer `BOOT_STATUS` puis verifier `waiting_key=1 ...`
   - envoyer `BOOT_REPLAY` pour relire intro + relancer scan
   - envoyer `BOOT_TEST_TONE` puis `BOOT_TEST_DIAG` pour test audio
   - envoyer `BOOT_PA_STATUS` (et si besoin `BOOT_PA_ON`)
   - si silence audio: envoyer `BOOT_PA_INV` puis re-tester `BOOT_TEST_TONE`
   - envoyer `BOOT_FS_INFO` puis `BOOT_FS_LIST`
   - optionnel: envoyer `BOOT_FS_TEST` pour lire le FX boot LittleFS
   - envoyer `BOOT_NEXT` (ou alias `OK`) pour passer a l'etape suivante
   - envoyer `BOOT_REOPEN` pour relancer le protocole sans reset carte
3. Limite replay:
   - declencher plus de 6 replays et verifier `REPLAY refuse: max atteint.`

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
   - `FS_INFO`
   - `FS_LIST`
   - `FSTEST` (lecture du FX boot detecte)

## 4) Touches

1. En `U_LOCK`:
   - verifier que `K1..K5` sont ignorees
   - verifier que `K6` lance la calibration micro
2. En `U-SON FONCTIONNEL`:
   - `K1`: toggle detection LA
   - `K2`: tone test I2S 440 Hz
   - `K3`: sequence diag I2S 220/440/880 Hz
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
   - affichage `LINK DOWN` apres timeout (~10 s + anti-flicker ~1.5 s)
3. Reconnecter le fil.
4. Verifier retour automatique vers un ecran de mode.

## 6) Retrait SD en lecture

1. Demarrer une lecture audio.
2. Retirer la SD.
3. Verifier les logs ESP32:
   - `[MP3] SD removed/unmounted.`
4. Verifier retour mode signal/U_LOCK selon l'etat LA.
