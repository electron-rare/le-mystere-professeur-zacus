# UI Spec (RP2040 TFT 480x320 + XPT2046)

Ce document est la source de vérité de l'interface UI tactile.

Transport UART actif:
- `../../protocol/ui_link_v2.md` (trames `TYPE,k=v*CC\n`, CRC8 poly `0x07`)

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

## Story scenes (Story V2)

Ces IDs sont utilises par le moteur Story V2 pour piloter l'UI. Ils sont mappes sur les pages ci-dessus.

| Scene ID | Page UI | Usage Story |
|---|---|---|
| `SCENE_LOCKED` | `LECTURE` | Etat verrouille / attente |
| `SCENE_BROKEN` | `LECTURE` | Variante boot / proto U-SON |
| `SCENE_SEARCH` | `LISTE` | Recherche / detection |
| `SCENE_LA_DETECTOR` | `LISTE` | Attente LA / detection |
| `SCENE_CAMERA_SCAN` | `LISTE` | Capture visuelle |
| `SCENE_SIGNAL_SPIKE` | `LISTE` | Interference détectée |
| `SCENE_REWARD` | `REGLAGES` | Recompense |
| `SCENE_WIN` | `REGLAGES` | Variante win |
| `SCENE_READY` | `REGLAGES` | Etat pret / fin |
| `SCENE_MEDIA_ARCHIVE` | `REGLAGES` | Fin de mission / recap |

## Gestes et interactions
- Swipe horizontal: `next/prev`.
- Swipe vertical: `vol_delta`.
- Tap:
  - Header: switch de page.
  - Zones basses: actions contextuelles.
- Poll tactile périodique avec anti-rebond.

## Contrat de données UI

### Entrée (ESP32 -> UI)
- `ACK`: confirmation de session après `HELLO`.
- `KEYFRAME`: snapshot complet (resync immédiat, puis périodique).
- `STAT`: télémétrie compacte périodique (delta-friendly).
- `PING`: heartbeat (UI répond `PONG`).

### Sortie (UI -> ESP32)
- `HELLO` au boot/reconnect (`proto=2`, `ui_type`, `ui_id`, `fw`, `caps`).
- `PONG` en réponse au `PING`.
- `BTN` logique (`id=NEXT|PREV|OK|BACK|VOL_UP|VOL_DOWN|MODE`, `action=...`).
- `TOUCH` optionnel (coordonnées brutes, action `down|move|up`).
- `CMD` optionnel (ex: `op=request_keyframe`).

## Performance et rendu
- Rendu partiel par zones (pas de full refresh continu).
- Cible:
  - max 30 FPS quand dirty.
  - env. 5 FPS en idle.
- La boucle UI doit rester non bloquante sur réception UART.

## Modes dégradés
- Pas de trame valide > 1500 ms: état offline.
- En mode offline:
  - affichage explicite du statut lien.
  - réémission périodique `HELLO` (1s) jusqu'au retour du lien.
