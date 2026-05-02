# Frontend V3 (Monorepo)

pnpm + turbo monorepo. Three apps + three shared packages. Node ≥20, pnpm ≥9.

## Layout

```
apps/
  dashboard/         # Live game-master dashboard (analytics, control)
  editor/            # Scenario editor (will fuse with simulation into atelier — see docs/superpowers/specs/2026-05-01-v3-fusion-atelier-design.md)
  simulation/        # Runtime simulator / playtest UI
packages/
  scenario-engine/   # Runtime 3 IR + execution (shared core)
  shared/            # Cross-app utilities, types, constants
  ui/                # Shared component library
```

## Commands

```bash
pnpm install            # Bootstrap workspace
pnpm dev                # turbo run dev (all apps)
pnpm --filter dashboard dev   # Single app
pnpm build              # turbo run build
pnpm test               # turbo run test
pnpm typecheck          # turbo run typecheck
pnpm lint               # turbo run lint
```

## Patterns

- `scenario-engine` is the canonical TS Runtime 3 implementation. New apps consume it; do not fork the IR.
- All cross-app types live in `packages/shared` — never copy-paste types between apps.
- Components shared across ≥2 apps move to `packages/ui`. Keep app-local components in `apps/<app>/src/components`.
- Turbo cache keys: ensure `inputs` in `turbo.json` cover all source paths; missing globs cause stale cache.

## Anti-Patterns

- Adding deps to root `package.json` (use the relevant app/package)
- Bypassing turbo with manual `tsc` invocations — breaks cache invalidation
- Circular dependencies between packages (turbo will warn; fix immediately)
- Publishing `packages/*` to npm — they're internal workspace-only
