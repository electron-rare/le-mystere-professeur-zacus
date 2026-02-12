# Firmware ESP32 (MIT)

## Version PlatformIO (ESP32 DevKit)

Ce dossier contient maintenant un projet **PlatformIO** prêt à compiler, sans code préalable requis.

- Configuration : `platformio.ini`
- Code : `src/main.cpp`

## Fonctionnalités

- LED RGB en clignotement aléatoire.
- Si le micro capte un **LA (~440 Hz)**, la LED passe en **vert**.
- Génération d'une sinusoïde **~440 Hz** sur le DAC interne (`GPIO25`).
- Lecture d'un MP3 (`/track001.mp3`) depuis la carte SD (boucle automatique).

## GPIO utilisés (profil ESP32 DevKit)

- LED RGB externe :
  - `GPIO16` (R)
  - `GPIO17` (G)
  - `GPIO4` (B)
- Micro analogique : `GPIO34`
- DAC sinus : `GPIO25`
- SD (VSPI standard ESP32) :
  - `CS GPIO5`
  - `SCK GPIO18`
  - `MISO GPIO19`
  - `MOSI GPIO23`
- I2S MP3 (DAC/ampli I2S externe) :
  - `BCLK GPIO26`
  - `LRC GPIO27`
  - `DOUT GPIO22`

## Dépendance

- `earlephilhower/ESP8266Audio`

## Utilisation rapide

1. Mettre `track001.mp3` à la racine de la carte SD.
2. Ouvrir ce dossier avec PlatformIO.
3. Compiler et flasher :
   - `pio run`
   - `pio run -t upload`
4. Ouvrir le moniteur série :
   - `pio device monitor`

## Réglages

- Le seuil de détection du LA se règle via `DETECT_RATIO_THRESHOLD` dans `src/main.cpp`.
- Si votre câblage diffère, modifier les constantes GPIO en tête de `src/main.cpp`.
