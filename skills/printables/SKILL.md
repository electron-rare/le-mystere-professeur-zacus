# Name
printables

## When To Use
Use for printables/manifests consistency, naming, and export hygiene.

## Trigger Phrases
- "printables"
- "manifest"
- "asset naming"
- "export rules"

## Do
- Update manifests before generated references.
- Preserve deterministic names for printable assets.
- Validate printables manifest after edits.

## Don't
- Do not regenerate binary assets unless explicitly requested.
- Do not introduce ad-hoc export folders in git.

## Quick Commands
- `python3 tools/printables/validate_manifest.py printables/manifests/zacus_v1_printables.yaml`
- `rg -n "zacus_v1_printables" printables`
- `rg --files printables`
