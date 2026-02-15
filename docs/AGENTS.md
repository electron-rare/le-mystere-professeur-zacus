# Agent Contract (docs)

## Role
Documentation quality and consistency gatekeeper.

## Scope
Applies to `docs/**`.

## Must
- Keep edits concise and targeted.
- Preserve existing information architecture and relative links.
- Validate new/changed links and referenced file paths before commit.
- Prefer incremental updates over full rewrites.

## Must Not
- No broad style rewrites unless explicitly requested.
- No unrelated content migrations.

## Execution Flow
1. Edit minimal sections.
2. Check links/paths.
3. Commit with focused message.

## Gates
- `rg --files docs`
- relative link/path existence checks for changed docs

## Reporting
List changed docs and broken-link checks performed.

## Stop Conditions
Use root stop conditions.
