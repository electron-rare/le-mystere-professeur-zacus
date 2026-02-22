# Story V2 WebUI

Frontend Mission Control pour les devices Zacus.

## Nouveautes principales

- `Scenario Selector` revu: recherche, tri, mode legacy explicite, CTA adaptes aux capabilities.
- `Live Orchestrator` revu: statut runtime lisible, recovery stream, filtre d'evenements, controles reseau legacy.
- `Story Designer` nodal avec React Flow:
  - import YAML -> graphe (Story V2 canonique + legacy simplifie),
  - export graphe -> YAML canonique Story V2,
  - edition guidee des `app_bindings`,
  - edition node/edge, undo/redo, auto-layout.
- UI globale harmonisee en style glass modern, labels FR et focus accessibilite.

## Modes API (dual mode)

Détection automatique du firmware connecté :

- `story_v2`: endpoints `/api/story/*` + stream WebSocket.
- `freenove_legacy`: endpoints `/api/status`, `/api/scenario/*`, `/api/stream` (SSE).

Les actions non supportees en mode legacy sont desactivees avec message explicite.

## Diagnostics firmware

À la connexion, le front analyse aussi des endpoints firmware non intrusifs :

- version firmware (`/api/version`, `/api/firmware`, `/api/system/info`, `/api/status`),
- endpoints OTA (`/api/update`, `/api/ota*`, `/api/upgrade*`),
- endpoints reboot (`/api/reboot`, `/api/reset`, `/api/system/reboot`, `/api/restart`).

L'état est affiché dans le panneau "Firmware". En cas d'absence, le front affiche une alerte claire et les actions de mise à jour ne sont pas proposées.

## Variables d'environnement

- `VITE_API_BASE` cible prioritaire (ex: `http://192.168.0.91`)
- `VITE_API_PROBE_PORTS` ordre de probe (defaut: `80,8080`)
- `VITE_API_FLAVOR` override (`auto|story_v2|freenove_legacy`, defaut `auto`)

## Run

```bash
npm install
npm run dev
```

Preset ESP cible `192.168.0.91`:

```bash
npm run dev:esp
```

- Dev local: `http://localhost:5173`
- Dev LAN: `http://<ip-machine>:5173`

Preview sur build existant:

```bash
npm run preview:esp
```

- Preview local: `http://localhost:4173`
- Preview LAN: `http://<ip-machine>:4173`

Build + preview:

```bash
npm run preview:esp:build
```

## Tests / gates frontend

```bash
npm run lint
npm run build
npm run test:unit
npx playwright test
npx playwright test --grep @live
```

- `test:unit`: validation parser/generation YAML Story Designer.
- `playwright @mock`: detection Story V2/legacy, UX et regressions principales.
- `playwright @live`: tests live sur `192.168.0.91` (actions mutantes autorisees).

## Notes live

- La suite `@live` execute des actions de controle (`unlock`, `next`, `wifi reconnect`, `espnow off/on`).
- Un finalizer force `espnow on` en fin de run.
- A lancer sur une fenetre de test dediee (pas pendant une partie active).
