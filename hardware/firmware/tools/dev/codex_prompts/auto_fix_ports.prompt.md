ROLE: Firmware PM + QA gatekeeper. 2 messages max (plan+final).
Goal: Make port resolution fully automatic on macOS even when LOCATION strings change/truncate.
Do:
- Improve tools/test/resolve_ports.py: prefix match, learned-map (.local), fingerprint (2s @115200 then 19200), env overrides.
- Ensure run_matrix_and_smoke.sh loops until both roles found when ZACUS_REQUIRE_HW=1 (no Enter).
Gates: ZACUS_REQUIRE_HW=1 ./tools/dev/run_matrix_and_smoke.sh --skip-build --skip-upload
