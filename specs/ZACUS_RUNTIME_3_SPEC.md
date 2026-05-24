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

## Action Shape

Actions are emitted by authoring tools (blocks studio, firmware import, hand-written YAML) and consumed by runtime adapters. Two equivalent shapes are accepted:

- **String form** (legacy firmware import): `"led_pattern:rainbow"` — opaque, runtime parses at its discretion.
- **Dict form** (preferred for new authoring): `{ "kind": "<name>", ...payload }`.

### Standard action kinds

| `kind` | Payload | Semantics |
|--------|---------|-----------|
| `tts_say` | `text: string` | Speak `text` via the active TTS backend |
| `wait_user_voice` | `timeout_ms: int` | Block until utterance end or timeout |
| `hw_servo` | `channel: int, angle: int` | Set servo on `channel` to `angle` degrees |
| `qr_expect` | `value: string` | Arm the QR matcher with expected payload |
| `led_pattern` | `pattern: string` | Play a named LED pattern |
| `sound_play` | `asset: string` | Play an asset from the media manager |
| `score_add` | `delta: int` | Mutate the run-scoped score variable |
| `set_var` | `name: string, value: string` | Set a scenario variable |
| `condition` | `expr: string, then: [action,…], else: [action,…]` | **Standalone branching.** Evaluate `expr`; execute the `then` action list if truthy, `else` otherwise. Nested `condition` actions are allowed. |

### `condition` action

The `condition` action keeps **branching local to the current step** — no extra transitions or step duplication needed for simple if/else logic. The runtime evaluates `expr` against the current variable scope (left intentionally loose; runtimes that don't implement evaluation MAY treat unknown `expr` as falsy and log it).

Authoring tools that produce `condition` actions (e.g. `BlockKind.logicIf` in the Blocks Studio) MUST place all branch-local actions inside `then` / `else`. Use a transition (event_type `action`, event_name `goto:<target>`) when you need to jump to another step, not a `condition`.

Example:
```json
{
  "kind": "condition",
  "expr": "score >= 3",
  "then": [
    { "kind": "tts_say", "text": "Bravo, niveau suivant." },
    { "kind": "set_var", "name": "unlocked_act2", "value": "true" }
  ],
  "else": [
    { "kind": "tts_say", "text": "Encore un effort." }
  ]
}
```
