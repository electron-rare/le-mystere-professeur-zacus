# Tools Agent Contract

Purpose: keep repo tooling reproducible, scriptable, and low-noise.

Allowed scope:
- `tools/**`
- helper docs that describe tool usage

Validate:
- run each changed script with `--help`
- run referenced validators from repo root

Common commands:
- `rg --files tools`
- `rg -n "argparse|Usage:|--help" tools`
- `bash tools/test/run_rc_gate.sh --help`

Do not:
- hardcode machine-specific paths or ports in committed scripts
- require chat interaction when a local prompt/script wait can handle it
