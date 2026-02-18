# Story Portability Migration

## Canonical layout

- Canonical firmware layout: `hardware/firmware/...`
- Legacy paths are kept only via adapters/wrappers.
- No rollback to legacy tree.

## Path mapping snapshot

| Legacy path | Canonical path | Impact | Status |
|---|---|---|---|
| `esp32_audio/` | `hardware/firmware/esp32_audio/` | PlatformIO `src_dir`, include paths | done |
| `ui/esp8266_oled/` | `hardware/firmware/ui/esp8266_oled/` | UI build filters, scripts | done |
| `ui/rp2040_tft/` | `hardware/firmware/ui/rp2040_tft/` | UI build filters, scripts | done |
| `ui_freenove_allinone/` | `hardware/firmware/ui_freenove_allinone/` | Freenove include path/docs | done |
| `story_generator/story_specs/` | `docs/protocols/story_specs/` | story specs source and tooling | done |
| `esp32_audio/tools/story_gen/` | `hardware/libs/story/tools/story_gen/` + `lib/zacus_story_gen_ai/` | generation stack migrated to Yamale + Jinja2 | done |

## Tooling compatibility layer

- Shared path resolvers:
  - `tools/dev/layout_paths.sh`
  - `tools/dev/layout_paths.py`
- Story generator compatibility wrapper:
  - `hardware/libs/story/tools/story_gen/story_gen.py` delegates to `zacus_story_gen_ai`.

## Runtime migration

- Portable runtime library: `lib/zacus_story_portable/`
- Public facade kept stable: `StoryPortableRuntime`.
- Internal state handling moved to tinyfsm-style core (`tinyfsm_core.h` + runtime states).

## Protocol migration

- Story serial protocol V3 is now JSON-lines (`story.*` commands).
- Legacy `STORY_V2_*` commands are removed from command routing.
- See `docs/protocols/story_v3_serial.md`.
