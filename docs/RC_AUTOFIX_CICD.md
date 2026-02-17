# RC AutoFix (Scheduled) Workflow

This document describes the GitHub Actions workflow at .github/workflows/rc-autofix-cicd.yml.

## Trigger Manually (GitHub UI)

1. Go to Actions tab
2. Select "RC AutoFix (Scheduled)"
3. Click "Run workflow"

## Trigger via CLI

```bash
gh workflow run rc-autofix-cicd.yml
```

## Automatic Scheduled Run

- Runs daily at 2 AM UTC (adjust cron expression as needed)
- Can be modified in workflow file under schedule.cron

## Environment Variables Explained

| Variable | Value | Purpose |
|----------|-------|---------|
| ZACUS_GIT_AUTOCOMMIT | 1 | Enable automatic git commits after Codex fixes |
| ZACUS_GIT_ALLOW_WRITE | 1 | Allow git write operations (required for commits) |
| ZACUS_GIT_NO_CONFIRM | 1 | Skip confirmation prompts (required for automation) |
| ZACUS_REQUIRE_HW | 0 | Do not require physical hardware (simulation mode) |
| ZACUS_SKIP_UPLOAD | 1 | Skip ESP32/ESP8266 upload (stability + speed) |

## Workflow Steps

1. Checkout code from current branch
2. Setup Python and PlatformIO
3. Bootstrap Zacus environment
4. Configure Git user for commits
5. Run RC AutoFix with auto-commit enabled
6. Push changes if any commits were made
7. Upload artifacts for review (30-day retention)
8. Notify on failure (optional: configure GitHub issue comment)

## Tracking Results

### Via GitHub Actions UI

- Check "Run workflow" and the latest run
- View step logs for detailed output
- Download artifacts (rc_autofix.log, codex_last_message.md, etc.)

### Via Git History

```bash
git log --oneline | grep "Auto-fix:"  # Show all auto-fix commits
git show HEAD  # Review latest commit
```

## Failure Modes

| Failure | Action |
|---------|--------|
| RC pre-condition fails | Workflow continues (will fail unless fix applied) |
| Codex execution fails | Check logs in artifact/codex_last_message.md |
| Git commit fails | Check git config and repository permissions |
| Push fails | Check branch protection rules and credentials |

## Customization

### Change Schedule

Edit the cron expression:

```yaml
schedule:
  - cron: '0 2 * * 1-5'  # Mon-Fri at 2 AM UTC
  - cron: '0 6 * * *'    # Daily at 6 AM UTC
```

### Disable Auto-Commit

Set ZACUS_GIT_AUTOCOMMIT: '0' (requires manual commit review).

### Add Hardware Testing

Set ZACUS_REQUIRE_HW: '1' (requires physical ESP32/ESP8266 and a CI runner with USB).

### Add Slack Notification

```yaml
- name: Notify Slack on failure
  if: failure()
  uses: slackapi/slack-github-action@v1.24.0
  with:
    webhook-url: ${{ secrets.SLACK_WEBHOOK }}
    payload: |
      {
        "text": "RC AutoFix failed - check artifacts"
      }
```

## References

- Zacus CLI: hardware/firmware/tools/dev/zacus.sh
- RC Live Codex Status: hardware/firmware/docs/RC_LIVE_CODEX_STATUS.md
- Codex Prompt Maintenance: hardware/firmware/docs/CODEX_PROMPT_MAINTENANCE.md
- Agent Briefing: .github/agents/AGENT_BRIEFINGS.md
