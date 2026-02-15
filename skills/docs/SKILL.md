# Name
docs

## When To Use
Use for documentation changes, index updates, and link/path consistency checks.

## Trigger Phrases
- "update docs"
- "write guide"
- "add index"
- "AGENTS docs"

## Do
- Keep edits concise and scoped.
- Verify relative links and command examples.
- Maintain existing information architecture.

## Don't
- Do not perform broad rewrites without explicit request.
- Do not leave stale links.

## Quick Commands
- `rg --files docs`
- `rg -n "\]\(" docs/AGENTS_INDEX.md`
- `rg --files -g 'AGENTS.md'`
