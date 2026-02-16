# Codex Script Expert Agent

## Role Brief

**Your mission:** Own Codex prompt management and scripting around RC live gate. Reduce credit usage by automating prompt selection, evidence capture, and safe git operations through cockpit scripts.

**Duration:** 30 days (initial)

**Scope:** `hardware/firmware/**` (all file, tools/dev + tools/test + codex prompts)

---

## Objectives

1. **Codex prompt hygiene**
   - Keep `tools/dev/codex_prompts/` organized and minimal.
   - Ensure prompt usage is deterministic (no manual copy/paste in RC live gate).

2. **RC live gate integration**
   - Ensure `./tools/dev/cockpit.sh rc` uses codex prompts consistently on failure.
   - Minimize prompt length and token usage while preserving signal.

3. **Scripted git operations**
   - Ensure git actions for RC live are performed via cockpit scripts.
   - Wire evidence logging for git commands (commands.txt).

4. **Evidence compliance**
   - Ensure RC live failures generate artifacts and prompt context in logs.

---

## Acceptance Criteria

- RC live gate uses scripted Codex prompts only (no manual steps).
- Prompt size reduced where possible (no redundant text, remove duplication).
- All Codex prompt runs record evidence under `artifacts/rc_live/<timestamp>/`.
- Git commands used by RC live are executed via cockpit and logged in evidence.
- A minimal runbook exists for prompt update + verification.

---

## Deliverables

- A short report (`docs/RC_LIVE_CODEX_STATUS.md`) with:
  - Prompt inventory
  - RC live usage flow
  - Known gaps + fixes
- Updated codex prompts (if needed) under `tools/dev/codex_prompts/`.
- Script changes in `tools/dev/` with evidence logging.

---

## Must Read

1. [tools/dev/cockpit.sh](tools/dev/cockpit.sh)
2. [tools/dev/agent_utils.sh](tools/dev/agent_utils.sh)
3. [docs/RC_FINAL_BOARD.md](docs/RC_FINAL_BOARD.md)
4. [docs/TEST_SCRIPT_COORDINATOR.md](docs/TEST_SCRIPT_COORDINATOR.md)

---

## Weekly Update Format

```
**Codex Script Expert Update**
- ✅ Completed:
- 🔄 In progress:
- ⏸️ Blocked:
- 📋 Next 3 days:
- 🧪 Tests run: (commands + result)
- 📁 Evidence: (artifacts/logs path)
- 📈 Health: (green/yellow/red)
```

---

## Start Here (Day 1)

1. Inventory prompts in `tools/dev/codex_prompts/`.
2. Run `./tools/dev/cockpit.sh rc` once to confirm failure path and prompt usage.
3. Create first status update with evidence path.
