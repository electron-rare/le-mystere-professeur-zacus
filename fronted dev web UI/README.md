# Story V2 WebUI

Mission Control frontend for Zacus devices.

## Features

- `Scenario Selector`: browse and open scenarios.
- `Live Orchestrator`: live stream, controls, and event log.
- `Story Designer`: YAML editing, validate, deploy, and test-run.
- `Nodal Story Generator`: generate node/transition YAML drafts (linear, fork/merge, hub).

## API Modes

The app auto-detects the connected firmware flavor:

- `story_v2`: `/api/story/*` + WebSocket stream.
- `freenove_legacy`: `/api/status`, `/api/scenario/*`, `/api/stream` (SSE).

When running in legacy mode, unsupported actions are disabled with explicit UI messaging.

## Environment

- `VITE_API_BASE` default target seed, example: `http://192.168.0.91`
- `VITE_API_PROBE_PORTS` probe order, default: `80,8080`
- `VITE_API_FLAVOR` override: `auto|story_v2|freenove_legacy` (default `auto`)

## Run

```bash
npm install
npm run dev
```

Example (current ESP target):

```bash
VITE_API_BASE=http://192.168.0.91 VITE_API_PROBE_PORTS=80,8080 npm run dev
```

## Gates

```bash
npm run lint
npm run build
npx playwright test
npx playwright test --grep @live
```

Default Playwright execution runs `@mock` scenarios. `@live` tests are intended for the real device and include control mutations.

## Notes

- Live tests target `http://192.168.0.91` by default.
- Keep a stable local network during `@live` runs because tests include WiFi/ESP-NOW control actions.
