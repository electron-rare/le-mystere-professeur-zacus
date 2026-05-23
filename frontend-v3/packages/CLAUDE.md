# Frontend Packages

Shared workspace packages consumed by `apps/atelier` and `apps/dashboard` via `workspace:*`. Three packages, each a distinct concern.

```
packages/
  scenario-engine/   # Pure TS scenario interpreter (no React) — engine + profiler + scorer
  shared/            # Types, constants, yaml-parser — zero runtime deps beyond yaml
  ui/                # React component library + Tailwind tokens (tokens.css)
```

## Scope rules

- **`scenario-engine/`** — no React, no DOM. Pure functions over the Runtime 3 IR. Atelier instantiates it for live simulation ; dashboard does not import it. Tests live in `__tests__/`.
- **`shared/`** — `types.ts` is the source of truth for IR / event / WS message shapes. `yaml-parser.ts` is the only place that parses scenario YAML on the web side (the canonical compiler is `tools/scenario/compile_runtime3.py`). If a type is used by both apps, it lives here.
- **`ui/`** — design-system components only (buttons, panels, layout primitives). App-specific composite components stay in the app. `tokens.css` defines the CSS variables ; `tailwind.config.ts` reads them.

## Build

Each package builds independently :

```bash
pnpm --filter @zacus/scenario-engine build
pnpm --filter @zacus/shared build
pnpm --filter @zacus/ui build
```

TS project references wire it together (`tsconfig.json` in each package + root `tsconfig.json`).

## Patterns

- **Re-export from `index.ts`** — apps import `@zacus/shared`, never `@zacus/shared/src/types`.
- **Bump together** : a breaking change in `shared` requires touching both apps in the same PR. Keep the public API small to make this cheap.
- **No app code in packages** — if it references atelier / dashboard concepts, it stays in the app.

## Anti-patterns

- Adding React to `scenario-engine` — it must run in a Node test harness without DOM.
- Putting Tailwind classes in `shared` — `shared` has no view layer.
- Duplicating YAML parsing logic in an app — extend `yaml-parser.ts` instead.
- Cross-importing between packages beyond `shared → (none)`, `ui → shared`, `scenario-engine → shared`. No cycles.
