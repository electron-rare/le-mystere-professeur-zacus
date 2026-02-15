# Name
tooling

## When To Use
Use for bash/python utility scripts, CLI design, and port-resolution automation.

## Trigger Phrases
- "dev script"
- "CLI helper"
- "port resolver"
- "automation wrapper"

## Do
- Expose `--help` and stable flags.
- Keep non-interactive defaults and explicit timeout controls.
- Emit concise step/status markers and actionable failure output.

## Don't
- Do not hardcode machine-specific port names.
- Do not rely on chat interaction when script waiting can be local.

## Quick Commands
- `python3 hardware/firmware/tools/dev/serial_smoke.py --help`
- `bash hardware/firmware/tools/dev/run_matrix_and_smoke.sh`
- `rg -n "argparse|--help|Usage" hardware/firmware/tools/dev`
