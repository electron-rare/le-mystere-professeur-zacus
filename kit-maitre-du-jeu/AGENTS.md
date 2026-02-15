# Kit Maitre du Jeu Agent Contract

Purpose: maintain GM kit consistency across prompts, stations, and export references.

Allowed scope:
- `kit-maitre-du-jeu/stations/**`
- `kit-maitre-du-jeu/export/**`
- textual instructions linked from game/printables sources

Validate:
- `rg --files kit-maitre-du-jeu`
- `rg -n "station|indice|enigme|zacus" kit-maitre-du-jeu`

Common commands:
- `python3 tools/printables/validate_manifest.py printables/manifests/zacus_v1_printables.yaml`

Do not:
- regenerate bulk binary exports unless explicitly requested
- rename station identifiers without synchronizing game/printables references
