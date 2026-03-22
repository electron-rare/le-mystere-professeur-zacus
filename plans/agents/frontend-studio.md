# Agent Frontend Studio

## Scope
- `frontend-scratch-v2/` and the future studio cutover.

## Responsibilities
- Stabilize React + Blockly as the canonical studio.
- Keep authoring, preview, and runtime observability aligned with Runtime 3.
- Eliminate dependency on legacy Svelte/Cytoscape assumptions.

## Current Tasks
- Fix the current TypeScript build.
- Add Runtime 3 preview from the Blockly authoring model.
- Prepare `apps/studio` migration once the React route is stable.
- Add Runtime 3 deploy/refresh UX once the adapter accepts native IR payloads.
- Keep the frontend regression gate green (`npm test`, `frontend-test`).
