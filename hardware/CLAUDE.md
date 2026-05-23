# Hardware

Physical artefacts of the escape room : firmware sources, PCB projects, enclosures, BOM, wiring diagrams, puzzle assembly. The ESP32-S3 master firmware lives in the top-level `ESP32_ZACUS/` submodule — **this folder is the surrounding hardware**, not the master firmware.

```
hardware/
  firmware/      # Arduino / PlatformIO sketches for puzzle satellites + UI boards
  projects/      # KiCad projects — plip-telephone, slic-phone, slic-phone-esp32
  puzzles/       # Per-puzzle BOM, GPIO mapping, assembly checklists, suitcase layout
  bom/           # Aggregated bill of materials (bom.md)
  wiring/        # Wiring diagrams
  enclosure/     # 3D-printed / laser-cut enclosure files
  ui_freenove_allinone/  # Reference Freenove board UI assets
```

## Where to look

| Task | Location |
|------|----------|
| Order parts | `bom/bom.md` + per-puzzle BOMs in `puzzles/BOM_V3_complete.csv` |
| Wire a puzzle station | `puzzles/gpio_mapping.md` + `wiring/` |
| Build a satellite firmware | `firmware/` (PlatformIO — `platformio.ini`) |
| Edit a KiCad schematic / PCB | `projects/<board>/` |
| Lay out the suitcase | `puzzles/suitcase_layout.md` |
| Print an enclosure | `enclosure/` |

## Patterns

- **PlatformIO is the firmware build system here**, not Arduino IDE. Each board has an env in `firmware/platformio.ini`. Override locally with `platformio_override.ini` (gitignored on field machines).
- **KiCad projects use the standard kicad skill** — see `/Users/electron/CLAUDE.md` for the kicad-pro MCP server. Don't hand-edit `.kicad_pcb` for routing.
- **BOM stays in sync with KiCad symbols** — use the `bom` skill / `lcsc` / `digikey` skills, not manual CSV edits.
- **Per-puzzle docs are the source of truth for assembly** — when GPIO assignment changes, update `puzzles/gpio_mapping.md` *and* the firmware constants in the same commit.

## Boundary with `ESP32_ZACUS/`

- `ESP32_ZACUS/` (submodule) = the **master** ESP32-S3 (NPC engine, voice pipeline, vision, media manager). Owns the game loop.
- `hardware/firmware/` = **satellite** boards (puzzle stations, secondary UIs, audio kits). They speak to the master over the protocol in `firmware/protocol/`.
- `PLIP_FIRMWARE/` (top-level) = the retro telephone annex. Distinct from `hardware/projects/plip-telephone` (which is the KiCad project for the same artefact).

## Anti-patterns

- Editing `ESP32_ZACUS/` files from here — it's a submodule with its own repo and CLAUDE.md.
- Duplicating BOM rows between `bom/bom.md` and `puzzles/BOM_V3_complete.csv` without marking one as derived.
- Changing GPIO assignments without grepping firmware *and* `puzzles/gpio_mapping.md`.
- Committing `.pio/`, `build/`, or local-override `platformio_override.ini`.
