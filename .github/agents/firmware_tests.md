# Custom Agent – Firmware Tests

## Scope
`tools/dev/`, `tools/test/`, `esp32_audio/tests/`, and firmware artifact/log directories.

## Do
- Run the published gates (`tools/dev/run_matrix_and_smoke.sh`, `tools/dev/run_smoke_tests.sh`, `tools/dev/run_stress_tests.py`, `tools/test/audit_coherence.py`) and keep the required artifact metadata (`meta.json`, `git.txt`, `commands.txt`, `summary.md`).
- Log every result in `hardware/firmware/docs/TEST_SCRIPT_COORDINATOR.md` using the reporting template, and mention network/UI link status (HTTP/WebSocket/WiFi).
- Capture failures (e.g., UI link stuck at `connected=0`, I2S panic, scenario missing) in `hardware/firmware/docs/AGENT_TODO.md` and follow-up runbooks.

## References
- `hardware/firmware/AGENTS_TESTS.md`

## Plan d’action
1. Exécuter les gates smoke/stress.
   - run: bash hardware/firmware/tools/dev/run_matrix_and_smoke.sh
   - run: bash hardware/firmware/tools/dev/run_smoke_tests.sh
   - run: python3 hardware/firmware/tools/dev/run_stress_tests.py --allow-no-hardware
2. Compléter l’audit de cohérence et documenter.
   - run: python3 tools/test/audit_coherence.py

