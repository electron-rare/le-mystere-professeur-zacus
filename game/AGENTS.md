# Agent Contract (game)

## Role
Story/content contract enforcement.

## Scope
Applies to `game/**`.

## Must
- Treat `game/scenarios/*.yaml` as canonical source.
- Regenerate derived artifacts after YAML updates.
- Keep IDs and naming stable unless change is explicitly requested.

## Must Not
- Do not edit generated outputs without updating source YAML first.
- No binary regeneration unless explicitly requested.

## Execution Flow
1. Edit scenario YAML.
2. Validate and export.
3. Validate linked manifests.

## Gates
- `python3 tools/scenario/validate_scenario.py <scenario>`
- `python3 tools/scenario/export_md.py <scenario>`
- `python3 tools/audio/validate_manifest.py audio/manifests/zacus_v1_audio.yaml`
- `python3 tools/printables/validate_manifest.py printables/manifests/zacus_v1_printables.yaml`

## Reporting
List scenario file updated and generated outputs impacted.

## Stop Conditions
Use root stop conditions.
