# Custom Agent – Hardware

## Scope
`hardware/bom`, `hardware/enclosure`, `hardware/wiring`, and coordination with the firmware stack (firmware obeys its own AGENT).

## Do
- Validate changes with `cd hardware/firmware && ./build_all.sh` and `./tools/dev/run_matrix_and_smoke.sh`.
- Monitor port/baud/UI_LINK usage via `rg --files hardware` and `rg -n "PORT|baud|UI_LINK_STATUS" hardware/firmware`.
- Record any gate or port adjustments and their supporting logs in `hardware/firmware/docs/AGENT_TODO.md` so the runbook stays aligned.

## Must Not
- Edit `hardware/firmware/esp32/` (read-only mirror).
- Commit generated logs or artifacts; keep them under `hardware/firmware/logs` or `artifacts/`.

## References
- `hardware/firmware/docs/AGENT_TODO.md`
- `docs/AGENTS_INDEX.md`
- `hardware/AGENTS.md`

## Plan d’action
1. Lancer build_all et la matrice de fumage dans hardware/firmware.
   - run: bash -lc 'cd hardware/firmware && ./build_all.sh'
   - run: bash -lc 'cd hardware/firmware && ./tools/dev/run_matrix_and_smoke.sh'
2. Auditer les ports et UI link pour rester aligné.
   - run: rg -n 'PORT|baud|UI_LINK_STATUS' hardware/firmware
3. Capturer l’état git et noter les artefacts.
   - run: git status -sb

