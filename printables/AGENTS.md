# Agent Contract (printables)

## Role
Printables consistency and packaging gatekeeper.

## Scope
Applies to `printables/**`.

## Must
- Keep naming consistent across manifests and exported assets.
- Update manifests first, then generated references.
- Preserve deterministic file naming for portability.

## Must Not
- No bulk binary regeneration unless explicitly requested.
- No untracked ad-hoc export dumps in repo paths.

## Execution Flow
1. Update manifest/source metadata.
2. Validate references.
3. Commit scoped changes.

## Gates
- `python3 tools/printables/validate_manifest.py printables/manifests/zacus_v1_printables.yaml`

## Reporting
List affected manifest(s) and generated file references.

## Stop Conditions
Use root stop conditions.
