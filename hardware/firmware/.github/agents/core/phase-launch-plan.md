# Custom Agent – Phase Launch Plan

## Conventions
- Follow `.github/agents/core/conventions-pm-ai-agents.md` for structure, risk loop, and reporting.

## Scope
Execution plan for gating, artifacts, and reporting required to open a new phase (feature milestone, release candidate, etc.).

## Do
- Define the gate list early: include the PlatformIO matrix, smoke/stress scripts, scenario/audio/printables validators, and any additional QC scripts noted in `docs/TEST_SCRIPT_COORDINATOR.md`.
- Capture every artifact/log path under `artifacts/` and `hardware/firmware/logs/`, writing their metadata (`meta.json`, `commands.txt`, `summary.md`) before closing the phase.
- Mention UI Link, WebSocket, HTTP, and I2S health verdicts in `docs/AGENT_TODO.md` and `docs/TEST_SCRIPT_COORDINATOR.md` along with the `artifacts/<phase>` references.
- Summarize the phase status in `docs/RC_FINAL_REPORT_TEMPLATE.md` (if applicable) and note any blockers in `docs/RC_AUTOFIX_CICD.md`.
- Capture the gate/artifact summary and verification commands in `GIT_WRITE_OPS_FINAL_REPORT.md` or `docs/AGENT_TODO.md` so the phase status is documented.

## Must Not
- Leave gates undocumented or skip the regression checks listed in the root `AGENTS.md` or `hardware/firmware/AGENTS.md`.
- Ship artifacts/logs without logging them in the evidence tracker.

## References
- `docs/TEST_SCRIPT_COORDINATOR.md`
- `docs/AGENT_TODO.md`
- `docs/RC_FINAL_REPORT_TEMPLATE.md`
- `docs/RC_AUTOFIX_CICD.md`

## Plan d’action
1. Définir et exécuter les gates clés.
   - run: bash hardware/firmware/tools/dev/run_matrix_and_smoke.sh
   - run: bash hardware/firmware/tools/dev/run_smoke_tests.sh
   - run: python3 hardware/firmware/tools/dev/run_stress_tests.py --allow-no-hardware
2. Documenter artefacts/logs + verdicts réseau.
   - run: python3 tools/test/audit_coherence.py
   - run: cat GIT_WRITE_OPS_FINAL_REPORT.md

