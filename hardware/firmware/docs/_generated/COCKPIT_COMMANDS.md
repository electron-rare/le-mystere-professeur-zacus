# Cockpit Commands

Generated from tools/dev/cockpit_commands.yaml. Do not edit manually.

| ID | Description | Entrypoint | Args | Runbook | Evidence |
| --- | --- | --- | --- | --- | --- |
| wifi-debug | Monitor WiFi debug logs via ESP32 serial (live logs, scan/connect/errors) | tools/dev/cockpit.sh | wifi-debug | docs/TEST_SCRIPT_COORDINATOR.md | artifacts/serial_smoke/<timestamp>/summary.md<br>artifacts/serial_smoke/<timestamp>/meta.json<br>artifacts/serial_smoke/<timestamp>/commands.txt<br>artifacts/serial_smoke/<timestamp>/git.txt |
| rc | Run RC live gate (build matrix + smoke) | tools/dev/cockpit.sh | rc | docs/QUICKSTART.md | artifacts/rc_live/<timestamp>/summary.json<br>artifacts/rc_live/<timestamp>/summary.md<br>artifacts/rc_live/<timestamp>/steps.tsv<br>artifacts/rc_live/<timestamp>/run_matrix_and_smoke.log |
| rc-autofix | Run RC live gate with auto-fix helper | tools/dev/cockpit.sh | rc-autofix | docs/QUICKSTART.md | artifacts/rc_live/<timestamp>/summary.json<br>artifacts/rc_live/<timestamp>/summary.md |
| build | Build all firmware targets | tools/dev/cockpit.sh | build | README.md |  |
| bootstrap | Bootstrap local tooling (.venv, pyserial) | tools/dev/cockpit.sh | bootstrap | docs/QUICKSTART.md |  |
| ports | Watch serial ports list | tools/dev/cockpit.sh | ports | docs/QUICKSTART.md |  |
| latest | Print latest artifact directory | tools/dev/cockpit.sh | latest | docs/QUICKSTART.md |  |
| audit | Run full audit (build + rc + driver/test checks) | tools/dev/cockpit.sh | audit | docs/TEST_SCRIPT_COORDINATOR.md | logs/agent_build.log<br>logs/agent_smoke.log<br>logs/audit_sync_report.md |
| report | Generate sync report | tools/dev/cockpit.sh | report | docs/TEST_SCRIPT_COORDINATOR.md | logs/audit_sync_report.md |
| cleanup | Archive old logs and artifacts | tools/dev/cockpit.sh | cleanup | docs/TEST_SCRIPT_COORDINATOR.md | logs/archive/<timestamp>/<br>artifacts/archive/<timestamp>/ |
| codex-check | Check codex CLI + prompts inventory | tools/dev/cockpit.sh | codex-check | docs/TEST_SCRIPT_COORDINATOR.md |  |
| plan | Execute an agent plan by running its `## Plan d’action` commands | tools/dev/cockpit.sh | plan<br><agent><br>[--dry-run]<br>[--plan-only] | docs/AGENTS_INDEX.md#exécution-planifiée |  |
| drivers | Run driver audit for a specific platform | tools/dev/cockpit.sh | drivers<br><platform> | docs/TEST_SCRIPT_COORDINATOR.md |  |
| test | Run tests audit for a specific platform | tools/dev/cockpit.sh | test<br><platform> | docs/TEST_SCRIPT_COORDINATOR.md |  |
| git | Run git commands via cockpit (status, diff, log, branch, show, add, commit, stash, push) | tools/dev/cockpit.sh | git<br><action><br>[args...] | docs/TEST_SCRIPT_COORDINATOR.md#git-operations-policy |  |
| git-add | Stage files for commit (requires ZACUS_GIT_ALLOW_WRITE=1) | tools/dev/cockpit.sh | git<br>add<br><pathspec> | docs/TEST_SCRIPT_COORDINATOR.md#git-operations-policy |  |
| git-commit | Commit staged changes (requires ZACUS_GIT_ALLOW_WRITE=1) | tools/dev/cockpit.sh | git<br>commit<br>-m<br><message> | docs/TEST_SCRIPT_COORDINATOR.md#git-operations-policy |  |
| git-stash | Stash working tree changes (requires ZACUS_GIT_ALLOW_WRITE=1) | tools/dev/cockpit.sh | git<br>stash<br>[save|pop|list] | docs/TEST_SCRIPT_COORDINATOR.md#git-operations-policy |  |
| git-push | Push commits to remote (requires ZACUS_GIT_ALLOW_WRITE=1) | tools/dev/cockpit.sh | git<br>push<br>[remote]<br>[branch] | docs/TEST_SCRIPT_COORDINATOR.md#git-operations-policy |  |
| baseline | Generate Phase 1 firmware health baseline (3 builds, 5 flash tests, 10 smoke runs) | tools/dev/generate_baseline.sh |  | docs/FIRMWARE_HEALTH_BASELINE.md | artifacts/baseline_YYYYMMDD_###/1_build/build_*.log<br>artifacts/baseline_YYYYMMDD_###/2_flash_tests/flash_*.log<br>artifacts/baseline_YYYYMMDD_###/3_smoke_001-010/smoke_**/<br>artifacts/baseline_YYYYMMDD_###/4_healthcheck/health_snapshot_*.txt<br>logs/generate_baseline_YYYYMMDD-HHMMSS.log |
| flash | Flash firmware (auto port resolution with ESP32/ESP8266/RP2040 support) | tools/dev/cockpit.sh | flash | docs/FLASH_GATE_VALIDATION_GUIDE.md | artifacts/rc_live/flash-<timestamp>/ports_resolve.json<br>logs/flash_<timestamp>.log |
| help | Print cockpit help | tools/dev/cockpit.sh | help | docs/QUICKSTART.md |  |
