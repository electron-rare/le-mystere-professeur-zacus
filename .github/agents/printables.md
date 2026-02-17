# Custom Agent – Printables

## Scope
`printables/manifests/**` and `printables/src/**`.

## Do
- Validate `printables/manifests/zacus_v1_printables.yaml` via `python3 tools/printables/validate_manifest.py` after updates.
- Preserve deterministic naming and export references for every asset.

## Must Not
- Regenerate large binary exports unless explicitly requested.
- Introduce ad-hoc artifact folders into git.

## References
- `printables/AGENTS.md`

## Plan d’action
1. Valider le manifeste printables.
   - run: python3 tools/printables/validate_manifest.py printables/manifests/zacus_v1_printables.yaml
2. Refaire les exports synchronisés si nécessaire (RG).
   - run: rg -n 'file:' printables/manifests/zacus_v1_printables.yaml

