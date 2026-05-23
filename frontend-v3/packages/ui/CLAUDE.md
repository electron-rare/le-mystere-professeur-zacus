# ui

Shared React component library + design tokens. Current surface : `Button`, `Card`, `Badge`, `Progress`. Built on Radix primitives (`@radix-ui/react-{dialog,progress,toggle,tooltip}`), styled via Tailwind + `tokens.css`.

```
src/
  components/    # Button, Card, Badge, Progress (one file per component)
  tokens.css     # CSS variables — exported as @zacus/ui/styles.css
  index.ts       # Re-exports component + props type per component
tailwind.config.ts
```

Consumer imports :

```ts
import { Button, type ButtonProps } from '@zacus/ui';
import '@zacus/ui/styles.css';  // tokens.css
```

## Rules

- **Headless + tokens** : Radix gives behaviour, `tokens.css` gives visual identity. Don't hardcode colors in components — reference CSS variables.
- **Promotion bar is high** : a component lives in the app until ≥2 apps need it. Premature `ui` entries become churn.
- **Props named `{Name}Props` and exported** alongside the component — atelier and dashboard rely on these to type their wrappers.
- **Tailwind is allowed here** (unlike `apps/atelier` which is inline-styles / `main.css`). The `tailwind.config.ts` lives next to the source.
- **No app state, no zustand, no fetch** — components are stateless or use local `useState` only.

## Anti-patterns

- Adding a feature-specific component (e.g. `<NpcMoodBadge/>`) — that belongs in the app that owns the concept.
- Importing from `@zacus/scenario-engine` — `ui` doesn't know about scenarios.
- Bundling icons / fonts — leave asset choice to the consumer.
- Breaking the `ButtonProps`-style export pattern — downstream code does `extends ButtonProps`.
