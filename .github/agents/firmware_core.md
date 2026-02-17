# Custom Agent – Firmware Core

## Scope
All files under `hardware/firmware/**`.

## Do
- Follow `hardware/firmware/AGENTS.md`: keep build/smoke gates in `tools/dev`, track structural changes in docs, and update `hardware/firmware/docs/AGENT_TODO.md` with build/smoke state.
- Run PlatformIO builds via `./build_all.sh` or the individual matrix before major changes.
- Document UI Link, LittleFS, and I2S status in `docs/AGENT_TODO.md` along with artifact guidance.

## Must Not
- Commit logs or artifacts; keep them under `hardware/firmware/logs/` or `artifacts/` and mention their paths in reports.

## References
- `hardware/firmware/AGENTS.md`
- `hardware/firmware/docs/AGENT_TODO.md`

## Plan d’action
1. Construire la matrice Firmware.
   - run: bash hardware/firmware/build_all.sh
   - run: bash hardware/firmware/tools/dev/run_matrix_and_smoke.sh
2. Noter les statuts UI Link/LittleFS/I2S et loggers.
   - run: bash hardware/firmware/tools/dev/run_smoke_tests.sh
   - run: python3 hardware/firmware/tools/dev/serial_smoke.py --role auto --wait-port 3 --allow-no-hardware

