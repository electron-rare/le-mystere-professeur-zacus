# Checklist de validation

Cette checklist couvre le comportement attendu du couple ESP32 + ESP8266 OLED.

## Prerequis

- Firmware ESP32 flashe.
- Firmware ESP8266 OLED flashe.
- Liaison `GPIO22 -> D6` et `GND <-> GND` cablee.
- Moniteur serie disponible sur les deux cartes.

## 1) Boot sans SD

1. Demarrer l'ESP32 sans carte SD.
2. Verifier les logs ESP32:
   - `[MODE] U_LOCK (appuyer touche pour detecter LA)`
   - pas de montage SD immediat
   - `[BOOT_PROTO] START ...`
   - `[KEYMAP][BOOT_PROTO] K1=OK, K2=REPLAY, K3=KO+REPLAY, K4=TONE, K5=DIAG, K6=SKIP`
3. Verifier l'OLED:
   - pictogramme casse + attente appui touche

## 1b) Validation audio boot (touches + serial)

1. Pendant la fenetre de validation boot:
   - appuyer `K2`: verifier `REPLAY #...` dans les logs et relecture FX.
   - appuyer `K3`: verifier `KO recu ...` + relecture FX.
   - appuyer `K4`: verifier tone test 440 Hz + logs `[AUDIO_DBG]`.
   - appuyer `K5`: verifier sequence 220/440/880 Hz + logs `[AUDIO_DBG]`.
2. Validation touches:
   - appuyer `K1`: verifier `[BOOT_PROTO] DONE status=VALIDATED ...`.
3. Validation serial:
   - envoyer `BOOT_STATUS` puis verifier `left=... replay=...`.
   - envoyer `BOOT_REPLAY` pour rejouer.
   - envoyer `BOOT_TEST_TONE` puis `BOOT_TEST_DIAG` pour test audio.
   - envoyer `BOOT_PA_STATUS` (et si besoin `BOOT_PA_ON`).
   - envoyer `BOOT_OK` pour valider.
   - optionnel: verifier aussi les alias `STATUS`, `REPLAY`, `OK`.
4. Timeout:
   - ne rien faire pendant ~12 s et verifier `TIMEOUT -> SKIP auto`.
5. Limite replay:
   - declencher plus de 3 replays et verifier `REPLAY refuse: max atteint.`

## 2) Unlock LA

1. Appuyer sur une touche (`K1..K6`) pour lancer la detection LA.
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

## 3) SD et MP3

1. Inserer une SD avec au moins un fichier `.mp3` a la racine.
2. Verifier les logs ESP32:
   - `[MP3] SD_MMC mounted.`
   - `[MP3] x track(s) loaded.`
   - `[MODE] LECTEUR U-SON (SD detectee)`
3. Verifier l'OLED:
   - ecran `LECTEUR U-SON`
   - piste + volume visibles

## 4) Touches

1. En `U_LOCK`:
   - verifier que `K1..K5` sont ignorees
   - verifier que `K6` lance la calibration micro
2. En `U-SON FONCTIONNEL`:
   - `K1`: toggle detection LA
   - `K2/K3`: frequence sine -/+
   - `K4`: sine on/off
   - `K5`: refresh SD (rescan immediat)
   - `K6`: calibration micro
3. En mode MP3:
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

1. Demarrer une lecture MP3.
2. Retirer la SD.
3. Verifier les logs ESP32:
   - `[MP3] SD removed/unmounted.`
4. Verifier retour mode signal/U_LOCK selon l'etat LA.
