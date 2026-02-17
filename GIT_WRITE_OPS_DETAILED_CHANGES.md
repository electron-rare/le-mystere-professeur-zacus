# Git Write Operations - Detailed Changes

## Summary of Changes

### 1. agent_utils.sh

**Addition**: Git write operation safeguards and handlers

```bash
# New functions added:

git_write_check() {
  if [[ "${ZACUS_GIT_ALLOW_WRITE:-0}" != "1" ]]; then
    fail "Git write operation requires ZACUS_GIT_ALLOW_WRITE=1"
  fi
  if [[ "${ZACUS_GIT_NO_CONFIRM:-0}" != "1" ]]; then
    local prompt="$1"
    echo "[WARN] $prompt" >&2
    echo -n "Continue? (y/N): " >&2
    local answer
    read -r answer
    if [[ "$answer" != "y" && "$answer" != "Y" ]]; then
      fail "Cancelled by user"
    fi
  fi
}

git_add() {
  git_write_check "git add $*"
  git_cmd add "$@"
}

git_commit() {
  git_write_check "git commit $*"
  git_cmd commit "$@"
}

git_stash() {
  git_write_check "git stash $*"
  git_cmd stash "$@"
}

git_push() {
  git_write_check "git push $*"
  git_cmd push "$@"
}
```

---

### 2. cockpit.sh

**Modification**: Extended run_git() to support write actions

```bash
# Before (read-only actions):
run_git() {
  local action="${1:-status}"
  shift || true
  case "$action" in
    status) git_cmd status "$@" ;;
    diff) git_cmd diff "$@" ;;
    log) ... ;;
    branch) git_cmd branch -vv ;;
    show) git_cmd show "$@" ;;
    *) fail "Unknown git action: $action" ;;
  esac
}

# After (read + write actions):
run_git() {
  local action="${1:-status}"
  shift || true
  case "$action" in
    # Read actions (existing):
    status) git_cmd status "$@" ;;
    diff) git_cmd diff "$@" ;;
    log) ... ;;
    branch) git_cmd branch -vv ;;
    show) git_cmd show "$@" ;;
    # Write actions (NEW):
    add) git_add "$@" ;;
    commit) git_commit "$@" ;;
    stash) git_stash "$@" ;;
    push) git_push "$@" ;;
    *) fail "Unknown git action: $action. Use: status, diff, log, branch, show, add, commit, stash, push" ;;
  esac
}
```

---

### 3. cockpit_commands.yaml

**Modification**: Updated main git entry + added 4 new entries

```yaml
# Updated (main git entry):
- id: git
  description: Run git commands via cockpit (status, diff, log, branch, show, add, commit, stash, push)
  entrypoint: tools/dev/cockpit.sh
  args: ["git", "<action>", "[args...]"]
  runbook_ref: docs/TEST_SCRIPT_COORDINATOR.md#git-operations-policy
  evidence_outputs: []
  notes: |
    Read actions: status, diff, log, branch, show
    Write actions (require ZACUS_GIT_ALLOW_WRITE=1): add, commit, stash, push
    All git commands are logged in evidence/commands.txt

# Added (new entries):
- id: git-add
  description: Stage files for commit (requires ZACUS_GIT_ALLOW_WRITE=1)
  entrypoint: tools/dev/cockpit.sh
  args: ["git", "add", "<pathspec>"]
  runbook_ref: docs/TEST_SCRIPT_COORDINATOR.md#git-operations-policy
  evidence_outputs: []
  notes: "Set ZACUS_GIT_NO_CONFIRM=1 to skip confirmation prompt"

- id: git-commit
  description: Commit staged changes (requires ZACUS_GIT_ALLOW_WRITE=1)
  entrypoint: tools/dev/cockpit.sh
  args: ["git", "commit", "-m", "<message>"]
  runbook_ref: docs/TEST_SCRIPT_COORDINATOR.md#git-operations-policy
  evidence_outputs: []
  notes: "Set ZACUS_GIT_NO_CONFIRM=1 to skip confirmation prompt"

- id: git-stash
  description: Stash working tree changes (requires ZACUS_GIT_ALLOW_WRITE=1)
  entrypoint: tools/dev/cockpit.sh
  args: ["git", "stash", "[save|pop|list]"]
  runbook_ref: docs/TEST_SCRIPT_COORDINATOR.md#git-operations-policy
  evidence_outputs: []
  notes: "Set ZACUS_GIT_NO_CONFIRM=1 to skip confirmation prompt"

- id: git-push
  description: Push commits to remote (requires ZACUS_GIT_ALLOW_WRITE=1)
  entrypoint: tools/dev/cockpit.sh
  args: ["git", "push", "[remote]", "[branch]"]
  runbook_ref: docs/TEST_SCRIPT_COORDINATOR.md#git-operations-policy
  evidence_outputs: []
  notes: "Set ZACUS_GIT_NO_CONFIRM=1 to skip confirmation prompt. Dangerous operation!"
```

---

### 4. docs/TEST_SCRIPT_COORDINATOR.md

**Modification**: Added "Git Operations Policy" section

```markdown
# Before:
--- (section did not exist) ---

# After:
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
```

---

### 5. tools/test/audit_coherence.py

**Modification**: Fixed runbook validation to handle markdown anchors

```python
# Before:
if runbook_ref:
    runbook_path = FW_ROOT / runbook_ref  # Fails on "file.md#anchor"
    if not runbook_path.exists():
        issues.append(f"Runbook missing for {cmd_id}: {runbook_ref}")

# After:
if runbook_ref:
    # Extract file path (before any anchor "#")
    runbook_file = runbook_ref.split("#")[0]
    runbook_path = FW_ROOT / runbook_file
    if not runbook_path.exists():
        issues.append(f"Runbook missing for {cmd_id}: {runbook_ref}")
```

This allows references like: `docs/TEST_SCRIPT_COORDINATOR.md#git-operations-policy`

---

### 6. .github/agents/AGENT_BRIEFINGS.md

**Creation**: New file with comprehensive git rules

```markdown
# Agent Briefings for le-mystere-professeur-zacus

## Git Write Operations via Cockpit

### Purpose
Save token credits by executing git write operations through shell scripts instead of agents...

[Full content: 200+ lines covering implementation, safeguards, examples]

```

**Key sections**:
- Purpose and implementation rules
- Read-only vs write command lists
- Safeguard requirements (ZACUS_GIT_ALLOW_WRITE=1)
- Environment variables reference
- Architecture diagram
- Usage examples (interactive, CI/CD, evidence)
- Error handling
- Token credit savings

---

### 7. docs/_generated/COCKPIT_COMMANDS.md

**Generation**: Auto-generated from cockpit_commands.yaml

```markdown
# Cockpit Commands

Generated from tools/dev/cockpit_commands.yaml. Do not edit manually.

| ID | Description | Entrypoint | Args | Runbook | Evidence |
| --- | --- | --- | --- | --- | --- |
| rc | Run RC live gate... | tools/dev/cockpit.sh | rc | ... |
| ... | ... | ... | ... | ... | ... |
| git | Run git commands (status, diff, log, branch, show, add, commit, stash, push) | tools/dev/cockpit.sh | git<br><action><br>[args...] | docs/TEST_SCRIPT_COORDINATOR.md#git-operations-policy |  |
| git-add | Stage files for commit... | tools/dev/cockpit.sh | git<br>add<br><pathspec> | ... |  |
| git-commit | Commit staged changes... | tools/dev/cockpit.sh | git<br>commit<br>-m<br><message> | ... |  |
| git-stash | Stash working tree changes... | tools/dev/cockpit.sh | git<br>stash<br>[action] | ... |  |
| git-push | Push commits to remote... | tools/dev/cockpit.sh | git<br>push<br>[remote]<br>[branch] | ... |  |
| ... | ... | ... | ... | ... | ... |
```

Total: 18 cockpit commands (5 git-related)

---

## File Modification Statistics

| File | Type | Lines Added | Lines Removed | Net Change |
|------|------|-------------|---|---|
| agent_utils.sh | Modified | 65 | 0 | +65 |
| cockpit.sh | Modified | 8 | 2 | +6 |
| cockpit_commands.yaml | Modified | 45 | 3 | +42 |
| TEST_SCRIPT_COORDINATOR.md | Modified | 15 | 2 | +13 |
| audit_coherence.py | Modified | 3 | 1 | +2 |
| AGENT_BRIEFINGS.md | Created | 220 | - | +220 |
| COCKPIT_COMMANDS.md | Generated | 25 | - | +25 |
| **TOTALS** | | **381** | **8** | **+373** |

---

## Verification Steps Executed

1. ✅ Extended run_git() in cockpit.sh
2. ✅ Added git_*() functions in agent_utils.sh
3. ✅ Updated cockpit_commands.yaml with new entries
4. ✅ Regenerated docs: `python3 tools/dev/gen_cockpit_docs.py`
5. ✅ Fixed audit script: anchor handling in runbook_ref
6. ✅ Ran coherence audit: `python3 tools/test/audit_coherence.py`
7. ✅ Audit result: **PASS** - all aligned

---

## Environment Variables

| Variable | Type | Default | Purpose |
|----------|------|---------|---------|
| ZACUS_GIT_ALLOW_WRITE | Required for writes | unset | Enable git write operations (add/commit/stash/push) |
| ZACUS_GIT_NO_CONFIRM | Optional for writes | unset | Skip confirmation prompts (for CI/CD automation) |

---

## References

- **Agent Briefing**: `.github/agents/AGENT_BRIEFINGS.md`
- **Policy Section**: `docs/TEST_SCRIPT_COORDINATOR.md#git-operations-policy`
- **Auto-generated Docs**: `docs/_generated/COCKPIT_COMMANDS.md`
- **Registry**: `tools/dev/cockpit_commands.yaml`

