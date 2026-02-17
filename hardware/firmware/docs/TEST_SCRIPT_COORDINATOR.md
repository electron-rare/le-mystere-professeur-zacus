# Test & Script Coordinator

## Mission

Own cross-project coherence of tests, scripts, and evidence. Keep cockpit commands, docs, and runbooks aligned, and ensure every gate produces the expected artifacts.

## Definitions (shared terms)

- Cockpit: the single entry point `tools/dev/cockpit.sh` and its CLI subcommands.
- Gate: a scripted validation sequence with a clear PASS/FAIL exit code.
- Runbook: a doc section that lists exact commands and expected evidence paths.
- Evidence: standardized artifacts under `artifacts/<phase>/<timestamp>/` or `logs/`.

## Single Source of Truth

- Command registry: `tools/dev/cockpit_commands.yaml`
- Generated docs: `docs/_generated/COCKPIT_COMMANDS.md` (do not edit manually)

Update flow:

1. Edit `tools/dev/cockpit_commands.yaml`.
2. Regenerate docs: `python3 tools/dev/gen_cockpit_docs.py`.
3. Run coherence audit: `python3 tools/test/audit_coherence.py`.


## Git Operations Policy

See the agent briefing in [.github/agents/AGENT_BRIEFINGS.md](../.github/agents/AGENT_BRIEFINGS.md) to avoid duplicating agent rules in docs.

### Quick Reference

- **Read operations**: `./tools/dev/cockpit.sh git status|diff|log|branch|show [args]` (no safeguards)
- **Write operations**: Require `ZACUS_GIT_ALLOW_WRITE=1` and confirmation (unless `ZACUS_GIT_NO_CONFIRM=1`)
  - `./tools/dev/cockpit.sh git add <pathspec>`
  - `./tools/dev/cockpit.sh git commit -m "<message>"`
  - `./tools/dev/cockpit.sh git stash [action]`
  - `./tools/dev/cockpit.sh git push [remote] [branch]`
- **Evidence**: All git commands logged to `commands.txt` via `git_cmd()`
- **For details**: See `.github/agents/AGENT_BRIEFINGS.md` (full rules, examples, safeguards)


## Baseline Generation (Phase 1)

The **Phase 1 firmware baseline** is a coordinated test run: **3 builds → 5 flash tests → 10 smoke runs**.

### Command

```bash
./tools/dev/generate_baseline.sh
```

This creates `artifacts/baseline_YYYYMMDD_###/` with:

```
baseline_YYYYMMDD_###/
  ├── 1_build/
  │   ├── build_1.log
  │   ├── build_2.log
  │   └── build_3.log
  ├── 2_flash_tests/
  │   ├── flash_1.log
  │   ├── flash_2.log
  │   ├── flash_3.log
  │   ├── flash_4.log
  │   └── flash_5.log
  ├── 3_smoke_001-010/
  │   ├── smoke_001/  (full artifact directory with run_matrix_and_smoke.log, etc.)
  │   ├── smoke_002/
  │   ├── ...
  │   └── smoke_010/
  └── 4_healthcheck/
      └── health_snapshot_YYYYMMDD-HHMMSS.txt
```

### Metrics Collected

- **Build reproducibility**: 3 consecutive builds of all 5 environments
- **Flash consistency**: 5 auto-port flashes (no interaction)
- **Smoke stability**: 10 RC live gates → panic marker tracking
- **RTOS health**: heap, stack, task count snapshots
- **Evidence completeness**: all required metadata + logs

### Interpretation

See [FIRMWARE_HEALTH_BASELINE.md](./FIRMWARE_HEALTH_BASELINE.md) for:

- Success criteria (pass/fail counts)
- Panic incident analysis
- WiFi resilience data
- RTOS memory health assessment
- Root cause tracing for failures

### Failure Handling

If baseline fails mid-run:

1. **Check logs**: `less logs/generate_baseline_*.log`
2. **Inspect specific phase**: `cat artifacts/baseline_*/1_build/build_1.log`
3. **Retry single phase**:
   - Build: `./tools/dev/cockpit.sh build`
   - Flash: `./tools/dev/cockpit.sh flash`
   - Smoke: Use `cockpit.sh rc` in a loop (see autofix for Codex assistance)

### Expected Duration

- **Build**: 2-3 min per cycle × 3 = 6–9 min
- **Flash**: ~1 min per test × 5 = 5 min
- **Smoke**: ~3–5 min per run × 10 = 30–50 min
- **Total**: 45–65 minutes (includes overhead + log collection)


## Flash Gate Testing (Phase 1 - Task 2)

The **flash gate** validates that `./tools/dev/cockpit.sh flash` reproducibly detects and flashes all 3 device types with 100% reliability.

### Test Scenarios

**3 mandatory tests** (see [FLASH_GATE_VALIDATION_GUIDE.md](./FLASH_GATE_VALIDATION_GUIDE.md) for details):

1. **Auto-detect** — Port resolution without overrides (fastest, most common)
2. **Explicit ports** — Manual ZACUS_PORT_ESP32/ESP8266 env vars (CI/CD, reproducible)
3. **Learned cache** — Using previously discovered mappings (persistent, fast)

**Optional:**
4. **RP2040 detection** — If hardware available (ZACUS_REQUIRE_RP2040=1)

### Expected Outcome

- All 3 flash tests PASS
- Port resolution < 5 seconds each
- Evidence: `artifacts/rc_live/flash-<timestamp>/ports_resolve.json`
- Learned cache: `.local/ports_map.learned.json` (updated after successful auto-detect)

### Deliverable

Phase 1 sign-off requires:
- ✅ All 3 test scenarios validated
- ✅ At least 5 consecutive successful flash cycles
- ✅ Evidence collected in `artifacts/flash_gate_validation_*`
- ✅ Runbook updated (this section)

See [FIRMWARE_EMBEDDED_EXPERT.md](../FIRMWARE_EMBEDDED_EXPERT.md#phase-1-flashgate-100-reproducible) for full Phase 1 acceptance criteria.


## Evidence Standard

All gates and test scripts must accept `--outdir` (or `ZACUS_OUTDIR`) and write:

- `meta.json`
- `git.txt`
- `commands.txt`
- `summary.md`
- per-step logs (one or more log files per step)

Default layout:

- `artifacts/<phase>/<timestamp>/...`

Example:

- `artifacts/rc_live/20260216-031500/summary.md`

## Core Responsibilities

- Keep `tools/dev/` and `tools/test/` scripts aligned with documented gates.
- Validate cockpit commands map 1:1 to runbook steps.
- Review test/script changes for cross-phase impact.
- Maintain reporting templates and evidence requirements.
- Flag regressions, missing evidence, or drift between docs and scripts.

## Intervention Points

- Before a phase starts: confirm gates, scripts, and evidence paths are correct.
- Before handoff: verify docs, scripts, and artifacts match acceptance criteria.
- After any script change: re-check runbooks, RC report, and docs.
- Before PM go/no-go: validate the evidence pack and sign off.

## Required Artifacts

- Evidence paths listed in runbooks (exact file or folder).
- `artifacts/<phase>/<timestamp>/` contains mandatory files.
- Logs saved under `artifacts/` or `logs/`.
- Script version or commit hash when applicable.

## Reporting Update Format

Use this in phase updates:

```
**Update**
- ✅ Completed:
- 🔄 In progress:
- ⏸️ Blocked:
- 📋 Next 3 days:
- 🧪 Tests run: (commands + result)
- 📁 Evidence: (artifacts/logs path)
- 📈 Health: (green/yellow/red)
```

## Exécutions récentes (17 février 2026)

- `bash hardware/firmware/tools/dev/plan_runner.sh --agent firmware_tooling --plan-only` puis `bash hardware/firmware/tools/dev/plan_runner.sh --agent firmware_tooling` — PASS; les trois commandes `--help` sont validées sans déclencher de run matrix involontaire.
- `ZACUS_REQUIRE_HW=1 bash hardware/firmware/tools/dev/plan_runner.sh --agent firmware_tests` — FAIL sur la première gate (`run_matrix_and_smoke`), voir `artifacts/rc_live/20260217-153129/summary.md` (`UI_LINK_STATUS connected=0`, smoke ESP8266 monitor KO).
- `ZACUS_REQUIRE_HW=1 PATH=$(pwd)/hardware/firmware/.venv/bin:$PATH bash hardware/firmware/tools/dev/run_smoke_tests.sh` — FAIL (résolution port stricte), evidence `artifacts/smoke_tests/20260217-153214/summary.md`.
- `PATH=$(pwd)/hardware/firmware/.venv/bin:$PATH python3 hardware/firmware/tools/dev/run_stress_tests.py --hours 0.5` — FAIL, scénario `DEFAULT` non terminé et (sur run précédent) panic I2S observé; evidence `artifacts/stress_test/20260217-153220/summary.md` + `artifacts/stress_test/20260217-153037/stress_test.log`.
- `PATH=$(pwd)/hardware/firmware/.venv/bin:$PATH python3 hardware/firmware/tools/test/audit_coherence.py` — PASS après correction des `runbook_ref` cockpit (`plan`, `baseline`) et régénération `docs/_generated/COCKPIT_COMMANDS.md`; evidence `artifacts/audit/20260217-153246/summary.md`.

## Exécutions récentes (16 février 2026)

- `ZACUS_REQUIRE_HW=1 ./tools/dev/run_matrix_and_smoke.sh` — ports resolved (ESP32 `/dev/cu.SLAB_USBtoUART`, ESP8266 `/dev/cu.SLAB_USBtoUART9`), per-role serial smokes passed, but `UI_LINK_STATUS` remained `connected=0` so the UI never handshook; see `artifacts/rc_live/20260216-143539/summary.md` (and `ui_link.log`) for the run.
- `PATH=$(pwd)/.venv/bin:$PATH ZACUS_REQUIRE_HW=1 ./tools/dev/run_smoke_tests.sh` — recorded under `artifacts/smoke_tests/20260216-144035/` (and sibling `20260216-144039/`); the gate stops with `DEFAULT` scenario missing `/story/scenarios/DEFAULT.json`, so EXPRESS/SPECTRE never start.
- `PATH=$(pwd)/.venv/bin:$PATH python3 tools/dev/run_stress_tests.py --hours 0.02 --port /dev/cu.SLAB_USBtoUART` — see `artifacts/stress_test/20260216-144114/`; the loop crashes on an I2S DMA allocation failure (Guru Meditation) while the U-Son recovery path rewinds, so the planned 4h stress run must wait for heap tuning plus valid scenario assets.
- `PATH=$(pwd)/.venv/bin:$PATH python3 tools/test/audit_coherence.py` — coherence audit passed and materialized logs in `artifacts/audit/20260216-144134/`.
 - `./tools/dev/healthcheck_wifi.sh` — generated `artifacts/rc_live/healthcheck_20260216-154709.log`; ping + `/api/status` calls failed because no AP is reachable, but the log documents the attempts for the coordinator.
 - `ESP_URL=http://127.0.0.1:9 bash esp32_audio/tests/test_story_http_api.sh` — executed the 13 HTTP endpoints with connection refusals (capture in `artifacts/http_ws/20260216-154945/http_api.log`); wscat not installed so WebSocket check skipped. This log shows the inability to reach the ESP API from this environment.

## Audits to Run

1. Docs vs Scripts vs Cockpit (high priority)
2. Evidence & Artifacts consistency (high priority)
3. HTTP/WS test harness coverage (medium priority)
4. RTOS/WiFi health evidence (medium priority)
5. RC report compliance (medium priority)

Canonical command:

- `python3 hardware/firmware/tools/test/audit_coherence.py`

Evidence output:

- `artifacts/audit/<timestamp>/`

## Signoff Checklist

- [ ] Docs and scripts match (no drift)
- [ ] Cockpit commands align with runbook
- [ ] Evidence paths present and valid
- [ ] Required tests executed and logged
- [ ] Reporting template updated if needed
