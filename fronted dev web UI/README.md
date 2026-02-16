# Story V2 WebUI

## Agent Briefing
Frontend WebUI with three core views:
- Scenario Selector: list scenarios, select and start.
- Live Orchestrator: real time stream, controls, audit log.
- Story Designer: YAML editor, validate, deploy, test run.

Phase 3 goals:
- Responsive layout (desktop, tablet, mobile).
- WebSocket auto reconnect with clear connection status.
- Friendly error handling for 400, 404, 409, 507.
- Touch friendly controls and keyboard navigation.

## Status (Current)
- App shell wired for selector, orchestrator, designer views.
- Tailwind theme and responsive layout in place.
- API client and WebSocket hook implemented.
- Templates are placeholders and must be replaced with real YAML.

## How To Run
```bash
npm install
npm run dev
```

Optional env override:
```
VITE_API_BASE=http://<ESP_IP>:8080
```

## Acceptance Checklist (Phase 3)
- Scenario list loads from GET /api/story/list
- Play calls POST /api/story/select/{id} then POST /api/story/start
- Orchestrator receives step updates from WS /api/story/stream
- Pause, resume, skip buttons call POST endpoints
- Audit log keeps last 100 events and auto scrolls
- Designer validates and deploys YAML
- Responsive layouts work at 1920x1080, 768x1024, 375x667
- Loading states visible during API calls
- Error messages are clear and recoverable

## Next Steps
- Replace YAML templates with real scenario files.
- Add E2E test notes for select, run, skip, deploy flow.
- Verify WS stability for 10 minute stream.
