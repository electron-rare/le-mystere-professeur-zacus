# Firmware ESP32 (Audio Kit A252)

Ce dossier contient le firmware principal pour **ESP32 Audio Kit V2.2 A252**.

## Licence firmware

- Code firmware: `GPL-3.0-or-later`
- Dependances open source: voir `OPEN_SOURCE.md`
- Politique de licence du depot: `../../../LICENSE.md`

## Profil cible

- Carte: ESP32 Audio Kit V2.2 A252
- Flash interne: partition `no_ota.csv` + filesystem `LittleFS`
- SD: `SD_MMC` (slot microSD onboard, mode 1-bit)
- Audio:
  - boot: `MODE U_LOCK` avec pictogramme casse
  - en `U_LOCK`: appui sur une touche pour lancer la detection du LA (440 Hz, micro onboard)
  - affichage OLED pendant detection: bargraphe volume + bargraphe accordage + scope micro (optionnel si `kUseI2SMicInput=true`)
  - apres cumul de 3 secondes de detection LA (continue ou repetee): pictogramme de validation, puis passage en `MODULE U-SON Fonctionnel`
  - ensuite: activation detection SD, puis passage auto en `MODE LECTEUR U-SON` si SD + fichiers audio supportes
- Touches: clavier analogique sur une seule entree ADC
- Ecran distant: ESP8266 NodeMCU OLED via UART

## Fichiers principaux

- Entree minimale Arduino: `src/main.cpp`
- Orchestrateur applicatif: `src/app.h`, `src/app.cpp`
- Ordonnanceur des briques actives/inactives: `src/runtime/app_scheduler.h`, `src/runtime/app_scheduler.cpp`
- Etat runtime partage (objets + etats): `src/runtime/runtime_state.h`, `src/runtime/runtime_state.cpp`
- Compatibilite includes historiques: `src/app_state.h`
- Codec ES8388 (wrapper `arduino-audio-driver`): `src/audio/codec_es8388_driver.h`, `src/audio/codec_es8388_driver.cpp`
- Config hardware/audio: `src/config.h`
- Lecteur audio SD_MMC + I2S (multi-format): `src/mp3_player.h`, `src/mp3_player.cpp`
- Touches analogiques: `src/keypad_analog.h`, `src/keypad_analog.cpp`
- Lien ecran ESP8266: `src/screen_link.h`, `src/screen_link.cpp`
- Memo board A252: `README_A252.md`
- Cablage: `WIRING.md`
- Validation terrain: `TESTING.md`
- Commandes de travail: `Makefile`
- Assets LittleFS (sons internes): `data/`

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

### Protocole validation audio boot

- Au demarrage, un FX audio I2S est joue puis la validation boot s'ouvre.
- Pendant cette phase, les commandes de mode normal sont bloquees jusqu'a validation/saut/timeout.
- Timeout auto: `kBootAudioValidationTimeoutMs` (par defaut 12000 ms).
- Relectures max: `kBootAudioValidationMaxReplays` (par defaut 3).

Touches actives:

- `K1` : valider le rendu audio boot (`OK`)
- `K2` : rejouer le FX boot
- `K3` : declarer `KO` + rejouer le FX boot
- `K4` : jouer un tone test `440 Hz` (debug sortie audio)
- `K5` : jouer une sequence diag `220 -> 440 -> 880 Hz`
- `K6` : ignorer la validation (`SKIP`)

Commandes serie (moniteur ESP32):

- `BOOT_OK`
- `BOOT_REPLAY`
- `BOOT_KO`
- `BOOT_SKIP`
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
- `BOOT_REOPEN` (relance le FX boot + reouvre la fenetre de validation sans reset)
- `CODEC_STATUS` (etat codec I2C + volumes de sortie lus)
- `CODEC_DUMP` ou `CODEC_DUMP 0x00 0x31` (dump registres codec)
- `CODEC_RD 0x2E` / `CODEC_WR 0x2E 0x10` (lecture/ecriture registre brut)
- `CODEC_VOL 70` (volume codec + gain logiciel lecteur a 70%)
- `CODEC_VOL_RAW 0x12` (force volume brut registres 0x2E..0x31)
- Alias courts acceptes: `OK`, `REPLAY`, `KO`, `SKIP`, `STATUS`, `HELP`, `PAINV`, `FS_INFO`, `FS_LIST`, `FSTEST`

Le firmware publie aussi un rappel de statut periodique (`left=... replay=...`) tant que la validation est active.
Hors fenetre boot: les commandes BOOT qui declenchent de l'audio sont autorisees uniquement en `U_LOCK`.
En `U-SON`/`MP3`, seules les commandes de statut restent autorisees (`BOOT_STATUS`, `BOOT_HELP`, `BOOT_PA_STATUS`, `BOOT_FS_INFO`, `BOOT_FS_LIST`).

### Mode U_LOCK (au boot, detection SD bloquee)

- Ecran initial: module casse avec effet glitch (sans texte)
- Apres appui touche: detection LA active + affichage accordage/volume/scope
- Le LA doit cumuler 3 secondes de detection (continue ou repetee) pour deverrouiller
- Les touches SIGNAL restent bloquees tant que le LA n'est pas detecte
- La detection SD/MP3 reste desactivee tant que `MODULE U-SON Fonctionnel` n'est pas atteint

### Module U-SON fonctionnel (apres detection du LA)

- `K1` : LA detect on/off
- `K2` : tone test I2S `440 Hz`
- `K3` : sequence diag I2S `220 -> 440 -> 880 Hz`
- `K4` : replay FX boot I2S
- `K5` : demande refresh SD
- `K6` : lance une calibration micro serie (30 s)
- Note: profil A252 en mode `I2S-only` (DAC analogique desactive).

### Mode lecteur (SD detectee)

- `K1` : play/pause
- `K2` : piste precedente
- `K3` : piste suivante
- `K4` : volume -
- `K5` : volume +
- `K6` : repeat `ALL/ONE`

Le firmware bascule automatiquement selon la SD:
- SD presente + pistes audio supportees: `MODE LECTEUR U-SON`
- SD absente: `MODE U_LOCK`, puis passage automatique en `MODULE U-SON Fonctionnel` apres detection du LA.
- Note: en `U_LOCK`, la SD n'est volontairement pas scannee ni montee.

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
   - `pio run -e esp32dev -t upload --upload-port /dev/ttyUSB0`
   - `pio run -e esp32dev -t uploadfs --upload-port /dev/ttyUSB0`
   - `pio device monitor -e esp32dev --port /dev/ttyUSB0`
2. ESP8266 OLED:
   - `pio run -e esp8266_oled`
   - `pio run -e esp8266_oled -t upload --upload-port /dev/ttyUSB1`
   - `pio device monitor -e esp8266_oled --port /dev/ttyUSB1`

Sans variable `PORT`, PlatformIO choisit automatiquement le port serie.

## Lecteur audio evolue

Le lecteur:

- detecte/monte la SD automatiquement
- rescane les pistes supportees en racine: `.mp3`, `.wav`, `.aac`, `.flac`, `.opus`, `.ogg`
- force un rescan immediat sur `K5` (mode SIGNAL)
- gere une playlist triee
- enchaine automatiquement les pistes
- supporte repeat `ALL/ONE`
- expose piste courante + volume vers l'ecran ESP8266

Sons internes:

- Le boot tente d'abord un FX depuis `LittleFS`:
  - priorite: chemin configure `kBootFxLittleFsPath` (defaut `/boot.mp3`)
  - puis auto-detection: `/boot.mp3`, `/boot.wav`, `/boot.aac`, `/boot.flac`, `/boot.opus`, `/boot.ogg`
  - fallback final: premier fichier audio supporte trouve dans la racine LittleFS
- Si absent/invalide, fallback automatique sur le bruit radio I2S genere.
- Les sons longs restent recommandes sur SD (`SD_MMC`).

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
- `FS_INFO` / `FS_LIST` / `FSTEST` : debug LittleFS et test lecture FX boot
- `CODEC_STATUS` / `CODEC_DUMP` : debug codec I2C ES8388
- `CODEC_RD reg` / `CODEC_WR reg val` : lecture/ecriture registre codec
- `CODEC_VOL pct` / `CODEC_VOL_RAW raw [out2]` : reglage volume sortie codec

Contraintes:

- ordre strict obligatoire: `K1 < K2 < K3 < K4 < K5 < K6 < REL`
