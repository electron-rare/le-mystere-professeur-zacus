# Frontend Scratch V2

Authoring studio: React 19 + Blockly 12 + Monaco Editor + Zod + Vite 7 + Vitest 3. TypeScript strict.

## Layout

```
src/
  App.tsx, main.tsx          # Entry
  components/                # UI (scenario editor, blocks, panels)
  lib/
    api.ts                   # ESP32/runtime HTTP client
    runtime3.ts              # Runtime 3 IR types + compiler bindings (TS mirror of Python)
    scenario.ts              # Scenario YAML/JSON helpers
    useRuntimeStore.ts       # Zustand-style runtime state hook
  __tests__/                 # Co-located unit tests
tests/
  scenario-runtime3.test.ts  # Integration test against real fixtures
```

## Commands

```bash
npm run dev        # Vite dev server
npm run build      # tsc -b && vite build
npm test           # Vitest run (18 tests)
npm run lint       # ESLint flat config
```

## Patterns

- `lib/runtime3.ts` is the TS mirror of Python `tools/scenario/runtime3_common.py` — keep field names + optionality identical. When the Python IR changes, update both in the same PR.
- Validate scenario imports through Zod schemas, not raw `JSON.parse` — Zod errors surface bad fixtures during dev.
- Blockly toolbox/blocks live in `components/scenario-editor/` — register custom blocks before `Blockly.inject`.
- Monaco editor language is YAML; load worker via Vite import, never CDN.

## Tests

- Vitest config inherits from `vite.config.ts`. Use `vitest run --reporter verbose` for diff output.
- Cross-stack invariant tests (Python compile → TS replay) live in `tests/scenario-runtime3.test.ts`. Run them locally before claiming Runtime 3 work is done.

## Anti-Patterns

- Drifting `lib/runtime3.ts` types from the Python compiler — schedule a sync, don't bandaid
- Calling `fetch()` outside `lib/api.ts` — keep network code centralised
- Using `any` to escape Zod typing — narrow with `z.infer` instead
- Importing from `frontend-v3/` packages — V2 and V3 share no runtime code
