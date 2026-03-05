# Memo Hardware - ESP32 Audio Kit V2.2 A252

Ce document sert de memo pour garder une configuration reproductible sur la carte A252.

## Objectif de ce profil

- Utiliser les touches en mode **analogique sur 1 pin ADC**
- Avoir un mode **fallback signal** sans SD (LA + debug audio)
- Passer automatiquement en mode **lecteur audio** apres unlock LA si SD presente
- Envoyer l'etat vers un NodeMCU OLED
- Stocker les sons internes dans **LittleFS** (sans OTA)

## Partition / filesystem

- `board_build.partitions = no_ota.csv`
- `board_build.filesystem = littlefs`
- Fichiers audio longs: SD (`SD_MMC`)
- SFX internes (boot, alertes): `LittleFS` (`data/`, ex: `/boot.mp3` ou `/boot.wav`)

## Configuration recommandee des switch/jumpers (S1)

Pour garder `KEY2` et la logique clavier:

- `S1-1 = ON` (IO13 -> KEY2)
- `S1-2 = OFF` (desactive IO13 -> SD_DATA3)
- `S1-3 = ON` (IO15 -> SD_CMD)
- `S1-4 = OFF`
- `S1-5 = OFF`

## Mode touches analogiques

Selon revision PCB, le mode analogique peut demander le reroutage de straps:

- deplacer `R66..R70` vers `R60..R64`
- ajouter `1.8k` sur `R55..R59`

Si deja preconfigure sur ta carte, ne rien modifier.

## Pins reserves dans ce firmware

- I2S codec: `GPIO27`, `GPIO25`, `GPIO26`
- Enable ampli: `GPIO21` (defaut `ACTIVE_HIGH`, inversable via `BOOT_PA_INV`)
- UART ecran TX: `GPIO22`
- ADC touches: `GPIO36`
- Mic codec onboard (I2S DIN): `GPIO35`
- Fallback mic analogique externe (optionnel): `GPIO34` (si `kUseI2SMicInput=false`)
- DAC analogique: desactive (`I2S-only` sur ce profil)
- DAC hardware ESP32 reel (non utilise ici): `GPIO25` / `GPIO26`

## Keymap runtime

- Mode U_LOCK (boot):
  - un appui touche (`K1..K6`) lance la detection LA
  - le LA doit cumuler 3 secondes de detection (continue ou repetee) pour deverrouiller
  - `K6` relance la calibration micro (30 s) pendant detection
  - autres touches ignorees tant que le LA n'est pas detecte
- Module U-SON fonctionnel (apres detection LA):
  - `K1` LA detect on/off
  - `K2` tone test I2S 440 Hz
  - `K3` sequence diag I2S 220/440/880 Hz
  - `K4` replay FX boot I2S
  - `K5` refresh SD (rescan immediat)
  - `K6` calibration micro (30 s)
- Mode lecteur (avec SD):
  - `K1` play/pause
  - `K2` prev track
  - `K3` next track
  - `K4/K5` volume -/+
  - `K6` repeat ALL/ONE

## Notes de validation terrain

- Le moniteur serie affiche:
  - `[MODE] U_LOCK (appuyer touche pour detecter LA)` au boot
  - `[MODE] MODULE U-SON Fonctionnel (LA detecte)` apres unlock
  - `[MODE] LECTEUR U-SON (SD detectee)` avec SD + pistes audio supportees
  - `[MP3] Playing x/y: ...`
  - `[KEY] Kx raw=...` lors d'un appui touche
- Les touches fonctionnent en mode analogique (valeurs ADC varient selon touche).
- Le debug audio hors MP3 passe uniquement par I2S (pas de DAC analogique).
- Extensions lues sur SD: `.mp3`, `.wav`, `.aac`, `.flac`, `.opus`, `.ogg`.
- Debug FS serie:
  - `FS_INFO` (capacite/etat LittleFS)
  - `FS_LIST` (liste fichiers)
  - `FSTEST` (lecture FX boot detecte depuis LittleFS)
- Debug codec I2C serie:
  - `CODEC_STATUS`, `CODEC_DUMP`
  - `CODEC_RD reg`, `CODEC_WR reg val`
  - `CODEC_VOL pct`, `CODEC_VOL_RAW raw`
- Le NodeMCU OLED affiche l'etat actuel (mode, track, volume, etc.).
- En mode lecteur, les commandes de lecture et de volume fonctionnent correctement.
- En mode SIGNAL, les commandes de debug audio I2S (K2/K3/K4) fonctionnent correctement.
- Le fallback mic (si utilisé) capte le son et le transmet via I2S au codec
- Le mode SIGNAL reste operationnel sans SD (detection LA + audio I2S boot/diag)
- Le passage automatique entre les modes SIGNAL et MP3 se fait sans erreur lors de l'insertion/retrait de la carte SD
- Les erreurs potentielles a surveiller:
  - Problemes de lecture SD (corruption, incompatibilite)
  - Problemes de son (distorsion, absence de son)
  - Problemes de touches (non reconnues, valeurs ADC erratiques)
  - Problemes d'affichage sur OLED (informations incorrectes ou absentes)
- En cas de problème, vérifier les connexions physiques, les configurations de switch/jumper, et les messages d'erreur dans le moniteur série pour diagnostiquer l'origine du problème.

Checklist detaillee disponible dans `TESTING.md`.
