## [2026-02-17] Rapport d’erreur automatisé – Story V2

- Correction du bug shell (heredoc Python → script temporaire).
- Échec de la vérification finale : build FAIL, ports USB OK, smoke/UI/artefacts SKIPPED.
- Rapport d’erreur généré : voir artifacts/rapport_erreur_story_v2_20260217.md
- Recommandation : analyser le log de build, corriger, relancer la vérification.

# Agent TODO & governance

## 1. Structural sweep & merge
- [ ] Commit the pending cleanup described in `docs/SPRINT_RECOMMENDATIONS.md:18-80` (structure/tree fixes + PR #86 merge/tag) so the repo is back on main.

## 2. Build/test gates
- [x] Re-run `./build_all.sh` (`build_all.sh:6`); artifacts landed under `artifacts/build/` and logs live in `logs/run_matrix_and_smoke_*.log` if rerun again via the smoke gate.
- [x] Re-launch `./tools/dev/run_matrix_and_smoke.sh` (`tools/dev/run_matrix_and_smoke.sh:9-200`) – run completed 2026-02-16 14:35 (artifact `artifacts/rc_live/20260216-143539/`), smoke scripts and serial monitors succeeded but UI link still reports `connected=0` (no UI handshake). Need to plug in/validate UI firmware before closing gate.
- [x] Capture evidence for HTTP API, WebSocket, and WiFi/Health steps noted as blocked or TODO in `docs/TEST_SCRIPT_COORDINATOR.md:13-20` – `tools/dev/healthcheck_wifi.sh` created `artifacts/rc_live/healthcheck_20260216-154709.log` (ping+HTTP fail) and the HTTP API script logged connection failures under `artifacts/http_ws/20260216-154945/http_api.log` (ESP_URL=127.0.0.1:9). WebSocket skipped (wscat missing). All failures logged to share evidence.

## 3. QA + automation hygiene
- [x] Execute the manual serial smoke path (`python3 tools/dev/serial_smoke.py --role auto --baud 115200 --wait-port 3 --allow-no-hardware`) – passed on /dev/cu.SLAB_USBtoUART + /dev/cu.SLAB_USBtoUART9, reporting UI link still down (same failure as matrix run).
- [ ] Run the story QA suite (`tools/dev/run_smoke_tests.sh`, `python3 tools/dev/run_stress_tests.py ...`, `make fast-*` loops) documented in `esp32_audio/TESTING.md:36-138` and capture logs (smoke_tests failed: DEFAULT scenario missing `/story/scenarios/DEFAULT.json`; run_stress_tests failed with I2S panic during recovery; `make fast-esp32` / `fast-ui-oled` built & flashed but monitor commands quit in non-interactive mode, `fast-ui-tft` not run because no RP2040 board connected). Need scenario files/UI recovery to unblock.
- [x] Ensure any generated artifacts remain untracked per agent contract (no logs/artifacts added to git).

## 4. Documentation & agent contracts
- [ ] Update `AGENTS.md` and `tools/dev/AGENTS.md` whenever scripts/gates change, per their own instructions (`AGENTS.md`, `tools/dev/AGENTS.md`).
- [ ] Keep `tools/dev/cockpit_commands.yaml` in sync with `docs/_generated/COCKPIT_COMMANDS.md` via `python3 tools/dev/gen_cockpit_docs.py` after edits, and confirm the command registry is reflected in `docs/TEST_SCRIPT_COORDINATOR.md` guidance.
- [ ] Review `docs/INDEX.md`, `docs/ARCHITECTURE_UML.md`, and `docs/QUICKSTART.md` after significant changes so the onboarding picture matches the agent constraints.

## 5. Reporting & evidence
- [ ] When publishing smoke/baseline runs, include the required artifacts (`meta.json`, `commands.txt`, `summary.md`, per-step logs) under `artifacts/…` as demanded by `docs/TEST_SCRIPT_COORDINATOR.md:160-199`.
- [ ] Document any pipeline/test regressions in `docs/RC_AUTOFIX_CICD.md` or similar briefing docs and flag them for the Test & Script Coordinator.
