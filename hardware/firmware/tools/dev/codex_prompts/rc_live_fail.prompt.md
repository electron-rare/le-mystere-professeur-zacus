# RC live failure triage

Goal: help the developer recover from an `run_matrix_and_smoke.sh` failure.

Inputs:
- `ARTIFACT_PATH`: path to the most recent `artifacts/rc_live/<timestamp>/` produced by the script (if available). Use files inside that directory to summarize the failure.

Suggested actions:
1. Open the `run_matrix_and_smoke.log` and any `smoke_*.log` in `ARTIFACT_PATH` to extract the failing step and exit code.
2. If port resolution failed, reason about `resolve_ports.log` + `ports_resolve.json`.
3. Emit a concise recovery plan (hardware check, USB cables, drivers, etc.), referencing the exact artifacts involved.
