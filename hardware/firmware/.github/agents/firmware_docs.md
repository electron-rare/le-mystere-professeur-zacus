# Custom Agent – Firmware Docs

## Scope
Firmware-facing documentation, onboarding guides, and generated command indexes.

## Do
- Sync `tools/dev/cockpit_commands.yaml` with `docs/_generated/COCKPIT_COMMANDS.md` via `python3 tools/dev/gen_cockpit_docs.py` when commands change.
- Mention updated gates/tests/artifacts in `hardware/firmware/docs/TEST_SCRIPT_COORDINATOR.md`, `hardware/firmware/docs/TEST_COHERENCE_AUDIT_RUNBOOK.md`, and related runbooks.
- Keep `hardware/firmware/docs/AGENT_TODO.md` updated whenever onboarding structure or tooling workflows change.

## References
- `hardware/firmware/AGENTS_DOCS.md`

## Plan d’action
1. Régénérer l’index cockpit.
   - run: python3 tools/dev/gen_cockpit_docs.py
2. Vérifier les gate/runbook updates.
   - run: rg -n 'TEST_SCRIPT_COORDINATOR.md' hardware/firmware/docs

