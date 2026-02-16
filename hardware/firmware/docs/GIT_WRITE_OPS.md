# Git Write Operations via Cockpit

## Overview

Save **token credits** by executing git write operations through shell scripts instead of agents.

- **Read** operations (status, diff, log, branch, show): No restrictions
- **Write** operations (add, commit, stash, push): Require `ZACUS_GIT_ALLOW_WRITE=1` + safeguards

All git commands are **logged in evidence** (`commands.txt`) automatically.

---

## Quick Start

### 1. Enable git write operations
```bash
export ZACUS_GIT_ALLOW_WRITE=1
```

### 2. Run git commands via cockpit
```bash
# Read operations (no safeguards)
./tools/dev/cockpit.sh git status
./tools/dev/cockpit.sh git log 10
./tools/dev/cockpit.sh git diff
./tools/dev/cockpit.sh git branch
./tools/dev/cockpit.sh git show HEAD

# Write operations (with confirmation prompt by default)
./tools/dev/cockpit.sh git add src/
./tools/dev/cockpit.sh git commit -m "Update firmware"
./tools/dev/cockpit.sh git stash save "temp work"
./tools/dev/cockpit.sh git push origin main
```

### 3. For CI/CD: suppress confirmation
```bash
export ZACUS_GIT_ALLOW_WRITE=1
export ZACUS_GIT_NO_CONFIRM=1
./tools/dev/cockpit.sh git commit -m "Automated commit"
```

---

## Implementation Details

### Architecture

```
cockpit.sh (CLI dispatcher)
    ↓
run_git() function (handles all git actions)
    ↓
git_write_check() safeguard (requires ZACUS_GIT_ALLOW_WRITE=1)
    ↓
git_add(), git_commit(), git_stash(), git_push() (in agent_utils.sh)
    ↓
git_cmd() (records command in evidence, executes git)
```

### Files Modified

1. **tools/dev/agent_utils.sh**
   - Added `git_write_check()` - validates safeguards
   - Added `git_add()`, `git_commit()`, `git_stash()`, `git_push()` - write operations

2. **tools/dev/cockpit.sh**
   - Extended `run_git()` to support write actions (add, commit, stash, push)

3. **tools/dev/cockpit_commands.yaml**
   - Added entries for git-add, git-commit, git-stash, git-push
   - Updated main git entry to list all supported actions

4. **docs/_generated/COCKPIT_COMMANDS.md**
   - Generated from cockpit_commands.yaml
   - Shows all cockpit subcommands with args and runbook references

5. **docs/TEST_SCRIPT_COORDINATOR.md**
   - Documented Git Operations Policy section
   - Listed read vs. write operations
   - Provided usage examples

### Environment Variables

| Variable | Value | Purpose |
|----------|-------|---------|
| `ZACUS_GIT_ALLOW_WRITE` | `1` | **REQUIRED** for all write operations (add, commit, stash, push) |
| `ZACUS_GIT_NO_CONFIRM` | `1` | Skip confirmation prompt (for CI/CD automation) |

---

## Usage Patterns

### Pattern 1: Interactive git operations (CLI)
```bash
export ZACUS_GIT_ALLOW_WRITE=1
./tools/dev/cockpit.sh git add .
# User sees: "[WARN] git add . \nContinue? (y/N):"
./tools/dev/cockpit.sh git commit -m "my changes"
# User sees: "[WARN] git commit -m my changes\nContinue? (y/N):"
```

### Pattern 2: Automated CI/CD pipeline
```bash
#!/bin/bash
set -euo pipefail

export ZACUS_GIT_ALLOW_WRITE=1
export ZACUS_GIT_NO_CONFIRM=1

cd hardware/firmware
./tools/dev/cockpit.sh git add docs/
./tools/dev/cockpit.sh git commit -m "Auto-update: firmware docs"
./tools/dev/cockpit.sh git push origin feature-branch
```

### Pattern 3: Scripted test gate with evidence
```bash
#!/bin/bash
source tools/dev/agent_utils.sh

# Initialize evidence tracking
evidence_init "release_gate" "artifacts/release/$(date +%Y%m%d-%H%M%S)"

export ZACUS_GIT_ALLOW_WRITE=1
export ZACUS_GIT_NO_CONFIRM=1

# Commands are auto-logged to EVIDENCE_COMMANDS
git_add "hardware/firmware/src/"
git_commit -m "Release v1.2.3"
git_push origin main

# Verify evidence
cat "$EVIDENCE_COMMANDS"
# Output:
# # Commands
# git add hardware/firmware/src/
# git commit -m Release v1.2.3
# git push origin main
```

---

## Safeguards & Error Handling

### Missing ZACUS_GIT_ALLOW_WRITE
```bash
$ ./tools/dev/cockpit.sh git add src/
[AGENT][FAIL] Git write operation requires ZACUS_GIT_ALLOW_WRITE=1
exit 1
```

### Confirmation Flow (with safeguards enabled)
```bash
$ export ZACUS_GIT_ALLOW_WRITE=1
$ ./tools/dev/cockpit.sh git add .
[WARN] git add . 
Continue? (y/N): n
[AGENT][FAIL] Cancelled by user
exit 1
```

### Confirmation Bypassed
```bash
$ export ZACUS_GIT_ALLOW_WRITE=1
$ export ZACUS_GIT_NO_CONFIRM=1
$ ./tools/dev/cockpit.sh git add .
  # Command executes without prompting
```

---

## Evidence Integration

All git commands are automatically logged in evidence when evidence tracking is enabled:

```bash
source tools/dev/agent_utils.sh
evidence_init "my_phase"

# Now all git_* calls log their commands
git_add src/
git_commit -m "changes"

# Verify logged commands
cat "$EVIDENCE_COMMANDS"
```

Output file: `artifacts/my_phase/<timestamp>/commands.txt`
```
# Commands
git add src/
git commit -m changes
```

---

## Cockpit Command Registry

### New Commands

| ID | Description | Args | Requires |
|----|-------------|------|----------|
| `git-add` | Stage files | `<pathspec>` | `ZACUS_GIT_ALLOW_WRITE=1` |
| `git-commit` | Commit staged | `-m "<message>"` | `ZACUS_GIT_ALLOW_WRITE=1` |
| `git-stash` | Stash changes | `[save\|pop\|list]` | `ZACUS_GIT_ALLOW_WRITE=1` |
| `git-push` | Push to remote | `[remote] [branch]` | `ZACUS_GIT_ALLOW_WRITE=1` |

All entries in [docs/_generated/COCKPIT_COMMANDS.md](docs/_generated/COCKPIT_COMMANDS.md)

---

## Use Cases

### 1. Automated Documentation Updates
```bash
# Update docs, stage, commit, push (no agent needed)
./tools/dev/cockpit.sh git add docs/
ZACUS_GIT_ALLOW_WRITE=1 ZACUS_GIT_NO_CONFIRM=1 ./tools/dev/cockpit.sh git commit -m "Auto-update docs"
ZACUS_GIT_ALLOW_WRITE=1 ./tools/dev/cockpit.sh git push origin main
```

### 2. Release Pipeline
```bash
# CI/CD gate: tag, commit, push
ZACUS_GIT_ALLOW_WRITE=1 ZACUS_GIT_NO_CONFIRM=1 \
  ./tools/dev/cockpit.sh git commit -m "Bump version to v1.2.3"
```

### 3. Evidence-Tracked Development
```bash
# Include git operations in test evidence
source tools/dev/agent_utils.sh
evidence_init "integration_test"
./tools/dev/cockpit.sh git add tests/
./tools/dev/cockpit.sh git status
# All commands logged to EVIDENCE_COMMANDS
```

---

## Testing

Run examples:
```bash
./tools/dev/examples_git_write_ops.sh 1    # Stage files
./tools/dev/examples_git_write_ops.sh 2    # Commit with prompt
./tools/dev/examples_git_write_ops.sh 3    # Commit silent
./tools/dev/examples_git_write_ops.sh 6    # With evidence
./tools/dev/examples_git_write_ops.sh 7    # Error case
```

---

## Limitations & Design Notes

- **Read-only git actions** (status, diff, log, branch, show) run without `ZACUS_GIT_ALLOW_WRITE`
- **Write actions** require explicit `ZACUS_GIT_ALLOW_WRITE=1` to prevent accidental modifications
- **Confirmation prompts** are always shown unless `ZACUS_GIT_NO_CONFIRM=1` (for safety)
- **Push operations** log to evidence but operate the same as other write ops
- In evidence context, git commands are logged via `evidence_record_command()`
- All git execution is via `git_cmd()` which records commands automatically

---

## Benefits

✅ **Save token credits** - Use shell scripts instead of calling agents
✅ **Evidence integration** - All git operations logged automatically
✅ **Safe by default** - Confirmation prompts prevent accidents
✅ **CI/CD friendly** - Bypass prompts with `ZACUS_GIT_NO_CONFIRM=1`
✅ **Auditable** - Commands recorded in `commands.txt`
✅ **Simple API** - Same cockpit.sh dispatcher for all operations

---

## Troubleshooting

### "Git write operation requires ZACUS_GIT_ALLOW_WRITE=1"
Set: `export ZACUS_GIT_ALLOW_WRITE=1`

### "Cancelled by user"
Confirm the prompt with `y`, or set `ZACUS_GIT_NO_CONFIRM=1` to skip

### Commands not logged in evidence
Verify `EVIDENCE_COMMANDS` env var is set. Usually initialized via `evidence_init()`

### Git command fails with authentication error
Use git credentials or SSH key configured in your shell

---

## See Also

- [TEST_SCRIPT_COORDINATOR.md](TEST_SCRIPT_COORDINATOR.md) - Git Operations Policy section
- [COCKPIT_COMMANDS.md](docs/_generated/COCKPIT_COMMANDS.md) - Full command registry
- [examples_git_write_ops.sh](tools/dev/examples_git_write_ops.sh) - Usage examples
