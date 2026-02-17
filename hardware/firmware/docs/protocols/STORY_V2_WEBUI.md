# STORY V2 WebUI API

Base URL: http://<ESP_IP>:8080

## REST Endpoints


### GET /api/story/list
Liste tous les scénarios disponibles.

Réponse :
```json
{
	"scenarios": [
		{"id": "DEFAULT", "version": 1, "estimated_duration_s": 120},
		{"id": "EXPRESS", "version": 1, "estimated_duration_s": 90}
	]
}
```
Statuts : 200 OK | 500 Internal Error


### POST /api/story/select/{scenario_id}
Sélectionne un scénario pour le prochain démarrage.

Réponse :
```json
{"selected": "DEFAULT", "status": "ready"}
```
Statuts : 200 OK | 404 Not Found | 400 Bad Request


### POST /api/story/start
Démarre le scénario sélectionné.

Réponse :
```json
{"status": "running", "current_step": "STEP_ID", "started_at_ms": 123456}
```
Statuts : 200 OK | 409 Conflict (déjà en cours) | 412 Precondition Failed (non sélectionné)


### GET /api/story/status
Retourne l’état courant du scénario.

Réponse :
```json
{
	"status": "running|paused|idle",
	"scenario_id": "DEFAULT",
	"current_step": "STEP_ID",
	"progress_pct": 10,
	"started_at_ms": 123456,
	"selected": "DEFAULT",
	"queue_depth": 0
}
```
Statuts : 200 OK


### POST /api/story/pause
Met en pause l’exécution.

Réponse :
```json
{"status": "paused", "paused_at_step": "STEP_ID"}
```
Statuts : 200 OK | 409 Conflict (non en cours)


### POST /api/story/resume
Reprend l’exécution après pause.

Réponse :
```json
{"status": "running"}
```
Statuts : 200 OK | 409 Conflict (non en pause)


### POST /api/story/skip
Passe à l’étape suivante.

Réponse :
```json
{"previous_step": "STEP_A", "current_step": "STEP_B"}
```
Statuts : 200 OK | 409 Conflict (non en cours)


### POST /api/story/validate
Valide un YAML de scénario (JSON wrapper).

Requête :
```json
{"yaml": "---\nversion: 1\n..."}
```
Réponse :
```json
{"valid": true}
```
Statuts : 200 OK | 400 Bad Request


### POST /api/story/deploy
Déploie une archive de scénario sur /story (tar.gz).

Réponse :
```json
{"deployed": "UPLOAD", "status": "ok"}
```
Statuts : 200 OK | 400 Bad Request | 507 Insufficient Storage


### GET /api/audit/log
Retourne les derniers événements d’audit.

Paramètre : `?limit=50` (max 500)

Réponse :
```json
{"events": [ {"type": "step_change", ...} ]}
```
Statuts : 200 OK


### GET /api/story/fs-info
Infos sur le filesystem /story.

Réponse :
```json
{
	"total_bytes": 1048576,
	"used_bytes": 512000,
	"free_bytes": 536576,
	"scenarios": 4
}
```
Statuts : 200 OK


### POST /api/story/serial-command
Exécute une commande série Story et retourne la réponse.

Requête :
```json
{"command": "STORY_V2_STATUS"}
```
Réponse :
```json
{"command": "...", "response": "...", "latency_ms": 12}
```
Statuts : 200 OK | 400 Bad Request | 500 Internal Error


## WebSocket

Endpoint : ws://<ESP_IP>:8080/api/story/stream

Messages JSON :
- `step_change` : changement d’étape
- `transition` : transition d’état
- `audit_log` : événement d’audit
- `status` : ping/état
- `error` : erreur

Exemple :
```json
{
	"type": "step_change",
	"timestamp": 1234567,
	"data": {
		"previous_step": "unlock_event",
		"current_step": "action_1",
		"progress_pct": 25
	}
}
```


## CORS

En-têtes :
- Access-Control-Allow-Origin: *
- Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS
- Access-Control-Allow-Headers: Content-Type, Authorization
- Access-Control-Max-Age: 3600

## Format d’erreur (tous endpoints)

En cas d’erreur, la réponse est :
```json
{
	"error": {
		"code": 400,
		"message": "Invalid scenario ID",
		"details": "Scenario 'UNKNOWN' not found in /story/"
	}
}
```
Le code HTTP correspond à l’erreur (400, 404, 409, 412, 507, 500).
