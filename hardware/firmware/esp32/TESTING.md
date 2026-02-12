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
3. Verifier l'OLED:
   - pictogramme casse + attente appui touche

## 2) Unlock LA

1. Appuyer sur une touche (`K1..K6`) pour lancer la detection LA.
2. Verifier l'OLED:
   - ecran `MODE U_LOCK` en detection
   - bargraphe volume + bargraphe accordage
3. Produire un LA stable (440 Hz) vers le micro.
4. Verifier les logs ESP32:
   - `[MODE] MODULE U-SON Fonctionnel (LA detecte)`
   - `[SD] Detection SD activee.`
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

## 5) Robustesse lien UART

1. Deconnecter temporairement le fil `GPIO22 -> D6`.
2. Verifier l'OLED:
   - affichage `LINK DOWN` apres timeout (~3 s)
3. Reconnecter le fil.
4. Verifier retour automatique vers un ecran de mode.

## 6) Retrait SD en lecture

1. Demarrer une lecture MP3.
2. Retirer la SD.
3. Verifier les logs ESP32:
   - `[MP3] SD removed/unmounted.`
4. Verifier retour mode signal/U_LOCK selon l'etat LA.
