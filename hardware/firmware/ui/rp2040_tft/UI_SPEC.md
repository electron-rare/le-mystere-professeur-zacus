# UI Spec — 480x320 Touch

## Pages

## 1) `LECTURE`
- Badge source (`SD` / `RADIO`)
- Statut lien UART
- RSSI Wi-Fi
- Titre (2 lignes, marquee si débordement)
- Ligne secondaire (`artist` en SD, `station` en radio)
- Barre de progression:
  - SD: seek par tap
  - Radio: `LIVE`
- Mini VU meter
- Actions bas écran: `PREV`, `PLAY`, `NEXT`, `VOL-`, `VOL+`

## 2) `LISTE`
- Source active (`SD` / `RADIO`)
- 4 lignes visibles avec surlignage
- Offset / total
- Actions bas écran: `UP`, `DOWN`, `OK`, `BACK`, `MODE`

## 3) `REGLAGES`
- Items:
  - Wi-Fi
  - EQ
  - Luminosite
  - Screensaver
- Action `APPLY`
- Actions bas écran: `UP`, `DOWN`, `APPLY`, `BACK`, `MODE`

## Gestures
- Swipe horizontal: `next/prev`
- Swipe vertical: `vol_delta`
- Tap zones:
  - Header: switch page
  - Bottom buttons: actions contextuelles

## Rendering policy
- Partiel (zones), pas de refresh plein écran en continu
- Cadence max:
  - 30 FPS quand dirty
  - 5 FPS en idle
