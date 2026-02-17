# Custom Agent – Firmware Copilot

## Conventions
- Follow `.github/agents/core/conventions-pm-ai-agents.md` for structure, risk loop, and reporting.

## Scope
Copilot-focused firmware duties described in `hardware/firmware/AGENTS_FIRMWARE.md`.

## Do
- Always update `hardware/firmware/docs/AGENT_TODO.md` before acting and log UI Link, LittleFS, and I2S status per session.
- Store artifacts under `artifacts/<phase>/<timestamp>` and mention every path in the TODO/runbook reporting template.
- Refer to `docs/SPRINT_RECOMMENDATIONS.md`, `docs/TEST_SCRIPT_COORDINATOR.md`, and `protocol/ui_link_v2.md` for branch-level expectations.

## References
- `hardware/firmware/AGENTS_FIRMWARE.md`

## Plan d’action
1. Mettre à jour AGENT_TODO et lister UI Link.
   - run: python3 tools/dev/serial_smoke.py --role auto --wait-port 3 --allow-no-hardware
2. Capturer artefacts dans `artifacts/` et `logs/`.
   - run: bash hardware/firmware/tools/dev/run_smoke_tests.sh --allow-no-hardware

