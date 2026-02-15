# Agent Contract (hardware/firmware/tools/dev)

## Role
Tooling conventions for firmware helper scripts.

## Scope
Applies to `hardware/firmware/tools/dev/**` scripts and helpers.

## Must
- Provide `--help` and sane non-interactive defaults.
- Use clear exit codes (`0` success, non-zero actionable failure).
- Keep port resolution and timeouts script-local and configurable by flags/env.
- Write logs to `hardware/firmware/logs/` with timestamped filenames.
- Keep CLI output short and grep-friendly (`[step]`, `[ok]`, `[fail]`).

## Must Not
- Do not require chat/operator interaction for waiting when script automation is possible.
- Do not hardcode machine-specific serial paths in committed scripts.

## Execution Flow
1. Detect dependencies.
2. Resolve ports with timeout.
3. Execute steps with per-step logging.
4. Emit clear summary status.

## Gates
- `python3 hardware/firmware/tools/dev/serial_smoke.py --help`
- `bash hardware/firmware/tools/dev/run_matrix_and_smoke.sh` (when hardware context is available)

## Reporting
Surface exact rerun command on failure.

## Stop Conditions
Use root stop conditions.
