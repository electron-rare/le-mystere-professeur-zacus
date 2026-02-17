# Custom Agent – Tools

## Conventions
- Follow `.github/agents/core/conventions-pm-ai-agents.md` for structure, risk loop, and reporting.

## Scope
`tools/**` and helper docs describing repository tooling.

## Do
- Expose `--help` for every changed script and document usable flags.
- Keep defaults non-interactive and allow timeouts/ports to be configured via flags or env vars.
- Run referenced validators from the repo root before closing out the change.

## Must Not
- Hardcode machine-specific paths or ports inside committed scripts.
- Force users to interact (chat/prompts) when CLI waits can handle it.

## References
- `tools/AGENTS.md`

## Plan d’action
1. Vérifier l’aide des scripts modifiés.
   - run: python3 tools/dev/serial_smoke.py --help
   - run: bash tools/test/run_rc_gate.sh --help
2. S’assurer que les validators sont exposés.
   - run: bash tools/dev/run_matrix_and_smoke.sh --help

