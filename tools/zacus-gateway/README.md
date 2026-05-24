# Zacus Hub Gateway

Thin FastAPI aggregator for the SwiftUI hub app
(`apps/zacus-hub/`). See `docs/specs/2026-05-24-zacus-hub-app.md`.

## Run

```bash
cd tools/zacus-gateway
python3 -m venv .venv && source .venv/bin/activate
pip install -r requirements.txt
python gen_token.py --env          # generates .env with ZACUS_HUB_TOKEN
uvicorn main:app --host 0.0.0.0 --port 8400 --reload
```

The token is printed to stdout — paste it into the iOS/macOS app
(Settings → Token).

## Env vars (all prefixed `ZACUS_HUB_`)

| Var | Default | Purpose |
|-----|---------|---------|
| `TOKEN` | `change-me-...` | bearer token the app must present |
| `VOICE_BRIDGE_URL` | `http://studio:8200` | upstream voice-bridge |
| `HINTS_URL` | `http://macm1:8311` | upstream hints engine |
| `ESP32_URL` | unset | optional master REST (LAN-only) |
| `REQUEST_TIMEOUT` | `10.0` | seconds, per upstream call |

## Endpoints (sprint 0)

| Path | Auth | Notes |
|------|------|-------|
| `GET /healthz` | no | liveness |
| `GET /v1/auth/ping` | bearer | client sanity check |
| `GET /v1/state` | bearer | aggregated game state (stub) |
| `WS /v1/state/ws?token=…` | query token | live state push |
| `POST /v1/companion/voice/intent` | bearer | proxy → voice-bridge |
| `POST /v1/companion/hint` | bearer | proxy → hints |
| `GET /v1/studio/scenarios` | bearer | list `game/scenarios/*.yaml` |
| `GET /v1/studio/scenario/{name}` | bearer | YAML + parsed |

## Deploy on MacStudio

Wrap in a launchd plist alongside the other voice services
(`tools/macstudio/`). Use the same Tailscale ACL pattern (tailnet
`electron-rare`).
