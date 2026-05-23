# Dashboard

Live game-master view : connection setup → expert/simple dashboards → real-time puzzle timeline + NPC / voice / hints panels. Sister app to `apps/atelier/`. Same React 19 + Vite 6 + zustand 5 stack, no Blockly / no R3F.

## Backend wiring

The dashboard talks to **three independent backends**, each through one hook + a typed contract in `@zacus/shared`. None of them touch each other ; the dashboard is the aggregator.

| Backend | Transport | Default URL | Hook | Endpoints used |
|---------|-----------|-------------|------|----------------|
| BOX-3 ESP32 master | **WebSocket** | `ws://zacus-box3.local:81` | `gameStore.connect()` | bidirectional event stream + command frames |
| voice-bridge (MacStudio) | **REST polling** | `http://100.116.92.12:8200` | `useVoiceBridge` (2 s) + `useVoiceUsage` (5 s) | `GET /health/ready`, `GET /tts/cache/stats`, `GET /usage/stats`, `POST /usage/reset` (X-Admin-Key) |
| hints engine | **SSE preferred, REST polling fallback** | `http://localhost:8311` | `useHintsEngine` | `GET /hints/sessions`, `GET /hints/events` (SSE), `DELETE /hints/sessions/{id}` |

All URLs are overridable via `VITE_*` env vars (`VITE_VOICE_BRIDGE_URL`, `VITE_HINTS_BASE_URL`, etc.) — the constants live in `packages/shared/src/constants.ts` and are the source of truth.

**Mock for offline dev** : `pnpm mock` boots `mock/ws-server.ts` on `:81` and replays a deterministic BOX-3 event sequence. Use it for any UI work that does not require a real ESP32 — the voice-bridge and hints backends do not have mocks ; point at the live MacStudio or run the Python services locally.

## Layout

```
src/
  main.tsx                  # ReactDOM root
  App.tsx                   # Routes ConnectionSetup ↔ ExpertDashboard / SimpleDashboard
  components/
    ConnectionSetup.tsx     # Host / port form, persists in localStorage
    ExpertDashboard.tsx     # Full grid: Timeline + ControlPanel + all panels
    SimpleDashboard.tsx     # Stripped GM view for non-tech operators
    Timeline.tsx            # Puzzle progression + event log
    ControlPanel.tsx        # Manual triggers (skip, reset, force-hint)
    PuzzleCard.tsx          # Per-puzzle status tile
    NpcPanel.tsx            # Live NPC mood + last utterance
    VoiceActivityPanel.tsx  # Mic state, last STT transcript
    VoiceUsagePanel.tsx     # Cost-audit panel — uses useVoiceUsage hook
    HintsAdaptivePanel.tsx  # Hints engine state + adaptive level
  hooks/
    useVoiceBridge.ts       # REST polling of voice-bridge :8200 health + cache
    useVoiceUsage.ts        # REST polling of voice-bridge /usage/* (cost audit)
    useHintsEngine.ts       # SSE+polling against the hints engine (auto-fallback)
  store/
    gameStore.ts            # Zustand : BOX-3 WebSocket + mirrored game state
  mock/
    ws-server.ts            # `pnpm mock` — fake BOX-3 WS on :81 for offline dev
```

## Patterns

- **One store** (`gameStore`) — owns the BOX-3 WebSocket, mirrors phase / puzzles / NPC / events. Auto-reconnects via `BOX3_WS_RECONNECT_MS`. The store keeps the last 200 events (`events.slice(-200)`). Don't split per-panel ; the dashboard is a coherent view of one game session.
- **Hooks own their endpoints**. Components consume hook output, never `fetch` directly. The three hooks are independent — none import each other.
- **`useHintsEngine` defaults to SSE** (`auto` mode) and demotes to polling after a 2 s connect timeout or 3 consecutive `onerror` callbacks. A 30 s heartbeat watchdog reconnects wedged streams. In `jsdom` / Node tests (no `EventSource`), it transparently falls back to polling.
- **Test files colocate** : `*.test.tsx` next to the component / hook. Vitest is configured in `vite.config.ts`.
- **Mock server is canonical for dev** : run `pnpm mock` instead of pointing at a real ESP32 — replays a fixed sequence of `game_start`, `profile_detected`, `puzzle_solved`, `hint_given`, `npc_spoke` events plus `timer_update` every 2 s.
- **Dashboard runs on `:5174`** (atelier uses `:5173`). Both can run together during integration work.

## Backend gotchas

- **voice-bridge `:8200`** lives on the MacStudio (`100.116.92.12`). Reachable from any machine on the Tailnet ; from outside, tunnel via `electron-server`. `/health/ready` returns `503` during F5 warmup with a coherent body — the hook treats that as "still loading", not an error.
- **`POST /usage/reset`** requires `X-Admin-Key` whenever the bridge env sets `VOICE_BRIDGE_ADMIN_KEY`. Pass it through `VITE_VOICE_BRIDGE_ADMIN_KEY` or `useVoiceUsage({ adminKey })`.
- **Hints engine is `:8311`** (per `HINTS_DEFAULT_BASE_URL`). The docstring at `tools/hints/server.py:48` recommends `--port 8300` — **don't follow it**, that port is taken by `whisper.cpp` on the MacStudio. Run with `--port 8311`. The service is typically launched on the GM laptop alongside the dashboard, not on the MacStudio.

## Anti-patterns

- Reaching into `apps/atelier/` — these apps share `packages/shared` and `packages/ui` only, never each other's internals.
- Fetching from components — go through a hook so mock / real switching stays in one place.
- Storing per-component UI state in `gameStore` — keep it `useState`. The store is for game state, not panel toggles.
- Opening a WebSocket to voice-bridge `:8200` from here — the WS at `/voice/ws` is reserved for the firmware (per `useVoiceBridge` docstring). Use the REST polls.
- Hardcoding hosts / ports — read from `packages/shared/src/constants.ts` (`BOX3_MDNS_HOST`, `VOICE_BRIDGE_DEFAULT_BASE_URL`, `HINTS_DEFAULT_BASE_URL`) or the matching `VITE_*` env.
- Treating SSE payloads as data in `useHintsEngine` — they are *refresh signals*. Authoritative state lives behind `GET /hints/sessions`.
