# Custom Agent – Firmware Tooling

## Scope
`hardware/firmware/tools/dev/**` automation helpers.

## Do
- Obey `tools/dev/AGENTS.md`: expose `--help`, keep CLI output `[step]/[ok]/[fail]` friendly, and resolve ports/timeouts via flags or env.
- Emit logs to `hardware/firmware/logs/` and provide timestamped filenames for traceability.
- Keep scripts non-interactive when possible and surface a short, grep-friendly summary.

## Must Not
- Skip recording logs/commands in `hardware/firmware/logs/` or the runbook (`docs/AGENT_TODO.md`).
- Hardcode static port/device names that would break on other machines.

## References
- `hardware/firmware/docs/AGENT_TODO.md`
- `hardware/firmware/tools/dev/AGENTS.md`

## Plan d’action
1. Vérifier les helpers avec `--help`.
   - run: PATH=$(pwd)/hardware/firmware/.venv/bin:$PATH python3 hardware/firmware/tools/dev/serial_smoke.py --help
   - run: PATH=$(pwd)/hardware/firmware/.venv/bin:$PATH bash hardware/firmware/tools/dev/run_smoke_tests.sh --help
2. Lancer la matrice pour confirmer les logs.
   - run: PATH=$(pwd)/hardware/firmware/.venv/bin:$PATH bash hardware/firmware/tools/dev/run_matrix_and_smoke.sh --help
