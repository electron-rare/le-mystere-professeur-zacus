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

## Audits to Run

1. Docs vs Scripts vs Cockpit (high priority)
2. Evidence & Artifacts consistency (high priority)
3. HTTP/WS test harness coverage (medium priority)
4. RTOS/WiFi health evidence (medium priority)
5. RC report compliance (medium priority)

Canonical command:

- `python3 tools/test/audit_coherence.py`

Evidence output:

- `artifacts/audit/<timestamp>/`

## Signoff Checklist

- [ ] Docs and scripts match (no drift)
- [ ] Cockpit commands align with runbook
- [ ] Evidence paths present and valid
- [ ] Required tests executed and logged
- [ ] Reporting template updated if needed
