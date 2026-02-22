# WebUI Dual-Mode E2E Runbook

## Scope

This runbook covers frontend validation for:

- Story V2 API mode (`/api/story/*`, WebSocket stream).
- Freenove legacy API mode (`/api/status`, `/api/scenario/*`, SSE stream).

## Prerequisites

- Node.js + npm installed.
- Dependencies installed: `npm install`
- Device reachable on LAN (default target: `192.168.0.91`).

## Environment

Recommended runtime values:

```bash
export VITE_API_BASE=http://192.168.0.91
export VITE_API_PROBE_PORTS=80,8080
export VITE_API_FLAVOR=auto
```

## Dev and Preview (ESP preset)

Quick commands with the target already set to `192.168.0.91`:

```bash
npm run dev:esp
```

- Dev URL (local): `http://localhost:5173`
- Dev URL (LAN): `http://<your-computer-ip>:5173`

Preview from existing build:

```bash
npm run preview:esp
```

Build + preview in one command:

```bash
npm run preview:esp:build
```

- Preview URL (local): `http://localhost:4173`
- Preview URL (LAN): `http://<your-computer-ip>:4173`

## Test Commands

Mock-only suite:

```bash
npx playwright test
```

Live device suite:

```bash
npx playwright test --grep @live
```

Full gate sequence:

```bash
npm run lint
npm run build
npx playwright test
npx playwright test --grep @live
```

## Live Test Behavior

`@live` tests perform:

- `GET /api/status`
- `GET /api/stream`
- `POST /api/scenario/unlock`
- `POST /api/scenario/next`
- `POST /api/network/wifi/reconnect`
- `POST /api/network/espnow/off`
- `POST /api/network/espnow/on`

A test finalizer always calls `POST /api/network/espnow/on` to return to a safe state.

## Risks (Full Control Mode)

- Temporary control-plane instability while WiFi reconnect is triggered.
- Short telemetry gaps during ESP-NOW off/on toggles.
- If the device is under active gameplay, unlock/next may advance story state.

Use `@live` only on a non-critical session or dedicated test window.
