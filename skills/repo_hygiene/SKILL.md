# Name
repo_hygiene

## When To Use
Use for checkpointing, artifact untracking, and cross-platform naming hygiene.

## Trigger Phrases
- "checkpoint"
- "gitignore"
- "untrack artifacts"
- "repo hygiene"

## Do
- Snapshot diff/status to `/tmp/zacus_checkpoint` before high-risk edits.
- Detect tracked build artifacts and untrack with `git rm --cached` only.
- Keep file names and docs portable across macOS/Linux.

## Don't
- Do not delete local artifact files when untracking.
- Do not mix hygiene fixes with unrelated refactors.

## Quick Commands
- `git diff --stat`
- `git ls-files | rg '(^|/)(\.pio|\.platformio|logs|dist|build|node_modules|\.venv)(/|$)'`
- `git status -sb`
