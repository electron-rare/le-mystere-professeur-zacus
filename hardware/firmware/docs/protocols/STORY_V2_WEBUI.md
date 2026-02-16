# STORY V2 WebUI API

Base URL: http://<ESP_IP>:8080

## REST Endpoints

### GET /api/story/list
Lists available scenarios.

Response:
```
{"scenarios":[{"id":"DEFAULT","version":1,"estimated_duration_s":0}]}
```

### POST /api/story/select/{scenario_id}
Selects a scenario for next start.

Response:
```
{"selected":"DEFAULT","status":"ready"}
```

### POST /api/story/start
Starts the selected scenario.

Response:
```
{"status":"running","current_step":"STEP_ID","started_at_ms":123456}
```

### GET /api/story/status
Returns current story state.

Response:
```
{"status":"running|paused|idle","scenario_id":"DEFAULT","current_step":"STEP_ID","progress_pct":10}
```

### POST /api/story/pause
Pauses execution.

Response:
```
{"status":"paused","paused_at_step":"STEP_ID"}
```

### POST /api/story/resume
Resumes execution.

Response:
```
{"status":"running"}
```

### POST /api/story/skip
Skips to next step.

Response:
```
{"previous_step":"STEP_A","current_step":"STEP_B"}
```

### POST /api/story/validate
Validates YAML (lightweight JSON wrapper).

Request:
```
{"yaml":"---\nversion: 1\n..."}
```

Response:
```
{"valid":true}
```

### POST /api/story/deploy
Uploads a scenario archive to /story (stored as-is).

Response:
```
{"deployed":"UPLOAD","status":"ok"}
```

### GET /api/audit/log
Returns last events.

Query: `?limit=50` (max 500)

Response:
```
{"events":[{"type":"step_change",...}]}
```

### GET /api/story/fs-info
Filesystem info.

Response:
```
{"total_bytes":1048576,"used_bytes":512000,"free_bytes":536576,"scenarios":4}
```

### POST /api/story/serial-command
Bridges story serial commands.

Request:
```
{"command":"STORY_V2_STATUS"}
```

Response:
```
{"command":"...","response":"...","latency_ms":12}
```

## WebSocket

Endpoint: ws://<ESP_IP>:8080/api/story/stream

Messages:
- `step_change`
- `transition`
- `audit_log`
- `status`
- `error`

## CORS

Headers:
- Access-Control-Allow-Origin: *
- Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS
- Access-Control-Allow-Headers: Content-Type, Authorization
- Access-Control-Max-Age: 3600
