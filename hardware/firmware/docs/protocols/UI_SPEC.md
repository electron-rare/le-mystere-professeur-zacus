# UI Spec (RP2040 TFT 480x320 + XPT2046)

Ce document est la source de vérité de l'interface UI tactile.

## Pages

### 1. `LECTURE`
- Badge source (`SD` / `RADIO`).
- Statut lien UART et heartbeat.
- RSSI, titre (2 lignes), ligne secondaire (`artist` ou `station`).
- Barre de progression:
  - SD: seek par tap.
  - Radio: état live.
- Mini VU meter.
- Actions bas écran: `PREV`, `PLAY`, `NEXT`, `VOL-`, `VOL+`.

### 2. `LISTE`
- Source active (`SD` / `RADIO`).
- 4 lignes visibles avec surlignage.
- Offset / total.
- Actions: `UP`, `DOWN`, `OK`, `BACK`, `MODE`.

### 3. `REGLAGES`
- Items actuels: `Wi-Fi`, `EQ`, `Luminosite`, `Screensaver`.
- Action `APPLY`.
- Actions: `UP`, `DOWN`, `APPLY`, `BACK`, `MODE`.

## Gestes et interactions
- Swipe horizontal: `next/prev`.
- Swipe vertical: `vol_delta`.
- Tap:
  - Header: switch de page.
  - Zones basses: actions contextuelles.
- Poll tactile périodique avec anti-rebond.

## Contrat de données UI

### Entrée (ESP32 -> UI)
- `state`: état complet (source, lecture, metadata, volume, RSSI, buffer).
- `tick`: mise à jour rapide (position, buffer, VU).
- `hb`: heartbeat lien.
- `list`: liste de navigation (source, offset, total, cursor, items).

### Sortie (UI -> ESP32)
- `t=cmd`, action `a`, valeur optionnelle `v`.
- Actions canoniques:
  - `play_pause`, `next`, `prev`
  - `vol_delta`, `vol_set`
  - `source_set`, `seek`, `station_delta`
  - `request_state`

## Performance et rendu
- Rendu partiel par zones (pas de full refresh continu).
- Cible:
  - max 30 FPS quand dirty.
  - env. 5 FPS en idle.
- La boucle UI doit rester non bloquante sur réception UART.

## Modes dégradés
- Pas de `hb` > 3s: état offline.
- En mode offline:
  - affichage explicite du statut lien.
  - émission périodique `request_state` (1s) jusqu'au retour du lien.
