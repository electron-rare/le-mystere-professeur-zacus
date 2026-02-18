# Agent Briefings for le-mystere-professeur-zacus

Agent rules, patterns, and implementation requirements to avoid duplication across docs.

Base standard for structure and reporting is documented in:
- `.github/agents/core/conventions-pm-ai-agents.md`
- `.github/agents/core/plan-template.md`
- `.github/agents/core/copilot-index.md`

---

## Git Write Operations via Cockpit

### Purpose
Save token credits by executing git write operations through shell scripts (cockpit.sh) instead of agents, while maintaining evidence tracking and safeguards.

### Implementation Rules

#### 1. Read-Only Git Commands (No Safeguards)
Available via: `./tools/dev/cockpit.sh git <action> [args]`

List of read actions:
- `status` - Git status
- `diff` - Show differences  
- `log [n]` - Show commit log (n = number of commits, default 20)
- `branch` - List branches with tracking info
- `show <ref>` - Show specific commit/ref

**Evidence**: All read operations logged via `git_cmd()` in `commands.txt`

#### 2. Write Git Commands (Require Safeguards)
Available via: `./tools/dev/cockpit.sh git <action> [args]`

**SAFETY REQUIREMENT**: All write operations require `ZACUS_GIT_ALLOW_WRITE=1`

List of write actions:
- `add <pathspec>` - Stage files (git add)
- `commit -m "<msg>"` - Commit staged changes
- `stash [save|pop|list]` - Stash working tree changes
- `push [remote] [branch]` - Push commits to remote

**Default Behavior**: Confirmation prompt before executing (unless `ZACUS_GIT_NO_CONFIRM=1`)

**Evidence**: All operations logged via `git_cmd()` → `evidence_record_command()` → `commands.txt`

### Environment Variables

| Variable | Value | Purpose | Required |
|----------|-------|---------|----------|
| `ZACUS_GIT_ALLOW_WRITE` | `1` | Enable git write operations | ✅ Yes (for add/commit/stash/push) |
| `ZACUS_GIT_NO_CONFIRM` | `1` | Skip confirmation prompts | ❌ No (optional, for CI/CD) |

### Architecture

```
cockpit.sh git <action> [args]
    ↓
run_git() function (in cockpit.sh)
    ↓
git_write_check() guard (validates ZACUS_GIT_ALLOW_WRITE=1 + confirmation)
    ↓
git_add() / git_commit() / git_stash() / git_push() (in agent_utils.sh)
    ↓
git_cmd() (in agent_utils.sh): records command, executes git
    ↓
evidence_record_command() (in agent_utils.sh): logs to EVIDENCE_COMMANDS file
```

### Implementation Details

**File**: `tools/dev/agent_utils.sh`

Functions added:
```bash
git_write_check()          # Guard: requires ZACUS_GIT_ALLOW_WRITE=1, prompts for confirmation
git_add(pathspec)          # Stage files
git_commit([args])         # Commit changes
git_stash([action])        # Stash working tree
git_push([args])           # Push to remote
git_cmd(args)              # Execute git + record in evidence (existing, reused)
```

**File**: `tools/dev/cockpit.sh`

Extended `run_git()` function (in run_git section):
```bash
case "$action" in
  # Read actions (existing):
  status|diff|log|branch|show) ... ;;
  # Write actions (NEW):
  add|commit|stash|push) ... ;;
esac
```

**File**: `tools/dev/cockpit_commands.yaml`

Registry entries:
```yaml
- id: git
  description: Run git commands (status, diff, log, branch, show, add, commit, stash, push)
  args: ["git", "<action>", "[args...]"]
  # ... etc
- id: git-add
  description: Stage files for commit (requires ZACUS_GIT_ALLOW_WRITE=1)
  args: ["git", "add", "<pathspec>"]
  # ... etc
- id: git-commit
  description: Commit staged changes (requires ZACUS_GIT_ALLOW_WRITE=1)
  args: ["git", "commit", "-m", "<message>"]
  # ... etc
- id: git-stash
  description: Stash working tree changes (requires ZACUS_GIT_ALLOW_WRITE=1)
  args: ["git", "stash", "[action]"]
  # ... etc
- id: git-push
  description: Push to remote (requires ZACUS_GIT_ALLOW_WRITE=1)
  args: ["git", "push", "[remote] [branch]"]
  # ... etc
```

### Usage Examples

#### Interactive Use (CLI)
```bash
# Enable write mode
export ZACUS_GIT_ALLOW_WRITE=1

# Stage changes (will prompt for confirmation)
./tools/dev/cockpit.sh git add src/

# Commit (will prompt for confirmation)
./tools/dev/cockpit.sh git commit -m "My changes"

# Confirm when prompted
# [WARN] git commit -m "My changes"
# Continue? (y/N): y
```

#### CI/CD Automation (Silent Mode)
```bash
# Enable write mode + skip confirmation
export ZACUS_GIT_ALLOW_WRITE=1
export ZACUS_GIT_NO_CONFIRM=1

./tools/dev/cockpit.sh git add .
./tools/dev/cockpit.sh git commit -m "Automated commit"
./tools/dev/cockpit.sh git push origin main
```

#### In Scripts with Evidence Tracking
```bash
#!/bin/bash
source tools/dev/agent_utils.sh

# Initialize evidence tracking
evidence_init "release_gate" "artifacts/release/$(date +%Y%m%d-%H%M%S)"

export ZACUS_GIT_ALLOW_WRITE=1
export ZACUS_GIT_NO_CONFIRM=1

# Commands are auto-logged to EVIDENCE_COMMANDS
./tools/dev/cockpit.sh git add .
./tools/dev/cockpit.sh git commit -m "Release v1.2.3"
./tools/dev/cockpit.sh git push origin main

# Evidence artifact at: $EVIDENCE_COMMANDS
# Contains:
#   git add .
#   git commit -m Release v1.2.3
#   git push origin main
```

### Safeguards

1. **Explicit opt-in**: `ZACUS_GIT_ALLOW_WRITE=1` required for all write operations
   - Without it: operation fails with clear error message
   
2. **Confirmation prompt**: Shown by default before executing
   - User must confirm with `y` or `Y` to proceed
   - Can be skipped with `ZACUS_GIT_NO_CONFIRM=1` for CI/CD
   
3. **Evidence tracking**: All git operations logged
   - File: `EVIDENCE_COMMANDS` (from `evidence_init()`)
   - Format: plain text log of each command executed
   - Available in evidence artifacts under `artifacts/<phase>/<timestamp>/`

### Error Handling

**Missing `ZACUS_GIT_ALLOW_WRITE=1`**:
```bash
$ ./tools/dev/cockpit.sh git add .
[AGENT][FAIL] Git write operation requires ZACUS_GIT_ALLOW_WRITE=1
exit 1
```

**User cancels confirmation prompt**:
```bash
$ ./tools/dev/cockpit.sh git commit -m "message"
[WARN] git commit -m "message"
Continue? (y/N): n
[AGENT][FAIL] Cancelled by user
exit 1
```

### Token Credit Savings

By using cockpit.sh (shell commands) instead of agents:

| Operation | Agent Calls | Shell Commands | Token Savings |
|-----------|------------|----------------|---------------|
| git add + commit | 4-5 | 2 | ~80-90% |
| Release (tag + push) | 6-8 | 2-3 | ~80-90% |
| Per operation | 50-200 | 5-10 | ~95% |

**Typical workflow**: 200-400+ tokens → 5-20 tokens per session

### Testing & Verification

**Run examples**:
```bash
./tools/dev/examples_git_write_ops.sh 1-8
```

**Generate docs from registry**:
```bash
python3 tools/dev/gen_cockpit_docs.py
```

**Run coherence audit**:
```bash
python3 tools/test/audit_coherence.py
```

### Related Sources

- Implementation: `tools/dev/agent_utils.sh` (git_*() functions)
- Dispatcher: `tools/dev/cockpit.sh` (run_git() function)
- Registry: `tools/dev/cockpit_commands.yaml` (metadata)
- Examples: `tools/dev/examples_git_write_ops.sh`
- Generated Docs: `docs/_generated/COCKPIT_COMMANDS.md`
- User Guide: `docs/GIT_WRITE_OPS.md`

---
## Custom Agent Files
- `.github/agents/core/plan-template.md` – modèle/guide pour les plans d’action dans chaque briefing.

- `.github/agents/domains/global.md` – repo-wide governance, checkpoints, and gate list.
- `.github/agents/domains/hardware.md` – enclosure/BOM/wiring plus firmware integration expectations.
- `.github/agents/domains/audio.md` – audio manifest ownership and validation workflow.
- `.github/agents/domains/game.md` – scenario/story content guardrails.
- `.github/agents/domains/printables.md` – printable manifests and export discipline.
- `.github/agents/domains/tools.md` – CLI/tooling helper instructions.
- `.github/agents/domains/docs.md` – documentation updates and link checks.
- `.github/agents/domains/kit.md` – GM kit station/export consistency.
- `.github/agents/domains/ci.md` – workflow/template maintenance.
- `.github/agents/domains/firmware-core.md` – firmware tree policies and AGENT_TODO reporting.
- `.github/agents/domains/firmware-tooling.md` – firmware tooling helper scripts.
- `.github/agents/domains/firmware-copilot.md` – Copilot-specific firmware duties around UI Link/LittleFS/I2S/artifacts.
- `.github/agents/domains/firmware-tests.md` – smoke/stress gate runners and artifact metadata.
- `.github/agents/domains/firmware-docs.md` – firmware-facing docs and command indexes.
- `.github/agents/core/alignment-complete.md` – final alignment sweep for AGENT contracts and onboarding before a phase hand-off.
- `.github/agents/core/phase-launch-plan.md` – launch-phase checklist for gates, artifacts, and reporting.

## Assumptions
- Les briefs actifs sont organisés par sous-dossiers (`core/`, `domains/`, `phases/`, `reports/`), avec `archive/` exclu des runs standards.
- En cas de conflit entre une fiche et un contrat AGENT, la hiérarchie root → sous-projet → briefing spécifique sert de guide et doit être documentée dans la fiche ou `agent-briefings.md`.


## Plan execution helper
- Read the relevant agent brief, then run `bash hardware/firmware/tools/dev/plan_runner.sh --agent <name>` to execute the `- run:` commands automatically (use `--dry-run` or `--plan-only` to preview).
- Canonical IDs now follow relative paths such as `domains/firmware-core` or `phases/phase-2b-firmware-rtos`; legacy aliases like `firmware_core` stay accepted.
- Launch `hardware/firmware/tools/dev/codex_prompts/trigger_firmware_core_plan.prompt.md` via Copilot/VS Code to trigger `bash hardware/firmware/tools/dev/plan_runner.sh --agent domains/firmware-core` without leaving the IDE.

## See Also

- [docs/TEST_SCRIPT_COORDINATOR.md](../../../docs/TEST_SCRIPT_COORDINATOR.md) - Main coordinator doc
- [docs/_generated/COCKPIT_COMMANDS.md](../../../docs/_generated/COCKPIT_COMMANDS.md) - Auto-generated command reference
- [.github/agents/core/copilot-index.md](copilot-index.md) - Quick index of the available custom agent files
