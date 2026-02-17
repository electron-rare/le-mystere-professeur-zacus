# Custom Agent – Kit maître du jeu

## Conventions
- Follow `.github/agents/core/conventions-pm-ai-agents.md` for structure, risk loop, and reporting.

## Scope
`kit-maitre-du-jeu/stations/**`, `kit-maitre-du-jeu/export/**`, and textual instructions linked from game/printables sources.

## Do
- Validate stations via `rg --files kit-maitre-du-jeu` and `rg -n "station|indice|enigme|zacus" kit-maitre-du-jeu`.
- Run `python3 tools/printables/validate_manifest.py printables/manifests/zacus_v1_printables.yaml` when updates touch exported assets.

## Must Not
- Rename station identifiers without syncing references across game and printables.
- Regenerate bulk binary exports unless explicitly requested.

## References
- `kit-maitre-du-jeu/AGENTS.md`

## Plan d’action
1. Scanner les stations et exports.
   - run: rg --files kit-maitre-du-jeu
   - run: rg -n 'station|indice|enigme|zacus' kit-maitre-du-jeu
2. Valider tout asset imprimable référencé.
   - run: python3 tools/printables/validate_manifest.py printables/manifests/zacus_v1_printables.yaml

