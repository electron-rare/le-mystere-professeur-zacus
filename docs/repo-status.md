# Repo Status Report

## Tree summary
- `kit-maitre-du-jeu/`: full GM script, solutions, checklists, and export folders for PDF/PNG deliverables.
- `printables/`: `src/` plus `export/{pdf,png}/` for invitations, cards, badges, and the one-page rule set.
- `hardware/`: BOMs, wiring docs, and the ESP32/Arduino firmware flow with `.pio` dependencies (libraries such as Adafruit and ESP8266Audio naturally expand here).
- `docs/`: maintenance plan, repo audit, and asset folder (`docs/assets/` contains the repo map SVG already referenced in `README.md`).
- `examples/` and `include humain:IA/`: auxiliary content and tools, the latter already named with spaces/colon which will need special handling.

## Empty / TODO files
- `printables/invitations/export/pdf/.gitkeep`
- `printables/invitations/export/png/.gitkeep`
- `printables/invitations/src/.gitkeep`
- assorted `.nojekyll`, `.uno.test.skip`, and upstream library placeholders under `hardware/firmware/esp32/.pio/` (e.g., Adafruit BusIO examples); none appear to contain TODO hints, but they are empty files that may surprise contributors or scripts.

## Broken links
- `hardware/firmware/esp32/.pio/libdeps/esp32dev/ESP8266Audio/README.md` line 250 → `examples/StreamMP3FromHTTP_SPIRAM/Schema_Spiram.png` (missing in that vendor snapshot).
- `hardware/firmware/esp32/.pio/libdeps/esp32_release/ESP8266Audio/README.md` line 250 → same missing `Schema_Spiram.png`.
- `hardware/firmware/esp32/.pio/libdeps/esp32dev/Mozzi/README.md` line 116 → `extras/NEWS.txt` (not included in the copied tree).
- `hardware/firmware/esp32/.pio/libdeps/esp32_release/Mozzi/README.md` line 116 → same missing `extras/NEWS.txt`.

## Naming / portability issues
- `include humain:IA/` and its subdirectory `version finale` contain spaces and a colon; these characters are tolerated by POSIX shells but will break many Windows commands, scripts, and tooling that expect simple identifiers. Consider renaming or documenting quoting rules.

## License inconsistencies
- `README.md` toggles between a CC BY-NC 4.0 / MIT split (lines 41‑46) and a CC BY-SA 4.0 / GPL-3.0-or-later pair (lines 48‑51), creating conflicting messaging about what applies where.
- `CONTRIBUTING.md` repeats the dual declaration (lines 25‑29), reinforcing the confusion for would-be contributors.
- `LICENSE.md` only lists GPL-3.0-or-later for code and CC BY-SA 4.0 for creative work (no non-commercial terms), making the README/CONTRIBUTING statements inconsistent with the canonical license file.

## Patch plan
- Draft `docs/repo-status.md` (done) as the summary deliverable requested.
- Decide whether the empty `.gitkeep`, `.nojekyll`, and `.uno.test.skip` placeholders should stay (documented here) or be pruned.
- Fix or suppress the four broken vendor links (either add the missing assets to the copies or remove the references from the embedded README sections).
- Rename or specially document `include humain:IA/` (and its child directories) to avoid spaces/colon for future automation.
- Align licensing statements across `README.md`, `CONTRIBUTING.md`, and `LICENSE.md` so the repo advertises a single authoritative regime for creative vs. code content.
