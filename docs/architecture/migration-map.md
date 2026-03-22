# Migration Map

## Migration Route

```mermaid
flowchart LR
  LegacyDocs["Conflicting docs/specs"] --> CanonDocs["Unified architecture docs"]
  LegacyFront["fronted dev web UI"] --> ReactStudio["frontend-scratch-v2"]
  StoryV2["Story V2 runtime"] --> Runtime3["Zacus Runtime 3"]
  DirectFirmwareStory["Firmware-owned story semantics"] --> FirmwareAdapter["Firmware adapter over IR"]
  AdHocTracking["Scattered TODO/plan refs"] --> RefactorCoord["memory/ + plans/ + todos/"]
```

## Rules
- No destructive cleanup before replacement proof.
- Legacy routes are quarantined first, then removed.
- Hardware evidence stays in `hardware/firmware/docs/AGENT_TODO.md` until the adapter route is stable.

## Current Quarantine Targets
- `fronted dev web UI/`
- Svelte/Cytoscape specs and references
- `zacus_v1` references outside archive or historical context
- duplicate docs such as `docs/AGENTS 2.md` and `docs/AGENT_TODO 2.md`
