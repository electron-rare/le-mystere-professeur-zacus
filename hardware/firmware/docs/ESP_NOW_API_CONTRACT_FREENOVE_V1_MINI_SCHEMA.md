# ESP-NOW Freenove — Contract v1 (Mini)

Pair cible: `RTC_BL_PHONE` (`https://github.com/electron-rare/RTC_BL_PHONE/tree/esp32_RTC_ZACUS`)

## Envoi (request)
```json
{
  "msg_id": "req-001",
  "seq": 12,
  "type": "command",
  "ack": true,
  "payload": { "cmd": "<CMD>", "args": <json|string|omitted> }
}
```

## Réponse (ACK)
```json
{
  "msg_id": "req-001",
  "seq": 12,
  "type": "ack",
  "ack": true,
  "payload": { "ok": true, "code": "<CMD>", "error": "", "data": {} }
}
```

## Cmds supportées
- `STATUS`, `WIFI_STATUS`, `ESPNOW_STATUS`
- `UNLOCK`, `NEXT`
- `WIFI_DISCONNECT`, `WIFI_RECONNECT`
- `ESPNOW_ON`, `ESPNOW_OFF`
- `STORY_REFRESH_SD`
- `SC_EVENT`
- `RING`
- `SCENE <scenario_id>` (`SCENE DEFAULT` or `{"cmd":"SCENE","args":{"id":"DEFAULT"}}`)
- `SCENE_GOTO <scene_id>` (`SCENE_GOTO SCENE_WIN_ETAPE`, extension one-shot Freenove)
- actions control (`HW_*`, `AUDIO_*`, `MEDIA_*`)
- `ESPNOW_SEND <text|json>` (broadcast forced)

## Erreurs `ack.payload.error`
- `unsupported_command`
- `missing_scene_id`
- `scene_not_found`
- `scene_goto_arg`

## Contraintes
- payload max 240
- peers max 16
- rx queue 6
- ACK reçu côté Freenove si `type="ack"` n’est pas dispatché.
