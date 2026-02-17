# Custom Agent – Alignment Complete

## Scope
Final alignment tasks that ensure every agent contract, runbook, and onboarding doc stays in sync before a major phase or hand-off.

## Do
- Review `docs/AGENT_TODO.md`, `docs/TEST_SCRIPT_COORDINATOR.md`, and `docs/AGENTS_INDEX.md` to confirm the current state of gates, artifacts, and command registries.
- Ensure every folder-specific `AGENTS*.md` entry (global, firmware, tools, docs, etc.) matches the latest instructions in `.github/agents` briefs; note mismatches in the release log and update the relevant doc.
- Check that onboarding materials (`docs/QUICKSTART.md`, `docs/_generated/COCKPIT_COMMANDS.md`) reflect the expected workflows referenced by the new gate or automation plan.
- Record each alignment review, gate status and artifact path in `GIT_WRITE_OPS_FINAL_REPORT.md` or `docs/AGENT_TODO.md` for traceability.

## Must Not
- Deliver feature changes in this pass; the goal is coherence and evidence before launch.
- Skip the safety checkpoint or artifact-tracking mandate from `AGENTS.md`.

## References
- `docs/AGENT_TODO.md`
- `docs/TEST_SCRIPT_COORDINATOR.md`
- `docs/AGENTS_INDEX.md`
- `.github/agents/*.md`

## Plan d’action
1. Revoir les TODO/runbooks centrés sur les gates.
   - run: python3 tools/dev/gen_cockpit_docs.py
   - run: git status -sb
2. Confirmer la cohérence des AGENT contracts et docs.
   - run: rg -n 'AGENT' docs/AGENT_TODO.md
3. Capturer les artefacts/étapes dans les rapports.
   - run: cat GIT_WRITE_OPS_FINAL_REPORT.md

