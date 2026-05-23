# shared

Source of truth for cross-app types, constants, and scenario YAML parsing. **No view layer, no React**. Runtime deps : `js-yaml` only.

```
src/
  types.ts        # IR shapes, event shapes, WS message shapes, ScenarioEngine interface
  constants.ts    # Shared magic numbers / enums
  yaml-parser.ts  # YAML → typed scenario object
  index.ts        # `export * from` each of the above
```

## Rules

- **`types.ts` is the contract**. Both apps and `scenario-engine` import from here. A type change is a workspace-wide change : bump together, check both apps compile.
- **`yaml-parser.ts` is the only web-side YAML reader**. The canonical compiler is the Python one in `tools/scenario/compile_runtime3.py` — this parser exists so the atelier can preview / live-diff without a Python round-trip. If they diverge, the Python compiler wins ; mirror the change here.
- **Constants stay tiny** — if a value is used in one place, inline it. Promote to `constants.ts` on the second use.
- **`.js` import suffix** in TS sources (NodeNext ESM). The existing `index.ts` follows this.

## Anti-patterns

- Adding a runtime dep beyond `js-yaml` — the value of `shared` is that it's cheap to embed everywhere.
- Re-exporting types under a renamed alias — keep one canonical name.
- App-specific types leaking in (e.g. dashboard panel props) — those stay in the app.
- Parsing YAML in an app component — go through `yaml-parser` so format changes have one place to land.
