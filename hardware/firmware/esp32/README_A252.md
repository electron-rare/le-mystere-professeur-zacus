# Memo Hardware - ESP32 Audio Kit V2.2 A252

Ce document sert de memo pour garder une configuration reproductible sur la carte A252.

## Objectif de ce profil

- Utiliser les touches en mode **analogique sur 1 pin ADC**
- Avoir un mode **fallback signal** sans SD (LA + DAC 440)
- Passer automatiquement en mode **lecteur MP3** apres unlock LA si SD presente
- Envoyer l'etat vers un NodeMCU OLED

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
- Enable ampli: `GPIO21`
- UART ecran TX: `GPIO22`
- ADC touches: `GPIO36`
- Mic codec onboard (I2S DIN): `GPIO35`
- Fallback mic analogique externe (optionnel): `GPIO34` (si `kUseI2SMicInput=false`)
- DAC fallback 440 Hz: `GPIO26` (actif seulement hors mode MP3)

## Keymap runtime

- Mode U_LOCK (boot):
  - un appui touche (`K1..K6`) lance la detection LA
  - le LA doit cumuler 3 secondes de detection (continue ou repetee) pour deverrouiller
  - `K6` relance la calibration micro (30 s) pendant detection
  - autres touches ignorees tant que le LA n'est pas detecte
- Module U-SON fonctionnel (apres detection LA):
  - `K1` LA detect on/off
  - `K2/K3` frequence sine -/+
  - `K4` sine on/off
  - `K5` refresh SD (rescan immediat)
  - `K6` calibration micro (30 s)
- Mode MP3 (avec SD):
  - `K1` play/pause
  - `K2` prev track
  - `K3` next track
  - `K4/K5` volume -/+
  - `K6` repeat ALL/ONE

## Notes de validation terrain

- Le moniteur serie affiche:
  - `[MODE] U_LOCK (appuyer touche pour detecter LA)` au boot
  - `[MODE] MODULE U-SON Fonctionnel (LA detecte)` apres unlock
  - `[MODE] LECTEUR U-SON (SD detectee)` avec SD + pistes MP3
  - `[MP3] Playing x/y: ...`
  - `[KEY] Kx raw=...` lors d'un appui touche
- Les touches fonctionnent en mode analogique (valeurs ADC varient selon touche).
- Le signal sine 440 Hz est audible sur les HP quand active.
- Le NodeMCU OLED affiche l'etat actuel (mode, track, volume, etc.).
- En mode MP3, les commandes de lecture et de volume fonctionnent correctement.
- En mode SIGNAL, les commandes de frequence et d'activation du sine fonctionnent correctement.
- Le fallback mic (si utilisé) capte le son et le transmet via I2S au codec
- Le fallback DAC 440 Hz est audible même sans SD, confirmant que le mode SIGNAL fonctionne
- Le passage automatique entre les modes SIGNAL et MP3 se fait sans erreur lors de l'insertion/retrait de la carte SD
- Les erreurs potentielles a surveiller:
  - Problemes de lecture SD (corruption, incompatibilite)
  - Problemes de son (distorsion, absence de son)
  - Problemes de touches (non reconnues, valeurs ADC erratiques)
  - Problemes d'affichage sur OLED (informations incorrectes ou absentes)
- En cas de problème, vérifier les connexions physiques, les configurations de switch/jumper, et les messages d'erreur dans le moniteur série pour diagnostiquer l'origine du problème.

Checklist detaillee disponible dans `TESTING.md`.
