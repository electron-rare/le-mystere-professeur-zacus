# UART Protocol (Deprecated)

This document is legacy. The RP2040 firmware uses UI Link v2.
See: `protocol/ui_link_v2.md`.

---

Transport: UART, UTF-8, 1 JSON object per line (`\n`).

- Baud recommandé: `115200`
- Longueur max ligne: `512` bytes
- Clés inconnues: ignorées
- Champs manquants: tolérés

## UI -> ESP32 (`t=cmd`)

```json
{"t":"cmd","a":"play_pause"}
{"t":"cmd","a":"next"}
{"t":"cmd","a":"prev"}
{"t":"cmd","a":"vol_delta","v":1}
{"t":"cmd","a":"vol_set","v":42}
{"t":"cmd","a":"source_set","v":"radio"}
{"t":"cmd","a":"seek","v":120}
{"t":"cmd","a":"station_delta","v":1}
{"t":"cmd","a":"request_state"}
```

## ESP32 -> UI

### `state`
```json
{"t":"state","playing":true,"source":"radio","title":"...","artist":"","station":"Station A","pos":12,"dur":0,"vol":42,"rssi":-61,"buffer":87,"error":""}
```

### `tick` (5 Hz recommandé)
```json
{"t":"tick","pos":13,"buffer":86,"vu":0.42}
```

### `hb` (1 Hz recommandé)
```json
{"t":"hb","ms":12345678}
```

### `list` (extension additive utilisée par l’UI)
```json
{"t":"list","source":"sd","offset":12,"total":75,"cursor":14,"items":["Track A","Track B","Track C","Track D"]}
```

## Reconnect policy UI
1. Si aucun `hb` reçu pendant `3s`, l’UI se marque offline.
2. L’UI envoie `request_state` toutes les `1s` tant que le lien n’est pas revenu.
