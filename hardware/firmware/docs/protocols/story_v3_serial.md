# Story Serial Protocol V3

## Transport

- Line-based serial protocol.
- One JSON request per line.
- One JSON response per line.

## Request shape

```json
{"cmd":"story.status"}
{"cmd":"story.load","data":{"scenario":"DEFAULT"}}
{"cmd":"story.step","data":{"step":"STEP_WAIT_UNLOCK"}}
{"cmd":"story.event","data":{"event":"UNLOCK"}}
```

## Response shape

```json
{"ok":true,"code":"ok","data":{...}}
{"ok":false,"code":"bad_args","data":{"detail":"..."}}
```

Fields:

- `ok`: boolean success flag.
- `code`: machine-readable status/error code.
- `data`: payload object.

## Commands

- `story.status`
  - Returns runtime state, scenario, step, source, and last error.
- `story.list`
  - Returns known scenarios from LittleFS/generated catalog.
- `story.load`
  - Input: `data.scenario`.
  - Loads target scenario.
- `story.step`
  - Input: `data.step`.
  - Forces jump to step.
- `story.validate`
  - Validates active scenario wiring.
- `story.event`
  - Input: `data.event`.
  - Posts a runtime event.

## Compatibility policy

- `STORY_V2_*` commands are removed from routing.
- Legacy non-V3 debug commands (`STORY_STATUS`, `STORY_ARM`, etc.) may remain for low-level manual operations, but protocol automation should use `story.*` JSON-lines.
