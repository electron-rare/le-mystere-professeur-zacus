# Release Map

```mermaid
flowchart LR
  Wave0["Wave 0 Baseline"] --> Wave1["Wave 1 Canon"]
  Wave1 --> Wave2["Wave 2 Runtime 3"]
  Wave2 --> Wave3["Wave 3 Studio"]
  Wave3 --> Wave4["Wave 4 Firmware"]
  Wave4 --> Wave5["Wave 5 Cleanup"]
  Wave5 --> Wave6["Wave 6 Cutover"]
```

## Exit Criteria
- Wave 0: baseline evidence captured and dirty-tree risks understood.
- Wave 1: architecture/spec/docs/plans canonized.
- Wave 2: Runtime 3 compiler and simulator usable on the canonical scenario.
- Wave 3: React/Blockly studio builds and previews Runtime 3.
- Wave 4: Freenove build path consumes the runtime contract without regression.
- Wave 5: legacy paths archived or deleted with proof of replacement.
- Wave 6: README, docs site, CI, and operator entrypoints all point to the new canon.
