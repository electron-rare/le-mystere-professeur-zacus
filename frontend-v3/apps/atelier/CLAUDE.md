# Atelier

Scratch-like authoring studio: Blockly editor (left/center) + R3F 3D simulation stage (right) + validation console (bottom). Live-diff: edit a block → 500 ms debounce → stale badge → click Run → simulation reloads via `simStore.loadScenario(yaml)`.

Design doc: `docs/superpowers/specs/2026-05-01-v3-fusion-atelier-design.md`.

## Layout

```
src/
  main.tsx               # ReactDOM root, StrictMode
  App.tsx                # useLiveDiff() + <Layout/>; dev hooks on window
  main.css               # Shell + pane styles
  vite-env.d.ts          # Vite client types
  components/
    Layout.tsx           # PanelGroup tree, ⌘B stage toggle
    EditorPane.tsx       # Lazy(BlocklyWorkspace) + register blocks + validate on change
    StagePane.tsx        # Lazy(RoomScene + mode HUDs) + Run button
    ConsolePane.tsx      # Mode tabs + validation entries list
    BlocklyWorkspace.tsx # forwardRef wrapper around Blockly.inject
    toolbox.ts           # XML toolbox config
    YamlPreview.tsx      # Syntax-highlighted YAML rendering
  blocks/
    {puzzle,npc,flow}-blocks.ts  # Block definitions + YAML generators
    index.ts                     # registerAllBlocks (idempotent)
  scene/                 # R3F 3D scene (RoomScene, RtcPhone, PuzzleStations, NpcBubble)
  puzzles/               # P1Sound, P5Morse, P6Symbols, P7Coffre (3D widgets)
  modes/                 # SandboxMode, DemoMode, TestMode HUDs
  stores/                # editor / runtime / sim / validation (zustand)
  lib/
    yaml-export.ts       # workspace -> YAML
    yaml-import.ts       # YAML -> workspace
    validator.ts         # 6-rule schema check (ScenarioYaml -> ValidationResult)
    useLiveDiff.ts       # debounce 500 ms then setPendingIr
```

## Patterns

- **Lazy chunks** via `React.lazy` + Suspense for both Blockly (~700 kB gzip) and R3F+three (~1 MB gzip). Initial shell stays under 80 kB. Vite `manualChunks` in `vite.config.ts` enforces the split.
- **Stores stay focused**: `editorStore` = blocklyJson string ; `runtimeStore` = pending/current IR + isStale ; `simStore` = mode + engine instance + npcMood ; `validationStore` = entries with `severity` + `source` (schema|compile|runtime|test).
- **Mode is sim concern**, not editor↔stage sync — lives in `simStore`. ConsolePane reads from there.
- **Run button** in `StagePane` is the only commit point: it calls `simStore.loadScenario(pendingIr)` then `runtimeStore.commitPendingIr()`.
- **Tailwind classes are not configured** — use inline styles or `main.css` classes (`atelier-pane`, `atelier-mode-tab`, `atelier-resizer--*`). Tailwind in legacy simulation HUDs was rewritten as inline styles during P4.
- **Dev-only window hooks**: `App.tsx` exposes stores on `window.__atelierStores`, `Layout.tsx` exposes `window.__atelierToggleStage`, `EditorPane.tsx` exposes `window.__atelierBlockly.getWorkspace()`. All gated by `import.meta.env.DEV`. E2E specs drive workflow through them instead of relying on real DnD.

## Build

```bash
pnpm exec vite build --base=/atelier/   # for prod deployment with prefix
pnpm exec vite build                    # for dev / desktop bundle (root path)
```

`pnpm --filter @zacus/atelier build -- --base=...` does NOT pass `--base` correctly through the script chain — call vite directly via `cd apps/atelier && pnpm exec vite ...`.

## Anti-patterns

- Putting `mode` back into `runtimeStore` — it belongs to `simStore`.
- Calling `voiceWsConnect()` or any lwip-touching code at boot without a network-ready guard (matches the ESP32_ZACUS bug fix from this session).
- Hardcoding `/atelier/` as base path — the dev server uses root, only the deployed prod build uses the prefix.
- Adding deps to root `frontend-v3/package.json` — atelier deps live here.
- Bypassing the Run button to mutate `simStore.engine` directly — breaks the live-diff contract.
