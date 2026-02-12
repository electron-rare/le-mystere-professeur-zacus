# Firmware ESP32 (Audio Kit A252)

Ce dossier contient le firmware principal pour **ESP32 Audio Kit V2.2 A252**.

## Profil cible

- Carte: ESP32 Audio Kit V2.2 A252
- SD: `SD_MMC` (slot microSD onboard, mode 1-bit)
- Audio:
  - boot: `MODE U_LOCK` avec pictogramme casse
  - en `U_LOCK`: appui sur une touche pour lancer la detection du LA (440 Hz, micro onboard)
  - affichage OLED pendant detection: bargraphe volume + bargraphe accordage + scope micro (optionnel si `kUseI2SMicInput=true`)
  - apres detection du LA: pictogramme de validation, puis passage en `MODULE U-SON Fonctionnel`
  - ensuite: activation detection SD, puis passage auto en `MODE LECTEUR U-SON` si SD + MP3
- Touches: clavier analogique sur une seule entree ADC
- Ecran distant: ESP8266 NodeMCU OLED via UART

## Fichiers principaux

- Orchestration: `src/main.cpp`
- Config hardware/audio: `src/config.h`
- MP3 SD_MMC + I2S: `src/mp3_player.h`, `src/mp3_player.cpp`
- Touches analogiques: `src/keypad_analog.h`, `src/keypad_analog.cpp`
- Lien ecran ESP8266: `src/screen_link.h`, `src/screen_link.cpp`
- Memo board A252: `README_A252.md`
- Cablage: `WIRING.md`
- Validation terrain: `TESTING.md`
- Commandes de travail: `Makefile`

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

### Mode U_LOCK (au boot, detection SD bloquee)

- Ecran initial: pictogramme casse + attente d'un appui touche
- Apres appui touche: detection LA active + affichage accordage/volume/scope
- Les touches SIGNAL restent bloquees tant que le LA n'est pas detecte
- La detection SD/MP3 reste desactivee tant que `MODULE U-SON Fonctionnel` n'est pas atteint

### Module U-SON fonctionnel (apres detection du LA)

- `K1` : LA detect on/off
- `K2` : frequence sinus -
- `K3` : frequence sinus +
- `K4` : sinus on/off
- `K5` : demande refresh SD
- `K6` : lance une calibration micro serie (30 s)

### Mode MP3 (SD detectee)

- `K1` : play/pause
- `K2` : piste precedente
- `K3` : piste suivante
- `K4` : volume -
- `K5` : volume +
- `K6` : repeat `ALL/ONE`

Le firmware bascule automatiquement selon la SD:
- SD presente + pistes MP3: `MODE LECTEUR U-SON`
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
   - `make upload-screen SCREEN_PORT=/dev/ttyUSB1`
4. Monitor:
   - `make monitor-esp32 ESP32_PORT=/dev/ttyUSB0`
   - `make monitor-screen SCREEN_PORT=/dev/ttyUSB1`

### Option 2: via PlatformIO direct

1. ESP32 principal:
   - `pio run -e esp32dev`
   - `pio run -e esp32dev -t upload --upload-port /dev/ttyUSB0`
   - `pio device monitor -e esp32dev --port /dev/ttyUSB0`
2. ESP8266 OLED:
   - `pio run -e esp8266_oled`
   - `pio run -e esp8266_oled -t upload --upload-port /dev/ttyUSB1`
   - `pio device monitor -e esp8266_oled --port /dev/ttyUSB1`

Sans variable `PORT`, PlatformIO choisit automatiquement le port serie.

## Lecteur MP3 evolue

Le lecteur MP3:

- detecte/monte la SD automatiquement
- rescane les pistes `.mp3` en racine
- force un rescan immediat sur `K5` (mode SIGNAL)
- gere une playlist triee
- enchaine automatiquement les pistes
- supporte repeat `ALL/ONE`
- expose piste courante + volume vers l'ecran ESP8266

## Calibration touches analogiques

Les seuils sont dans `src/config.h`:

- `kKey1Max` .. `kKey6Max`
- `kKeysReleaseThreshold`

Procedure:

1. Ouvrir le moniteur serie.
2. Appuyer chaque touche.
3. Relever les logs `[KEY] Kx raw=...`.
4. Ajuster les seuils pour separer les 6 zones.
