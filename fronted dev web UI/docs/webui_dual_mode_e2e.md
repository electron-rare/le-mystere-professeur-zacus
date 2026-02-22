# WebUI Dual-Mode E2E Runbook

## Objectif

Valider le frontend dual mode sur:

- Story V2 (`/api/story/*`, stream WebSocket)
- Legacy Freenove (`/api/status`, `/api/scenario/*`, `/api/stream` SSE)

Cible live par defaut: `192.168.0.91`.

## Prerequis

- Node.js + npm installes
- Dependances installees: `npm install`
- Device joignable sur le LAN

## Variables d'environnement

```bash
export VITE_API_BASE=http://192.168.0.91
export VITE_API_PROBE_PORTS=80,8080
export VITE_API_FLAVOR=auto
```

## Dev / Preview (preset ESP)

```bash
npm run dev:esp
```

- Dev local: `http://localhost:5173`
- Dev LAN: `http://<ip-machine>:5173`

```bash
npm run preview:esp
```

- Preview local: `http://localhost:4173`
- Preview LAN: `http://<ip-machine>:4173`

Build + preview:

```bash
npm run preview:esp:build
```

## Suites de tests

Unitaires parser/generation Story Designer:

```bash
npm run test:unit
```

Playwright mock (defaut):

```bash
npx playwright test
```

Playwright live:

```bash
npx playwright test --grep @live
```

## Gates frontend completes

```bash
npm run lint
npm run build
npm run test:unit
npx playwright test
npx playwright test --grep @live
```

## Analyse firmware (automatique)

Le frontend teste des endpoints non-intrusifs pour diagnostiquer le support firmware :

- version disponible,
- endpoint de mise à jour OTA,
- endpoint de redemarrage.

Sur le firmware branché en live (`192.168.0.91`), on attend :

- version non connue (aucun endpoint firmware standard),
- `Aucun endpoint de mise à jour OTA détecté`,
- `Aucun endpoint de redémarrage détecté`.

Ces limites sont visibles dans le panneau `Firmware` de l’application (et empêchent les actions de mise à jour automatisées).

## Scenarios verifies (mock)

- detection `story_v2`
- detection `freenove_legacy`
- UX selector/orchestrator/designer en FR
- Story Designer:
  - import YAML -> graphe
  - edition binding -> export/import coherent
  - undo/redo
- gestion erreurs API (404/507)

## Scenarios verifies (live @full-control)

Actions executees:

- `GET /api/status`
- `GET /api/stream`
- `POST /api/scenario/unlock`
- `POST /api/scenario/next`
- `POST /api/network/wifi/reconnect`
- `POST /api/network/espnow/off`
- `POST /api/network/espnow/on`

Verification finale: `GET /api/status` + finalizer `espnow on`.

## Risques (mode Full control)

- transitions de story avancees (unlock/next)
- micro-coupures reseau pendant reconnect WiFi
- telemetrie interrompue temporairement pendant off/on ESP-NOW

Lancer `@live` uniquement sur une fenetre de test dediee.
