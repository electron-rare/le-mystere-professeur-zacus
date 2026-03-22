# Agent Matrix

| Agent | Scope | Inputs | Outputs |
| --- | --- | --- | --- |
| PM / Architecture | Global audit, sequencing, dependency map | repo state, plans, docs | `plans/master-plan.md`, architecture maps |
| Runtime / Compiler | Runtime 3 schema, compiler, simulator | YAML scenarios, runtime spec | IR JSON, simulator traces, migration rules |
| Frontend Studio | React + Blockly authoring studio | Runtime 3 contract, API contract | Studio UI, previews, lint/build gates |
| Firmware Adapter | Runtime execution on device | IR JSON, firmware APIs | buildable firmware, runtime adapter |
| Content / Narrative | Scenario migration and feature cards | YAML canon, audio/printables manifests | consistent scenario graph, content backlog |
| Tooling / TUI / Logs | CLI/TUI wrappers and evidence | scripts, CI, artifact dirs | `tools/dev/zacus.sh`, validation entrypoints |
| Docs / Knowledge | Specs, Mermaid maps, READMEs | all workstreams | updated docs, MkDocs site |
| QA / Release | Gates, proofs, release readiness | builds, logs, simulations | validation matrix, release checklist |

## Coordination Rule
- `memory/project-memory.md` stores cross-wave context.
- `plans/agents/*.md` store agent-specific execution plans.
- `todos/*.md` store the next concrete chain of work.
