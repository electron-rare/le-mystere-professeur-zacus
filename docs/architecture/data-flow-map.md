# Data Flow Map

```mermaid
flowchart TD
  Author["Author edits Blockly graph"] --> Draft["Scenario draft in browser"]
  Draft --> Yaml["Canonical YAML"]
  Yaml --> Validate["Scenario validators"]
  Yaml --> Compile["Runtime 3 compiler"]
  Compile --> IR["Runtime 3 JSON"]
  IR --> Sim["Local simulator"]
  IR --> Firmware["Firmware adapter"]
  Yaml --> Exports["Audio / printables / MJ kit exports"]
  Firmware --> Api["Runtime API + status"]
  Api --> Studio["Studio dashboard + diagnostics"]
  Api --> Evidence["Logs / artifacts / AGENT_TODO evidence"]
```

## Guarantees
- Narrative truth remains in YAML during the migration window.
- Runtime truth is emitted as versioned IR JSON.
- Hardware evidence stays outside git and is referenced from planning/TODO docs.
