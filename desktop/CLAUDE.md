# Desktop — Zacus Studio

macOS Electron control hub for Professeur Zacus escape room kits. Bundles the V3 frontends, talks to ESP32 over USB serial, and ships notarised universal/arm64 builds.

## Layout

```
src/
  main/              # Electron main process (Node) — IPC, serial, app lifecycle
  preload/           # Context-bridge preload (exposes safe APIs to renderer)
  renderer/          # React renderer (UI)
scripts/
  build-frontends.sh # Bundles frontend-v3 apps into resources/
  notarize.js        # Apple notarization (post-build hook)
resources/           # Bundled frontend artefacts + assets
```

## Commands

```bash
npm install                  # Triggers electron-builder install-app-deps + electron-rebuild
npm run dev                  # Concurrent main + renderer
npm run build                # tsc main + vite renderer
npm run build:mac            # Universal (x64 + arm64) installer
npm run build:mac-arm64      # Apple Silicon only
npm run rebuild-native       # zacus-native addon for current Electron ABI
```

## Patterns

- Three tsconfigs (`tsconfig.main.json`, `tsconfig.renderer.json`, root) — never share between processes; main and renderer have different module systems.
- Serial port access lives in `src/main/` only. Renderer talks via `preload` IPC, never imports `serialport`.
- Native modules (`serialport`, `zacus-native`) need rebuilding for Electron's Node ABI — run `npm run rebuild-native` after Electron upgrades.
- Notarization requires `APPLE_ID`, `APPLE_APP_SPECIFIC_PASSWORD`, `APPLE_TEAM_ID` env vars; never commit them.

## Frontend Bundle

`scripts/build-frontends.sh` builds `frontend-v3/` apps and copies them into `resources/`. Run it before `electron-builder` if frontend changed.

## Anti-Patterns

- Calling `require('serialport')` from renderer (security: nodeIntegration must stay false)
- Hardcoding device paths (`/dev/cu.usbserial-*`) — enumerate at runtime via `serialport.list()`
- Skipping notarization on release builds (Gatekeeper will block)
