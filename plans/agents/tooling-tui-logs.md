# Agent Tooling / TUI / Logs

## Scope
- CLI/TUI entrypoints, logging, artifacts, and developer ergonomics.

## Responsibilities
- Keep `tools/dev/zacus.sh` as the main entrypoint until the refactor shell fully stabilizes.
- Add Runtime 3 compile/simulate/log cleanup commands.
- Standardize logs, summaries, and cleanup behavior.

## Current Tasks
- Add Runtime 3 commands to `tools/dev/zacus.sh`.
- Keep logs grep-friendly and safe in non-interactive mode.
- Introduce explicit log cleanup behavior instead of ad-hoc manual deletion.
