# ESP-NOW API v1 (enveloppe `msg_id/seq/type/payload/ack`)

Date: 2026-02-21  
Scope: contrat d'échange entre `RTC_BL_PHONE` (A252) et la seconde carte.

## 1. Objectif

Normaliser les trames ESP-NOW pour:
- corréler requête/réponse,
- conserver compatibilité legacy,
- simplifier l'intégration du second repo.

## 2. Requête v1 (nouveau format recommandé)

```json
{
  "msg_id": "req-001",
  "seq": 1,
  "type": "command",
  "ack": true,
  "payload": {
    "cmd": "STATUS",
    "args": {}
  }
}
```

Règles:
- `msg_id` sert à corréler la réponse.
- `seq` est un compteur local de trame (recommandé monotone par source).
- `type=command` déclenche l'exécution côté firmware.
- `ack=true` demande une réponse corrélée.
- `payload.cmd` obligatoire pour une commande dispatcher.
- `payload.args` optionnel; sérialisé puis passé au dispatcher.

## 3. Réponse v1 (ack corrélée)

```json
{
  "msg_id": "req-001",
  "seq": 1,
  "type": "ack",
  "ack": true,
  "payload": {
    "ok": true,
    "code": "STATUS",
    "data": {},
    "error": ""
  }
}
```

Règles:
- `msg_id` et `seq` reprennent la requête.
- `payload.ok=false` => `payload.error` non vide.
- `payload.data` contient le JSON de la commande si disponible.
- fallback possible `payload.data_raw` si la réponse n'est pas JSON.

## 4. Compatibilité legacy

Le firmware continue d'accepter les formats existants:
- `{"cmd":"..."}`
- `{"raw":"..."}`
- `{"command":"..."}`
- `{"action":"..."}`
- variantes imbriquées via `event`, `message`, `payload`
- format historique `rtcbl/1`:
  - `{"proto":"rtcbl/1","id":"...","cmd":"...","args":{}}`

## 5. Commandes recommandées v1

- `STATUS`
- `RING`
- `CALL`
- `HOOK`
- `LIST_FILES`
- `PLAY_FILE`
- `WIFI_STATUS`
- `MQTT_STATUS`
- `ESPNOW_STATUS`

- `BT_STATUS`

## 6. Intégration second repo

Checklist minimum côté seconde carte:
1. Émettre `msg_id` unique + `seq` monotone.
2. Positionner `type=command` et `ack=true` pour obtenir une réponse.
3. Implémenter timeout de réponse (2-5s) et corréler sur `msg_id`.
4. Prévoir fallback legacy (`rtcbl/1` + formats `cmd/raw/command/action`) pour compat.
