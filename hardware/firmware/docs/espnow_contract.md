# ESP-NOW Contract v1

Date: 2026-02-23

Ce document est la source canonique pour `docs/espnow_api_v1.md`.

## Source de vérité

- Canonique: `docs/espnow_contract.md`
- Le miroir `docs/espnow_api_v1.md` doit rester cohérent avec ce contrat.

## Reprise du contrat (v1)

- Trame recommandée: `type: "command"`, `ack: true`.
- Corrélation: `msg_id` + `seq`.
- Corps: objet JSON dans `payload`, ou texte compatible selon parser legacy.

Exemple de demande:

```json
{
  "msg_id": "req-001",
  "seq": 7,
  "type": "command",
  "ack": true,
  "payload": {
    "cmd": "WIFI_STATUS"
  }
}
```

Réponse attendue:

```json
{
  "msg_id": "req-001",
  "seq": 7,
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

## Compatibilité parser (legacy)

- `cmd`, `command`, `action` (root ou `payload`).
- format texte: `"CMD arg"`.
- ancien champ `id` au lieu de `msg_id` côté trames historiques.

## Commandes supportées

- `STATUS`
- `WIFI_STATUS`
- `ESPNOW_STATUS`
- `UNLOCK`
- `NEXT`
- `WIFI_DISCONNECT`
- `WIFI_RECONNECT`
- `ESPNOW_ON`
- `ESPNOW_OFF`
- `ESPNOW_DISCOVERY` (broadcast + discovery des pairs visibles)
- `ESPNOW_DISCOVERY_RUNTIME [on|off]` (probe périodique)
- `ESPNOW_DEVICE_NAME_GET`
- `ESPNOW_DEVICE_NAME_SET <NAME>`
- `STORY_REFRESH_SD`
- `SC_EVENT`
- `RING`
- `SCENE <id>`
  - `SCENE` retourne une erreur `missing_scene_id` si `id` absent
  - `NEXT` retourne `scene_not_found` si aucune scène n’est active

## Erreurs connues

- `unsupported_command`
- `missing_scene_id`
- `scene_not_found`
- `WIFI_RECONNECT no_credentials`
- erreurs réseau: `peer`, `payload` vide, trame > 240

## Limites runtime

- Trame brute max: `240`
- Peers: `16`
- RX queue: `6`

## Device name

- `device_name` est l'identité logique locale (persistée en NVS).
- Par défaut Freenove: `U_SON`.
- Visible via `ESPNOW_STATUS` et `STATUS` (`espnow.device_name`).

## Broadcast + discovery

- Mode d'envoi ESP-NOW: `broadcast` (cible unitaire ignorée).
- `ESPNOW_DISCOVERY` envoie des probes broadcast puis agrège les pairs vus dans le cache (`ESPNOW_PEER_LIST` / `ESPNOW_STATUS.peers`).
- Runtime périodique activé par défaut (`discovery_runtime=true`, intervalle `15000 ms`), pilotable via `ESPNOW_DISCOVERY_RUNTIME`.

## Script contrôleur (terrain)

- Script prêt à l'emploi: `scripts/espnow_hotline_control.py`
- Exemples:
  - `python3 scripts/espnow_hotline_control.py --port /dev/cu.usbserial-0001 --target broadcast --action ring`
  - `python3 scripts/espnow_hotline_control.py --port /dev/cu.usbserial-0001 --target AA:BB:CC:DD:EE:FF --ensure-peer --action status`
  - `python3 scripts/espnow_hotline_control.py --port /dev/cu.usbserial-0001 --target broadcast --action hotline1`

## Réception firmware

- `type=command` -> `executeEspNowCommandPayload`
- `type` non-`command` ignoré pour dispatch de commande
- `type=ack` ignoré côté dispatch commande
