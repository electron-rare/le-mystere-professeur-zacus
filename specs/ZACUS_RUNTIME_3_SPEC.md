# Zacus Runtime 3 Specification

## Goal
Define a portable runtime artifact that can be compiled from YAML or Blockly-authored content and executed by firmware or simulated locally.

## Design Principles
- Narrative canon remains in YAML during migration.
- Runtime IR is deterministic, versioned, and JSON serializable.
- Firmware consumes Runtime 3 but does not own narrative semantics.
- Import from legacy canonical YAML is allowed in a `linear_import` migration mode.

## IR Shape

```json
{
  "schema_version": "zacus.runtime3.v1",
  "scenario": {
    "id": "ZACUS_V2",
    "version": 3,
    "title": "Le mystère du Professeur Zacus",
    "entry_step_id": "STEP_U_SON_PROTO",
    "source_kind": "yaml"
  },
  "steps": [
    {
      "id": "STEP_U_SON_PROTO",
      "scene_id": "SCENE_U_SON_PROTO",
      "audio_pack_id": "",
      "actions": [],
      "apps": [],
      "transitions": []
    }
  ],
  "metadata": {
    "migration_mode": "native",
    "generated_by": "zacus_runtime3"
  }
}
```

## Transition Model
- `event_type`: `button`, `serial`, `timer`, `audio_done`, `unlock`, `espnow`, `action`
- `event_name`: opaque token preserved as runtime contract
- `target_step_id`: required
- `priority`: integer, lower first
- `after_ms`: only meaningful for timer-style transitions

## Migration Modes
- `native`: authored directly for Runtime 3 with explicit transitions
- `linear_import`: derived from the legacy canonical YAML using ordered steps when no explicit graph exists

## Execution Model
- Runtime loads one IR document.
- Entry step is resolved from `scenario.entry_step_id`.
- A simulator can replay deterministic transition events without hardware.
- Firmware adapters may enrich steps with board-specific actions, but not mutate story flow semantics.
