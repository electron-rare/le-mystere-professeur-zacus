# Zacus Master Plan

## Objective
Ship a coherent Zacus V3 platform that unifies content authoring, runtime compilation, hardware execution, and documentation.

## Waves
1. Baseline and freeze
   - Capture repo drift, failing builds, validator status, CI drift, and legacy inventory.
   - Establish canonical coordination files in `memory/`, `plans/`, and `todos/`.
2. Product canon
   - Publish system map, feature map, migration map, runtime spec, and agent matrix.
   - Rewrite contradictory frontend and repo structure specs.
3. Runtime 3 core
   - Define IR schema.
   - Build YAML/Blockly -> IR compiler.
   - Add a local Runtime 3 simulator.
4. Studio canon
   - Stabilize React + Blockly.
   - Support roundtrip YAML <-> graph and JSON IR preview.
   - Align frontend API assumptions with Runtime 3.
5. Firmware adapter
   - Load Runtime 3 artifacts from firmware.
   - Expose canonical runtime APIs.
   - Restore green Freenove builds with memory margin.
6. Cleanup and cutover
   - Archive or remove legacy frontend/docs/specs after replacement proof.
   - Finalize docs, CI, runbooks, and release gates.

## Phase Exit Criteria
- Wave 1 exits when the repo has one documented target architecture.
- Wave 3 exits when Runtime 3 artifacts can be compiled and simulated locally.
- Wave 4 exits when the studio builds cleanly and can preview Runtime 3 JSON from authored content.
- Wave 5 exits when firmware builds green against the Runtime 3 adapter path.
- Wave 6 exits when legacy references are quarantined or removed and CI enforces the new route.

## Immediate Next Slice
- Retire the remaining `steps_reference_order` fallback from non-canonical importers once the adapter and studio deploy path no longer need compatibility output.
- Add Runtime 3 deploy/refresh actions once the adapter accepts native IR payloads.
- Continue quarantining legacy docs/agent references that still point to `docs/AGENT_TODO.md` or `zacus_v1`.
- Split the oversized frontend bundle once functionality is stable.

## Current Progress
- Wave 1 delivered: coordination files, runtime spec, architecture maps, updated README/Quickstart/Structure docs.
- Wave 3 slice delivered: React + Blockly studio builds and previews Runtime 3 JSON.
- Wave 3.5 delivered: Runtime 3 regression tests and firmware bundle export are part of the canonical validation flow.
- Wave 4 slice delivered: the canonical scenario now defines explicit `firmware.steps` transitions and compiles through the Runtime 3 firmware-import path.
- Wave 4.5 slice delivered: the studio dashboard now surfaces Runtime 3 adapter metadata without relying only on raw endpoint buttons.
- Wave 4.6 slice delivered: the studio now has a dedicated frontend regression gate for graph-first parsing and Runtime 3 compilation.
- Wave 4.7 slice delivered: the canonical scenario and studio builder no longer emit a top-level `steps_reference_order` contract.
- Wave 5 slice delivered: firmware now discovers Runtime 3 bundle metadata and exposes adapter status/document endpoints.
- Wave 5 build gate recovered: `freenove_esp32s3` and `esp8266_oled` both compile green.
