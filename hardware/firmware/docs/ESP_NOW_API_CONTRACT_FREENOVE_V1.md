# ESP-NOW API Contract — Freenove (v1)

Last update: 2026-02-23

Scope: contrat de communication entre la carte Freenove `ui_freenove_allinone` et la carte pair RTC (`RTC_BL_PHONE`, branche `esp32_RTC_ZACUS`).
Référence pair: https://github.com/electron-rare/RTC_BL_PHONE/tree/esp32_RTC_ZACUS
Basé sur l’implémentation actuelle (v1 recommandée + compatibilité legacy).

## 1) Conventions

- Transport: ESP-NOW avec payload texte JSON.
- Taille utile de trame: **240 octets max** (`kEspNowFrameCapacity`).
- Peer en format MAC: `AA:BB:CC:DD:EE:FF` ou `AABBCCDDEEFF`.
- Pas de fragmentation automatique.

Limites runtime (`NetworkManager`):
- Max peers: 16 (`kMaxPeerCache`)
- Queue RX: 6 messages (`kRxQueueSize`)
- Capacité payload stocké: 192 (`kPayloadCapacity`)

## 2) Format recommandé

### Requête `type=command`

```json
{
  "msg_id": "req-001",
  "seq": 1,
  "type": "command",
  "ack": true,
  "payload": {
    "cmd": "WIFI_STATUS",
    "args": {}
  }
}
```

Règles côté pair:
- `msg_id` et `seq` servent à corréler la réponse ACK.
- `type` doit être `"command"` pour exécuter une commande.
- `ack=true` demande un retour.
- `payload.cmd` est obligatoire pour exécuter.

### Réponse attendue

```json
{
  "msg_id": "req-001",
  "seq": 1,
  "type": "ack",
  "ack": true,
  "payload": {
    "ok": true,
    "code": "WIFI_STATUS",
    "error": "",
    "data": {}
  }
}
```

- `code` = nom commande normalisé.
- `ok=false` et `error` non vide sur erreur.
- Si `data` n’est pas JSON, `data_raw` peut être utilisé par la carte.

### Compatibilité legacy (acceptée en entrée)

- `{"cmd":"..."}`
- `{"command":"..."}`
- `{"action":"..."}`
- forme imbriquée via `payload`
- historique `{"proto":"rtcbl/1","cmd":"...",...}`

## 3) Comportement côté carte

Dans la boucle principale:
- `type == "command"` -> `executeEspNowCommandPayload`
- autre `type` -> bridge story (si bridge activé) via `normalizeEspNowPayloadToScenarioEvent`
- `type == "ack"`: reçu en queue RX, ignoré côté dispatch

Réponse command (si `ack=true` côté pair):
- envoi automatique d’une trame `type=ack` avec le même `msg_id`/`seq`.
- le callback fallback crée un `msg_id` si absent.

## 4) Commandes supportées

| Commande | Effet |
| --- | --- |
| `STATUS` | snapshot compact runtime (`appendCompactRuntimeStatus`) |
| `WIFI_STATUS` | état WiFi |
| `ESPNOW_STATUS` | état ESP-NOW |
| `UNLOCK` | dispatch story `UNLOCK` |
| `NEXT` | dispatch de NEXT via `notifyScenarioButtonGuarded(key=5)` |
| `WIFI_DISCONNECT` | déconnexion STA |
| `WIFI_RECONNECT` | reconnexion locale |
| `ESPNOW_ON` | active ESP-NOW |
| `ESPNOW_OFF` | désactive ESP-NOW |
| `STORY_REFRESH_SD` | recharge le scénario depuis SD |
| `SC_EVENT` | dispatch event story (`type`/`name` ou `event_type` + args) |
| `RING` | dispatche l'événement story `RING` |
| `SCENE` | charge un scenario par id (`SCENE <scenario_id>` ou `args.id`, ex: `SCENE DEFAULT`) |
| `SCENE_GOTO` | extension locale Freenove: saute vers une scene dans le scenario courant (`SCENE_GOTO SCENE_WIN_ETAPE`) |
| `control actions` | délègue à `dispatchControlAction(...)` (`HW_*`, `AUDIO_*`, `MEDIA_*`, ...) |

Commandes non reconnues:
- `error` = `unsupported_command`.

## 5) Contrôle peers

Entrées supportées:
- `parseMac` accepte `:` `-` espaces, et hex uniquement.
- Max 16 peers côté cache (pas de limite matérielle ESP-NOW ici documentée par carte).
- Les peers broadcast (`ff:ff:ff:ff:ff:ff`) sont supportés.

## 6) API série

Commands exposées dans `HELP`:
- `ESPNOW_ON`
- `ESPNOW_OFF`
- `ESPNOW_STATUS`
- `ESPNOW_STATUS_JSON`
- `ESPNOW_PEER_ADD <mac>`
- `ESPNOW_PEER_DEL <mac>`
- `ESPNOW_PEER_LIST`
- `ESPNOW_SEND <text|json>` (forcé en broadcast)

Réponses standard:
- `ACK <CMD> ...`
- `ERR <CMD>_ARG`

## 7) API HTTP (alias réseau)

- `GET /api/network/espnow`
- `GET /api/network/espnow/peer`
- `POST /api/network/espnow/send`
- `POST /api/espnow/send`
- `POST /api/network/espnow/on`
- `POST /api/network/espnow/off`
- `POST /api/network/espnow/peer`
- `DELETE /api/network/espnow/peer`

Réponse générale: `{"action":"...","ok":true|false}`.

Exemples:
- `POST /api/network/espnow/send`
  - body: `{ "payload": { "type":"command", "seq":12, "ack":true, "payload": {"cmd":"WIFI_STATUS"} } }`
- `POST /api/network/espnow/on` / `off`
- `POST /api/network/espnow/peer` avec `{ "mac":"AA:BB:CC:DD:EE:FF" }`

## 8) Observabilité

- `ESPNOW_STATUS` / `ESPNOW_STATUS_JSON` expose:
  - `ready`, `peer_count`, `tx_ok`, `tx_fail`, `rx_count`, `last_msg_id`, `last_seq`, `last_type`, `last_ack`, `peers`
- Compteur `drop` augmente si la queue RX est saturée.
- `NET_STATUS` (serial) ajoute également `last_payload`, dernier peer et métriques.

## 9) Erreurs / notes de compatibilité

- `ESPNOW_SEND` échoue si:
  - payload vide
  - taille dépassée
  - erreur ESP-NOW interne
- `SCENE_GOTO` sans argument retourne `scene_goto_arg`.
- `SCENE_GOTO` est une extension locale Freenove (non requise cote RTC).
- `target` est ignoré si fourni côté API web/serial; la carte envoie toujours en broadcast.
- En cas de `payload` non enveloppé, la carte auto-encapsule en `type` inféré.
- Le bridge story est conditionné par `espnow_bridge_to_story_event`.
