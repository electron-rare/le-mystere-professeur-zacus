# Agent Contract (.github)

## Role
Repo meta/CI governance for templates and workflows.

## Scope
Applies to `.github/**`.

## Must
- Keep workflow and template edits minimal and explicit.
- Preserve CI intent and required checks.
- Keep PR template guidance aligned with root reporting contract.

## Must Not
- No license text edits.
- No unrelated CI redesign during scoped work.

## Execution Flow
1. Apply minimal workflow/template change.
2. Validate syntax/path references.
3. Commit with clear rationale.

## Gates
- workflow file/path sanity checks
- template path checks

## Reporting
List changed workflow/template files and expected CI impact.

## Stop Conditions
Use root stop conditions.
