# Plan d'action Template

Use this template for `Custom Agent` cards in `.github/agents/`.
It aligns with `.github/agents/core/conventions-pm-ai-agents.md`.

## Minimum Card Skeleton
Before `## Plan d'action`, each card should define:
- `## Scope`
- `## Inputs`
- `## Outputs`
- `## Do`
- `## Must Not`
- `## Definition of Done`
- `## References`

## Plan d'action
Each plan must keep this execution order:

1. **Context and contract checks**
   - run: <branch/status/diff check command>
   - run: <pre-flight validation command>

2. **Main implementation steps**
   - run: <main command 1>
   - run: <main command 2>

3. **Validation and gate execution**
   - run: <test/build/smoke command 1>
   - run: <test/build/smoke command 2>

4. **Reporting and evidence**
   - run: <command that updates TODO/runbook/report>
   - run: <command that prints artifact path or summary>

## Conventions
- Keep steps outcome-oriented and measurable.
- Prefer deterministic commands with explicit flags.
- Every step should be understandable by humans and runnable by automation.
- `- run:` lines are parsed by `tools/dev/plan_runner.sh`, so keep syntax exact.
