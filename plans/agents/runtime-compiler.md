# Agent Runtime / Compiler

## Scope
- Zacus Runtime 3 IR, compiler, validator, simulator, and migration layer from the current YAML model.

## Responsibilities
- Define IR types and schema.
- Compile canonical scenario YAML or Blockly-authored data into Runtime 3 JSON.
- Provide local simulation hooks without hardware.

## Current Tasks
- Maintain `specs/ZACUS_RUNTIME_3_SPEC.md`.
- Maintain `tools/scenario/compile_runtime3.py`.
- Maintain `tools/scenario/simulate_runtime3.py`.
- Remove the transitional `steps_reference_order` bridge after the graph path is fully adopted by tooling and studio imports.
- Keep Runtime 3 regression tests green in `tests/runtime3/`.
