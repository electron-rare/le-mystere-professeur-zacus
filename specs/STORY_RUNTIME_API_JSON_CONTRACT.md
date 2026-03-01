# Story Runtime API JSON Contract (strict)

## Statut
- Etat: draft contractuel
- Date: 2026-03-01
- Source de verite runtime: firmware

## Scope
Ce contrat couvre les payloads utilises par l'integration Web:
1. Story V2 (`/api/story/*`, WebSocket `/api/story/stream`)
2. Legacy Freenove (`/api/status`, `/api/stream`, `/api/scenario/*`, `/api/control`, `/api/network/*`)

Le contrat est strict sur les champs consommes par le frontend.

## 1) Regles globales
- Encodage: JSON UTF-8.
- `Content-Type`: `application/json` (sauf stream).
- En erreur Story V2, le payload suit:
```json
{
  "error": {
    "code": 409,
    "message": "Story already running",
    "details": "already running"
  }
}
```

## 2) Story V2

### 2.1 GET `/api/story/list`
Reponse 200:
```json
{
  "scenarios": [
    {
      "id": "DEFAULT",
      "version": 2,
      "estimated_duration_s": 0
    }
  ]
}
```

Schema (strict):
```json
{
  "$schema": "https://json-schema.org/draft/2020-12/schema",
  "type": "object",
  "additionalProperties": false,
  "required": ["scenarios"],
  "properties": {
    "scenarios": {
      "type": "array",
      "items": {
        "type": "object",
        "additionalProperties": false,
        "required": ["id", "version", "estimated_duration_s"],
        "properties": {
          "id": { "type": "string", "minLength": 1 },
          "version": { "type": "integer", "minimum": 0 },
          "estimated_duration_s": { "type": "integer", "minimum": 0 }
        }
      }
    }
  }
}
```

### 2.2 GET `/api/story/status`
Reponse 200:
```json
{
  "status": "running",
  "scenario_id": "DEFAULT",
  "current_step": "RTC_ESP_ETAPE1",
  "progress_pct": 22,
  "started_at_ms": 123456,
  "selected": "DEFAULT",
  "queue_depth": 0
}
```

Schema (strict):
```json
{
  "$schema": "https://json-schema.org/draft/2020-12/schema",
  "type": "object",
  "additionalProperties": false,
  "required": [
    "status",
    "scenario_id",
    "current_step",
    "progress_pct",
    "started_at_ms",
    "selected",
    "queue_depth"
  ],
  "properties": {
    "status": { "type": "string", "enum": ["idle", "running", "paused"] },
    "scenario_id": { "type": "string" },
    "current_step": { "type": "string" },
    "progress_pct": { "type": "integer", "minimum": 0, "maximum": 100 },
    "started_at_ms": { "type": "integer", "minimum": 0 },
    "selected": { "type": "string" },
    "queue_depth": { "type": "integer", "minimum": 0 }
  }
}
```

### 2.3 POST `/api/story/select/{scenario_id}`
Reponse 200:
```json
{
  "selected": "DEFAULT",
  "status": "ready"
}
```

### 2.4 POST `/api/story/start`
Reponse 200:
```json
{
  "status": "running",
  "current_step": "RTC_ESP_ETAPE1",
  "started_at_ms": 123456
}
```

### 2.5 POST `/api/story/pause`
Reponse 200:
```json
{
  "status": "paused",
  "paused_at_step": "RTC_ESP_ETAPE1"
}
```

### 2.6 POST `/api/story/resume`
Reponse 200:
```json
{
  "status": "running"
}
```

### 2.7 POST `/api/story/skip`
Reponse 200:
```json
{
  "previous_step": "RTC_ESP_ETAPE1",
  "current_step": "WIN_ETAPE1"
}
```

### 2.8 POST `/api/story/validate`
Request:
```json
{
  "yaml": "id: TEST\\nversion: 2\\n..."
}
```
Reponse 200:
```json
{
  "valid": true
}
```

### 2.9 POST `/api/story/deploy`
Request: stream binaire tar.gz (upload chunked)

Reponse 200:
```json
{
  "deployed": "UPLOAD",
  "status": "ok"
}
```

### 2.10 WS `/api/story/stream`
Message envelope:
```json
{
  "type": "step_change",
  "timestamp": 123456,
  "data": {}
}
```

Types observes:
1. `status`
```json
{
  "type": "status",
  "timestamp": 123456,
  "data": {
    "status": "running",
    "memory_free": 203456,
    "heap_pct": 64
  }
}
```
2. `step_change`
```json
{
  "type": "step_change",
  "timestamp": 123456,
  "data": {
    "previous_step": "RTC_ESP_ETAPE1",
    "current_step": "WIN_ETAPE1",
    "progress_pct": 30
  }
}
```
3. `transition`
```json
{
  "type": "transition",
  "timestamp": 123456,
  "data": {
    "event": "transition",
    "transition_id": "TR_WIN_ETAPE1_TO_CREDIT"
  }
}
```
4. `audit_log`
```json
{
  "type": "audit_log",
  "timestamp": 123456,
  "data": {
    "event_type": "step_execute",
    "step_id": "WIN_ETAPE1"
  }
}
```

## 3) Legacy Freenove

### 3.1 GET `/api/status`
Reponse 200 (core minimal consomme):
```json
{
  "story": {
    "scenario": "DEFAULT",
    "step": "SCENE_LEFOU_DETECTOR",
    "screen": "SCENE_LEFOU_DETECTOR",
    "audio_pack": "PACK_CONFIRM_WIN_ETAPE2"
  },
  "network": {
    "state": "STA",
    "ip": "192.168.0.91"
  }
}
```

Schema core (strict):
```json
{
  "$schema": "https://json-schema.org/draft/2020-12/schema",
  "type": "object",
  "required": ["story"],
  "properties": {
    "story": {
      "type": "object",
      "additionalProperties": true,
      "required": ["scenario", "step"],
      "properties": {
        "scenario": { "type": "string", "minLength": 1 },
        "step": { "type": "string", "minLength": 1 },
        "screen": { "type": "string" },
        "audio_pack": { "type": "string" }
      }
    },
    "network": {
      "type": "object",
      "additionalProperties": true,
      "properties": {
        "state": { "type": "string" },
        "ip": { "type": "string" }
      }
    }
  },
  "additionalProperties": true
}
```

### 3.2 SSE `/api/stream`
Format:
1. event `status` -> `data: <json /api/status>`
2. event `done` -> `data: 1`

Exemple:
```text
event: status
data: {"story":{"scenario":"DEFAULT","step":"SCENE_LA_DETECTOR"}}

event: done
data: 1
```

### 3.3 POST `/api/scenario/unlock`
Reponse 200/400:
```json
{
  "action": "UNLOCK",
  "ok": true
}
```

### 3.4 POST `/api/scenario/next`
Reponse 200/400:
```json
{
  "action": "NEXT",
  "ok": true
}
```

### 3.5 POST `/api/network/wifi/reconnect`
Reponse 200/400:
```json
{
  "action": "WIFI_RECONNECT",
  "ok": true
}
```

### 3.6 POST `/api/network/espnow/on` et `/api/network/espnow/off`
Reponse 200/400:
```json
{
  "action": "ESPNOW_ON",
  "ok": true
}
```

### 3.7 POST `/api/control`
Request:
```json
{
  "action": "NEXT"
}
```

Actions minimales supportees pour compat frontend:
- `NEXT`
- `UNLOCK`
- `WIFI_RECONNECT`
- `ESPNOW_ON`
- `ESPNOW_OFF`

Reponse 200:
```json
{
  "ok": true,
  "action": "NEXT"
}
```
Reponse 400:
```json
{
  "ok": false,
  "action": "NEXT",
  "error": "unsupported_action"
}
```

## 4) Invariants frontend
1. Le frontend ne doit jamais supposer un prefixe de step (`STEP_` ou `SCENE_`).
2. Le frontend doit tolerer des champs additionnels dans `/api/status`.
3. Le frontend doit normaliser les erreurs sur:
   - HTTP status != 2xx
   - payload JSON `{ "error": ... }`
   - payload JSON `{ "ok": false, ... }`

## 5) Compatibility matrix
- Story V2:
  - control natif: `/api/story/skip`
  - stream: WebSocket `/api/story/stream`
- Legacy:
  - control primaire: `/api/scenario/*` ou fallback `/api/control`
  - stream: SSE `/api/stream`
