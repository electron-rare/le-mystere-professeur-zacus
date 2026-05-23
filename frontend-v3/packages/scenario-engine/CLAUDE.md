# scenario-engine

TS implementation of the Runtime 3 IR — interpreter + group profiler + scorer. **Pure logic, no React, no DOM**, runs in Node test harness.

Public API (`src/index.ts`) :

```ts
ZacusScenarioEngine                     // engine.ts — IR executor
detectGroupProfile, profileConfidence   // profiler.ts — player group classification
computeScore, assembleCode              // scorer.ts — end-of-game scoring
ScenarioEngine                          // type re-export from @zacus/shared
```

## Files

- `src/engine.ts` — the executor. Holds run state, advances pivots, emits events.
- `src/profiler.ts` — analyses player behaviour to pick a group profile (drives adaptive hints + NPC tone).
- `src/scorer.ts` — final score + the assembled-code reveal.
- `__tests__/engine.test.ts` — Vitest, runs without DOM.

## Rules

- **Contract = `specs/ZACUS_RUNTIME_3_SPEC.md`** (repo root). The Python compiler `tools/scenario/compile_runtime3.py` produces the IR ; this package consumes it. Both must stay in lockstep — if you add an opcode here, add it to the spec and the Python compiler in the same PR.
- **Imports end in `.js`** (NodeNext ESM resolution) even though source is `.ts`. The `index.ts` already follows this.
- **Only dep is `@zacus/shared`** for types. Adding another runtime dep needs justification — this package is meant to be embedded in test harnesses and possibly server contexts.
- **No `Date.now()`, no `Math.random()`** in `engine.ts` without injection — tests must be deterministic. Pass a clock / RNG through the engine constructor.

## Anti-patterns

- Importing React / DOM / browser APIs.
- Forking the IR shape locally instead of extending `@zacus/shared/types`.
- Calling out to network / fs from the engine — it's a pure interpreter.
- Letting `profiler` and `scorer` reach into engine internals — they consume the same IR + event log the host gives them.
