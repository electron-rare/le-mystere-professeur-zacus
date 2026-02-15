# Printables Agent Contract

Purpose: printable assets and manifest consistency.

Allowed scope:
- `printables/manifests/**`
- `printables/src/**`
- exported references when explicitly requested

Validate:
- `python3 tools/printables/validate_manifest.py printables/manifests/zacus_v1_printables.yaml`
- `rg --files printables`

Common commands:
- `rg -n "id:|file:|export" printables/manifests`

Do not:
- regenerate bulk binary exports unless requested
- introduce ad-hoc artifact folders into git
