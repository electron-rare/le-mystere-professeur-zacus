# Codex Prompt Maintenance Runbook

**Version**: 1.0  
**Last Updated**: 2026-02-16  
**Audience**: Firmware developers, Codex agents  

---

## Quick Start

```bash
# Edit a prompt
$EDITOR tools/dev/codex_prompts/rc_live_fail.prompt.md

# Test your changes
./tools/dev/cockpit.sh rc  # Or: ./tools/dev/zacus.sh rc-autofix

# Verify evidence was generated
ls -lh artifacts/rc_live/LATEST/commands.txt
cat artifacts/rc_live/LATEST/codex_last_message.md
```

---

## 1. Prompt Naming Convention

All prompt files must follow:

```
<scope>_<topic>.prompt.md
```

### Naming Rules

- **Scope**: One of `rc_live`, `auto_fix`
- **Topic**: Brief descriptor (e.g., `esp8266_panic`, `ports`, `ui_link`, `generic`)
- **Extension**: Always `.prompt.md`
- **Format**: Snake_case only (no dots, spaces, underscores in topic)

### Examples

```
✅ rc_live_fail.prompt.md          (main RC gate failure triage)
✅ auto_fix_generic.prompt.md       (fallback for unknown issues)
✅ auto_fix_esp8266_panic.prompt.md (ESP8266-specific fix)
❌ rcLiveFail.prompt.md             (camelCase not allowed)
❌ rc-live-ports.prompt                 (wrong extension)
```

---

## 2. Prompt Structure

Each prompt file must:

1. **Start with code fence** (3 backticks + "prompt")
2. **Define ROLE** (first line after fence)
3. **Define Goal** (what Codex should achieve)
4. **List any constraints** (e.g., "2 messages max")
5. **Reference inputs** (e.g., ARTIFACT_PATH)
6. **End with code fence**

### Template

```markdown
\`\`\`prompt
ROLE: Firmware PM + QA gatekeeper. [X messages max].
Goal: [Brief goal statement].
Inputs: [What variables/files are available from ARTIFACT_PATH?]
Output: [What should Codex produce?]
Gates: [What commands verify success?]
\`\`\`
```

### Real Example

```markdown
\`\`\`prompt
ROLE: Firmware PM + QA gatekeeper. 2 messages max.
Goal: Make UI_LINK_STATUS gate pass (connected=1) reliably.
Inputs: Check baud/pins from ARTIFACT_PATH/summary.json + logs
Output: Minimal patch to ui/main.cpp
Gates: pio run -e esp32dev + rc live fast + ui_link log.
\`\`\`
```

---

## 3. Testing Procedure

### Quick Test (No Hardware)

```bash
# Dry-run prompt without Codex call
cat tools/dev/codex_prompts/auto_fix_ports.prompt.md

# Check syntax
head -1 tools/dev/codex_prompts/auto_fix_ports.prompt.md  # Should see ```prompt
```

### Full Test (With Codex)

```bash
# Option A: Test via RC live
export ZACUS_REQUIRE_HW=0  # Skip hardware wait
./tools/dev/cockpit.sh rc
# → On failure, codex will run rc_live_fail.prompt.md

# Option B: Test via auto-fix
./tools/dev/zacus.sh rc-autofix
# → Script automatically selects correct auto_fix_*.prompt.md

# Option C: Test specific prompt manually
./tools/dev/codex_prompt_menu.sh --run tools/dev/codex_prompts/auto_fix_generic.prompt.md
```

### Verify Evidence

```bash
# Find latest artifact
LATEST=$(ls -1dt artifacts/rc_live/*/ | head -1)

# Check that codex command was logged
cat "$LATEST/commands.txt"

# Check codex response
cat "$LATEST/codex_last_message.md"

# Check RC auto-fix log (if using zacus.sh)
cat "$LATEST/rc_autofix.log"
```

---

## 4. Adding a New Prompt

### Workflow

1. **Create** new file in `tools/dev/codex_prompts/`
2. **Follow naming convention** (section 1)
3. **Follow prompt structure** (section 2)
4. **Choose trigger logic**:
   - If RC live specific → `rc_live_*.prompt.md` (used automatically by `cockpit.sh rc`)
   - If auto-fix specific → `auto_fix_*.prompt.md` + wire in `zacus.sh choose_autofix_prompt()`

### Example: New Prompt for RTOS Hang

```bash
# 1. Create file
cat > tools/dev/codex_prompts/auto_fix_rtos_hang.prompt.md <<'PROMPT'
\`\`\`prompt
ROLE: Firmware PM + QA gatekeeper. 2 messages max.
Goal: Fix RTOS deadlock (Task watchdog timeout) in main loop.
Inputs: artifacts from ARTIFACT_PATH
Output: Minimal changes to FreeRTOS config or task priorities
Gates: pio run -e esp32dev + rc live fast
\`\`\`
PROMPT

# 2. Wire in zacus.sh (if needed)
# Edit tools/dev/zacus.sh choose_autofix_prompt() function
# Add: elif [[ ... ]]; then prompt="$PROMPT_DIR/auto_fix_rtos_hang.prompt.md"

# 3. Test
./tools/dev/codex_prompt_menu.sh --run tools/dev/codex_prompts/auto_fix_rtos_hang.prompt.md
```

---

## 5. Modifying an Existing Prompt

### Before Editing

```bash
# Document what's changing
git log -1 --oneline tools/dev/codex_prompts/rc_live_fail.prompt.md

# Backup current version (optional)
cp tools/dev/codex_prompts/rc_live_fail.prompt.md \
   tools/dev/codex_prompts/rc_live_fail.prompt.md.backup.20260216
```

### Edit Carefully

- Keep Role + message limit constraint
- Keep ARTIFACT_PATH references (Codex depends on them)
- Be concise (every word costs tokens)
- Use cross-references to other prompts if needed

### After Editing

```bash
# Verify syntax (code fence must be closed)
tail -1 tools/dev/codex_prompts/rc_live_fail.prompt.md  # Should see ```

# Quick sanity check
wc -l tools/dev/codex_prompts/rc_live_fail.prompt.md   # Verify expected size

# Test via appropriate workflow
./tools/dev/cockpit.sh rc  # Or: ./tools/dev/zacus.sh rc-autofix
```

---

## 6. Verification Gate (Audit)

### Manual Verification

```bash
# Check all prompts exist
ls -1 tools/dev/codex_prompts/*.prompt.md | wc -l  # Should match expected count

# Check naming convention
ls -1 tools/dev/codex_prompts/*.prompt.md | \
  grep -E '^.+/(rc_live|auto_fix)_[a-z_]+\.prompt\.md$' | \
  wc -l  # All should match pattern

# Check syntax (code fence closed)
for f in tools/dev/codex_prompts/*.prompt.md; do
  tail -1 "$f" | grep -q '```' || echo "SYNTAX ERROR: $f"
done
```

### Automated Verification (Future)

Currently no audit script validates prompt files. Recommendation:

```bash
# In tools/test/audit_coherence.py (TODO):
def check_codex_prompts():
  for prompt in PROMPT_DIR.glob("*.prompt.md"):
    # Verify naming convention
    # Verify code fence syntax
    # Verify ARTIFACT_PATH references exist
    # Verify referenced gates are valid commands
```

---

## 7. Prompt Usage by Path

### RC Live Trigger Points

| Path | When Called | Prompt | Env. Vars |
|------|------------|--------|-----------|
| `cockpit.sh rc` | Smoke test fails | `rc_live_fail.prompt.md` | `ARTIFACT_PATH` = latest artifacts dir |
| `zacus.sh rc-autofix` + generic | Unknown issue | `auto_fix_generic.prompt.md` | `ARTIFACT_PATH`, + summary.json |
| `zacus.sh rc-autofix` + ports | Port fail | `auto_fix_ports.prompt.md` | `ARTIFACT_PATH`, + resolve_ports.json |
| `zacus.sh rc-autofix` + ui_link | UI_LINK fail | `auto_fix_ui_link.prompt.md` | `ARTIFACT_PATH`, + ui_link.log |
| `zacus.sh rc-autofix` + panic | ESP8266 panic | `auto_fix_esp8266_panic.prompt.md` | `ARTIFACT_PATH`, + smoke_esp8266_usb.log |
| Manual menu | User selects | Any prompt | `EVIDENCE_DIR` (from evidence_init) |

---

## 8. Debugging Prompts

### Problem: Codex Not Called

```bash
# Check if RC actually failed
./tools/dev/cockpit.sh rc 2>&1 | tail -20

# If it passed, try forcing failure (for testing)
# Option 1: Unplug hardware
# Option 2: Mock failure in run_matrix_and_smoke.sh (not recommended in prod)
```

### Problem: Codex Called But Wrong Prompt

```bash
# Check prompt selection logic (if using zacus.sh)
cat artifacts/rc_live/LATEST/rc_autofix.log
# Should show: reason=<port|ui_link|esp8266_panic|generic>

# If reason is wrong, check condition in choose_autofix_prompt()
grep -A 30 "choose_autofix_prompt()" tools/dev/zacus.sh
```

### Problem: Prompt Doesn't Run

```bash
# Check if prompt file exists
test -f tools/dev/codex_prompts/rc_live_fail.prompt.md && echo "EXISTS" || echo "MISSING"

# Check if codex is installed
command -v codex || echo "Codex not in PATH"

# Check manual menu
./tools/dev/codex_prompt_menu.sh --list
```

---

## 9. Token Optimization

### Keep Prompts Small

- Remove redundant sentences (each word = tokens)
- Reference ARTIFACT_PATH instead of repeating context
- Use short, specific role titles
- 2-message constraint helps cut cost

### Example Savings

```
❌ VERBOSE (18 lines, ~200 tokens):
"You are a firmware expert with deep knowledge of..."

✅ MINIMAL (4 lines, ~50 tokens):
"ROLE: Firmware PM. 2 messages max. Goal: Fix panic."
```

---

## 10. Prompt Checklist

Before committing a new/modified prompt:

- [ ] File follows naming convention (`<scope>_<topic>.prompt.md`)
- [ ] Code fence syntax valid (```prompt ... ```)
- [ ] ROLE defined on first line
- [ ] Goal statement is clear and concise
- [ ] Message limit documented (e.g., "2 messages max")
- [ ] ARTIFACT_PATH or input sources referenced
- [ ] Output/gate clearly stated
- [ ] Prompt tested via appropriate trigger (rc / rc-autofix / menu)
- [ ] Evidence generated to `artifacts/rc_live/<timestamp>/`
- [ ] Codex response review (check `codex_last_message.md`)
- [ ] Size reasonable (< 20 lines ideally)

---

## References

- Prompt Inventory: [docs/RC_LIVE_CODEX_STATUS.md](RC_LIVE_CODEX_STATUS.md)
- RC Final Board: [docs/RC_FINAL_BOARD.md](RC_FINAL_BOARD.md)
- Evidence System: [docs/TEST_SCRIPT_COORDINATOR.md](TEST_SCRIPT_COORDINATOR.md)
- Agent Briefing: [.github/agents/AGENT_BRIEFINGS.md](../.github/agents/AGENT_BRIEFINGS.md)

