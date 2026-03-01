# Quick Reference: Git Write Ops via Cockpit

## 🎯 One-Liner Commands

### Basic Operations

```bash
# Enable write mode
export ZACUS_GIT_ALLOW_WRITE=1

# Stage all changes
./tools/dev/cockpit.sh git add .

# Commit with message
./tools/dev/cockpit.sh git commit -m "Your commit message"

# Stash changes
./tools/dev/cockpit.sh git stash save "description"

# Push to remote
./tools/dev/cockpit.sh git push origin main
```

### CI/CD Automation (no prompts)

```bash
export ZACUS_GIT_ALLOW_WRITE=1
export ZACUS_GIT_NO_CONFIRM=1

./tools/dev/cockpit.sh git add .
./tools/dev/cockpit.sh git commit -m "Auto-commit"
./tools/dev/cockpit.sh git push origin main
```

### With Evidence Tracking

```bash
source tools/dev/agent_utils.sh
evidence_init "my_phase"

export ZACUS_GIT_ALLOW_WRITE=1
export ZACUS_GIT_NO_CONFIRM=1

git_add .
git_commit -m "changes"

# All logged to: $EVIDENCE_COMMANDS
```

## 📖 Read Operations (no guards needed)

```bash
./tools/dev/cockpit.sh git status
./tools/dev/cockpit.sh git diff
./tools/dev/cockpit.sh git log 20
./tools/dev/cockpit.sh git branch
./tools/dev/cockpit.sh git show HEAD
```

## 🔒 Safeguard Errors

```bash
# Error: Missing ZACUS_GIT_ALLOW_WRITE
$ ./tools/dev/cockpit.sh git add .
[AGENT][FAIL] Git write operation requires ZACUS_GIT_ALLOW_WRITE=1

# Solution:
export ZACUS_GIT_ALLOW_WRITE=1
./tools/dev/cockpit.sh git add .
```

## 🧪 Test Examples

```bash
./tools/dev/examples_git_write_ops.sh 1  # Stage files
./tools/dev/examples_git_write_ops.sh 2  # Commit interactive
./tools/dev/examples_git_write_ops.sh 3  # Commit silent
./tools/dev/examples_git_write_ops.sh 4  # Stash
./tools/dev/examples_git_write_ops.sh 5  # Push
./tools/dev/examples_git_write_ops.sh 6  # With evidence
./tools/dev/examples_git_write_ops.sh 7  # Error demo
./tools/dev/examples_git_write_ops.sh 8  # Release pattern
```

## 📚 Full Documentation

- **Main Guide**: `docs/GIT_WRITE_OPS.md`
- **Policy**: `docs/TEST_SCRIPT_COORDINATOR.md#git-operations-policy`
- **Registry**: `docs/_generated/COCKPIT_COMMANDS.md`
- **Implementation**: `GIT_WRITE_OPS_IMPLEMENTATION.md`

## 🚀 Key Points

1. **Always set**: `export ZACUS_GIT_ALLOW_WRITE=1` for write ops
2. **Get prompted**: By default (unless `ZACUS_GIT_NO_CONFIRM=1`)
3. **Check status**: `./tools/dev/cockpit.sh git status`
4. **In scripts**: Use `git_add()`, `git_commit()`, etc. from `agent_utils.sh`
5. **Evidence**: All commands auto-logged when evidence is initialized

## 💡 Pro Tips

- Use `git status` before committing to verify changes
- Test with `ZACUS_GIT_NO_CONFIRM=1` in isolated branch first
- Review `docs/GIT_WRITE_OPS.md` for full examples
- Run `examples_git_write_ops.sh` to see all patterns
- Check `EVIDENCE_COMMANDS` file after operations

## 🔗 Related Commands

```bash
# Other cockpit commands
./tools/dev/cockpit.sh rc         # RC live gate
./tools/dev/cockpit.sh flash      # Flash firmware
./tools/dev/cockpit.sh build      # Build all
./tools/dev/cockpit.sh ports      # Watch ports

# Git read operations
./tools/dev/cockpit.sh git log 10
./tools/dev/cockpit.sh git diff HEAD~1
./tools/dev/cockpit.sh git branch -vv
```
