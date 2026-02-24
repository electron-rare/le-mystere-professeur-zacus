# ESP-NOW Contract — Freenove (Quick)

Cible: intégration avec `RTC_BL_PHONE` (branche `esp32_RTC_ZACUS`) (app/RTC).

## Recommandation trame
- `type=command`
- `msg_id` et `seq` pour corrélation
- `ack=true` pour obtenir un retour
- `payload` en JSON ou texte

Exemple recommandé:
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

## Réponse ACK attendue
```json
{
  "msg_id": "req-001",
  "seq": 7,
  "type": "ack",
  "ack": true,
  "payload": { "ok": true, "code": "WIFI_STATUS", "error": "", "data": {} }
}
```

## Compatibilité entrées
Le parseur accepte aussi: `cmd`, `command`, `action` (root ou `payload`) et `"CMD arg"`.

## Commandes supportées (ESP-NOW)
- `ESPNOW_SEND <text|json>`: émission toujours en broadcast côté Freenove
- `STATUS`
- `WIFI_STATUS`
- `ESPNOW_STATUS`
- `UNLOCK`, `NEXT`
- `WIFI_DISCONNECT`, `WIFI_RECONNECT`
- `ESPNOW_ON`, `ESPNOW_OFF`
- `STORY_REFRESH_SD`
- `SC_EVENT`
- `RING`
- `SCENE <scenario_id>`
  - exemples: `SCENE DEFAULT`
  - JSON: `{"cmd":"SCENE","args":{"id":"DEFAULT"}}`
- `SCENE_GOTO <scene_id>`
  - exemple: `SCENE_GOTO SCENE_WIN_ETAPE`
  - saute directement vers une scene presente dans le scenario courant (extension one-shot Freenove)
- Actions de contrôle (générique): `HW_*`, `AUDIO_*`, `MEDIA_*`, etc.

## Erreurs fréquentes
- `unsupported_command`
- `missing_scene_id`
- `scene_not_found`
- `scene_goto_arg`
- erreurs réseau/connexion habituelles (`payload` vide, trame > 240)

## Limites runtime
- Trame brute max: `240`
- Peers: `16`
- RX queue: `6`

## Réception côté firmware
- `type=command` -> `executeEspNowCommandPayload`
- autre `type` -> ignoré en commande (bridge story seulement si activé)
- les frames `type=ack` reçues sont ignorées côté dispatch
