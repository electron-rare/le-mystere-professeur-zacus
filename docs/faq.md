---
layout: default
title: FAQ & dépannage
description: Flash PlatformIO, SD, audio (ESP32) — symptômes, causes fréquentes, correctifs.
---

# FAQ & Dépannage (ESP32 / SD / Audio)

Cette FAQ est volontairement **pratique** : symptômes → causes fréquentes → correctifs.

---

## Flash / Upload (PlatformIO)

### « Timed out waiting for packet header » / Upload qui échoue
Causes fréquentes :
- câble USB “charge only” (pas de data),
- port série incorrect,
- board en mode bootloader non activé.

Correctifs :
- essaie un autre câble + un autre port USB,
- vérifie le port :
  ```bash
  pio device list
  ```
- lance un upload puis **maintien BOOT** (si la carte le demande) jusqu’au démarrage de l’upload,
- essaye un effacement complet :
  ```bash
  pio run -t erase
  pio run -t upload
  ```

### Mauvais environnement PlatformIO
Symptôme : ça compile mais pas pour la bonne carte / mauvais pinout.

Correctifs :
- vérifie `platformio.ini` (env, framework, flags),
- compile explicitement :
  ```bash
  pio run -e esp32dev
  ```

---

## Carte SD

### La SD n’est pas détectée
Causes fréquentes :
- formatage incompatible,
- câblage/pins SD différents selon la carte,
- init SD trop tôt (avant montage / alim stable).

Correctifs :
- formate en **FAT32** (recommandé) avec une taille d’unité d’allocation standard,
- teste une SD plus petite (≤ 32 Go) pour éliminer un souci exFAT,
- loggue le statut au boot (montage OK / erreur).

### Fichiers “introuvables” alors qu’ils sont présents
Causes fréquentes :
- chemins / casse (`/Music` vs `/music`),
- séparation `/` vs `\`,
- caractères spéciaux / noms trop longs.

Correctifs :
- impose une convention : `/music/`, `/audio/`, `/print/`…
- évite accents et caractères exotiques dans les noms,
- affiche dans les logs les chemins exacts.

---

## Audio

### Craquements / coupures
Causes fréquentes :
- débit SD insuffisant (ou fragmentation),
- buffer audio trop petit,
- resampling / décodage trop coûteux.

Correctifs :
- copie les fichiers sur une SD correcte (UHS‑I / A1) et évite la saturation,
- augmente les buffers (si possible) et privilégie la lecture séquentielle,
- teste d’abord un WAV PCM simple (44.1 kHz / 16-bit) pour isoler le problème.

### Volume trop faible / saturé
Causes fréquentes :
- gain numérique trop élevé (clip),
- conversion mono/stéréo,
- réglage codec/I2S.

Correctifs :
- démarre avec un gain faible puis augmente,
- vérifie que l’échantillonnage (sample rate) correspond,
- si tu appliques des effets (comp, EQ), désactive-les pour tester.

---

## Affichage / perf (bonus)

### FPS faible / tearing
Correctifs :
- privilégie **double-buffer partiel** + flush DMA,
- évite les redraw complets : utilise des “dirty rect” / mise à jour ciblée,
- limite les alpha/transparences lourdes dans LVGL.

---

## Logs utiles

### Obtenir les logs série
```bash
pio device monitor -b 115200
```

### Lister les devices
```bash
pio device list
```
