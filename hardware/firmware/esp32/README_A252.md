# Memo Hardware - ESP32 Audio Kit V2.2 A252

Ce document sert de memo pour garder une configuration reproductible sur la carte A252.

## Objectif de ce profil

- Utiliser les touches en mode **analogique sur 1 pin ADC**
- Avoir un mode **fallback signal** sans SD (LA + DAC 440)
- Passer automatiquement en mode **lecteur MP3** quand SD presente
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
- Mic analogique externe (optionnel): `GPIO34`
- DAC fallback 440 Hz: `GPIO26` (actif seulement hors mode MP3)

## Keymap runtime

- Mode SIGNAL (sans SD):
  - `K1` LA detect on/off
  - `K2/K3` frequence sine -/+
  - `K4` sine on/off
  - `K5` refresh SD
- Mode MP3 (avec SD):
  - `K1` play/pause
  - `K2` prev track
  - `K3` next track
  - `K4/K5` volume -/+
  - `K6` repeat ALL/ONE

## Notes de validation terrain

- Une ou plusieurs pistes `.mp3` doivent etre presentes a la racine SD.
- Le moniteur serie affiche:
  - `[MODE] SIGNAL (LA + DAC 440)` sans SD
  - `[MODE] MP3 (SD detectee)` avec SD
  - `[MP3] Playing x/y: ...`
  - `[KEY] Kx raw=...` lors d'un appui touche
- L'ecran ESP8266 doit afficher mode + piste + volume + `KEY: Kx`.
