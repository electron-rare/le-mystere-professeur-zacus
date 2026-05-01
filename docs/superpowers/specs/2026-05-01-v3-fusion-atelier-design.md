# V3 Fusion — `@zacus/atelier` Design

**Date**: 2026-05-01
**Status**: Approved (brainstorm session)
**Author**: L'électron rare (with Claude facilitation)

---

## Goal

Replace the current dual-frontend setup (`frontend-scratch-v2/` legacy + `frontend-v3/` monorepo with 3 apps) with a single V3 monorepo containing **2 apps**: `dashboard` (unchanged) and `atelier` (new — fusion of `editor` + `simulation`).

`@zacus/atelier` is a Scratch-like authoring environment: Blockly editor on the left, 3D simulation stage on the right, validation console at the bottom. Editing a block surfaces a "stale" badge on the stage; clicking Run replays the simulation against the freshly compiled Runtime 3 IR.

## Non-Goals

- Porting any V2 feature (`MediaManager`, `NetworkPanel`) — these are explicitly retired.
- Knowing about the PLIP retro phone target. PLIP is an annex device piloted by ESP32 that consumes shared YAML/NPC artefacts. Atelier produces those artefacts but has no PLIP runtime concept.
- Renaming the `desktop/` Electron app (stays `zacus-studio`). Atelier lives **inside** Zacus Studio Desktop.
- Replacing the `dashboard` app. Dashboard is out of scope.

## Constraints

- All conversation in user-facing UI is French; code, types, and identifiers in English. Full diacritics required in French strings (`é`, `à`, `ô`, `ç`).
- Runtime 3 IR contract (`specs/ZACUS_RUNTIME_3_SPEC.md`) is sacred. Atelier consumes via `@zacus/scenario-engine`.
- pnpm + turbo monorepo. Node ≥20, pnpm ≥9.
- Initial atelier bundle must stay < 200 KB (Blockly and Three lazy-loaded).
- No commits with AI co-author attribution (project CLAUDE.md rule).

---

## 1. Architecture — "Stage right" (Scratch-like split)

```
┌──────────────┬──────────────────────┬───────────────────────┐
│              │                      │                       │
│   Toolbox    │  Blockly workspace   │   3D simulation       │
│   (blocks)   │                      │   stage (Three.js)    │
│              │                      │                       │
├──────────────┴──────────────────────┴───────────────────────┤
│  Validation console + mode tabs [Sandbox] [Demo] [Test]      │
└──────────────────────────────────────────────────────────────┘
```

- **Always split** — no full-screen mode.
- Panels drag-resizable via `react-resizable-panels`.
- Modes (Sandbox / Demo / Test) are **tabs in the bottom strip**, not full-screen modes.
- Optional later nuance: `⌘B` toggle to temporarily collapse the 3D stage for deep Blockly debug. **Not in MVP.**

## 2. V2 cutover — Hard, immediate

Done **before** atelier work begins.

- Tag: `archive/frontend-scratch-v2-final` on `main` HEAD before deletion.
- `git rm -r frontend-scratch-v2/` (including its `CLAUDE.md` created on 2026-05-01).
- Makefile: `frontend-test`, `frontend-build`, `frontend-lint` repointed to `pnpm --filter` V3 (or removed if redundant with `pnpm test/build/lint` from `frontend-v3/`).
- Root `CLAUDE.md`: "Where to Look" table + Architecture stripped of V2 references.
- `frontend-v3/CLAUDE.md`: "V2 vs V3" section deleted.

**No backward-compatibility shim, no deprecation period.** V2 is dead code — the user has decided no V2 feature is worth porting.

## 3. Naming

Merged app: **`@zacus/atelier`**.

Justification:
- French branding consistent with the project ("Le Mystère du Professeur Zacus") and steampunk/cabinet-of-curiosities theme.
- Avoids collision with the desktop app (`zacus-studio`). Mental model: *Zacus Studio Desktop* is the macOS shell ; *Atelier* is the workshop space inside.
- No refactor of the desktop app required.

## 4. Technical design

### 4.1 State

Three zustand stores (`apps/atelier/src/stores/`):

| Store | Owns | Subscribers |
|------|------|-------------|
| `editorStore` | Blockly XML/JSON, current selection, toolbox state | `BlocklyWorkspace` |
| `runtimeStore` | Compiled Runtime 3 IR, simulation execution state, current mode | `Stage`, mode components |
| `validationStore` | Compile errors, Zod errors, runtime warnings, test failures | `ValidationConsole` |

`@zacus/scenario-engine` remains the source of truth for IR types and execution. Atelier imports `compile`, `simulate`, `runTests` from it — no IR redefinition.

### 4.2 Live-diff strategy

Chosen over Live auto and Hybrid threshold.

- Any change in `editorStore` → debounce 500ms → call `compile(blocklyJson) → ir`.
- Diff `ir` against `runtimeStore.currentIr`. If different → show **stale badge** on the 3D stage corner.
- User clicks **Run** → `runtimeStore.currentIr = ir` and simulation replays from start.
- Compile errors and Zod failures surface in `ValidationConsole` and **block the Run button**.

Rationale: 3D stage re-init costs (Three.js, fiber tear-down). Auto-replay on every keystroke would thrash. Stale badge keeps the Scratch-like immediacy without the cost.

### 4.3 Mode semantics

| Mode | Behaviour | Trigger |
|------|-----------|---------|
| Sandbox | Author mode: scrub timeline, jump to puzzle, force NPC state | Default on load |
| Demo | Continuous playback, no pause, auto-skip dialogue | Click "Demo" tab |
| Test | Scripted execution with assertions from `tests/runtime3/*.yaml`. Failures → red in `ValidationConsole`. | Click "Test" tab |

Mode tabs are always visible in the bottom strip; switching is instantaneous (no recompile).

### 4.4 Layout

CSS Grid 3-column + bottom row. `react-resizable-panels` for the column dividers and the bottom strip splitter. No tab/route system in v1.

### 4.5 Code-splitting

- Initial chunk : layout shell + zustand + console = **< 200 KB**.
- Blockly chunk : lazy via `React.lazy` on first render of left+center panels.
- Three.js chunk : lazy via `React.lazy` on first render of right panel.
- Verified at build time via `vite build --report` or `rollup-plugin-visualizer`.

### 4.6 Persistence

- Read YAML from disk via Electron IPC (Zacus Studio) or `<input type="file">` in browser dev mode.
- Save via Electron IPC (write to original path) or browser download (`URL.createObjectURL`).
- No IndexedDB, no localStorage for scenario content. Source of truth is the `.yaml` file on disk.

### 4.7 Mock WS server

Reuse `apps/dashboard/mock/ws-server.ts`. Atelier in browser dev mode connects to `ws://localhost:3001` for fake firmware events. Ignored when running inside Zacus Studio Desktop (real serial).

---

## 5. Phased plan

| Phase | Scope | Acceptance | Rough effort |
|-------|-------|------------|--------------|
| **P1** | Hard cutover V2 | `make frontend-test && make frontend-build` green ; `rg "frontend-scratch-v2"` returns 0 hits outside `.git` ; tag `archive/frontend-scratch-v2-final` exists | 2-3 h |
| **P2** | Scaffold `@zacus/atelier` | `pnpm --filter @zacus/atelier dev` shows 3 drag-resizable empty panels ; `pnpm build` produces separate Blockly/Three chunks | 3-4 h |
| **P3** | Migrate editor → atelier | Blockly works in left+center identically to old `apps/editor` ; existing Vitest tests pass | 4-6 h |
| **P4** | Migrate simulation → atelier | 3D stage renders right ; Sandbox by default ; tabs switch Sandbox/Demo/Test ; puzzles P1/P5/P6/P7 visible | 4-6 h |
| **P5** | Live-diff wiring | Editing a block flashes stale badge ; Run replays simulation ; compile error blocks Run + surfaces in console | 4-8 h |
| **P6** | Decommission + propagation | `git rm -r apps/editor/ apps/simulation/` ; `desktop/scripts/build-frontends.sh` updated ; root + nested CLAUDE.md updated ; `pnpm install && pnpm build && pnpm typecheck && pnpm test` green ; manual smoke test E2E (open YAML → edit → Run → 3D updates) | 1-2 h |

**Total**: ~20-30 h focused work. Realistic spread: 1-2 weeks alongside other work.

---

## 6. Out-of-scope follow-ups

To consider after MVP:

- `⌘B` toggle to collapse 3D stage temporarily.
- Layout persistence (save panel sizes per user).
- Multi-scenario tabs in atelier (currently one scenario at a time).
- Test mode rich UI: assertion timeline, failure replay.
- Atelier ↔ Dashboard live link (changes in atelier reflect in running dashboard session).

---

## 7. Open questions

None at this design stage. All decisions resolved during 2026-05-01 brainstorm:
- Architecture: Approach 1 (Stage right) ✅
- V2 cutover: Option A (hard immediate) ✅
- Naming: `@zacus/atelier` ✅
- Live strategy: Live-diff ✅
- Modes: Sandbox/Demo/Test as bottom tabs ✅

## 8. Validation gates before merge

- All acceptance criteria for phases P1–P6 met.
- `pnpm typecheck`, `pnpm test`, `pnpm lint`, `pnpm build` green at root.
- `make all-validate` green (scenario + audio + printables + Runtime 3).
- Manual smoke test E2E run on macOS in Zacus Studio Desktop.
- No reference to `apps/editor`, `apps/simulation`, or `frontend-scratch-v2` anywhere in the repo (excluding archive tag).
