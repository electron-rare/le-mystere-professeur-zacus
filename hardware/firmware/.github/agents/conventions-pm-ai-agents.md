# Project Management and AI Agent Conventions

## Purpose
This file is the baseline standard for `.github/agents/` content.
It reduces ambiguity across agent cards, phase playbooks, and status reports.

## Web References Used
- Scrum Guide: https://www.scrumguides.org/scrum-guide.html
- Agile Manifesto: https://agilemanifesto.org/
- GitHub Projects (planning/tracking): https://docs.github.com/en/issues/planning-and-tracking-with-projects/learning-about-projects/about-projects
- GitHub Issue/PR templates: https://docs.github.com/en/communities/using-templates-to-encourage-useful-issues-and-pull-requests/about-issue-and-pull-request-templates
- Anthropic, Building effective agents: https://www.anthropic.com/engineering/building-effective-agents
- Anthropic, Model Context Protocol: https://www.anthropic.com/news/model-context-protocol
- NIST AI RMF: https://www.nist.gov/itl/ai-risk-management-framework

## Folder Taxonomy
- `Custom Agent` files: operational cards for day-to-day execution.
- `PHASE_*` files: phase-level implementation playbooks.
- `*_REPORT` / `*_BASELINE` / `*_SUMMARY`: evidence and status artifacts.
- `INDEX` / `TEMPLATE` / `CONVENTIONS`: governance and onboarding files.

## Required Structure for Custom Agent Cards
Each `Custom Agent` file should contain, in this order:
1. `## Scope`
2. `## Inputs`
3. `## Outputs`
4. `## Do`
5. `## Must Not`
6. `## Gates` (or a clear pointer to canonical gates)
7. `## Definition of Done`
8. `## References`
9. `## Plan d'action` (with executable `- run:` lines)

## Project Management Conventions
- Keep one primary objective at a time and make it measurable (aligned with Product Goal / Sprint Goal logic).
- Keep work visible in backlog-style artifacts (`AGENT_TODO`, phase checklists, project boards).
- Use explicit Definition of Done before handoff.
- Track dependencies, blockers, and risk status in each report (`on_track`, `at_risk`, `blocked`).
- Keep stakeholder updates short, factual, and periodic.

## AI Agent Conventions
- Prefer the simplest pattern first: deterministic workflow before autonomous agent behavior.
- Increase autonomy only when the workflow path becomes too rigid for the task.
- Keep tool contracts explicit: inputs, outputs, failure behavior, timeout policy.
- Keep prompts and scripts auditable and versioned; avoid hidden magic abstractions.
- Require evidence for decisions (commands, logs, artifacts, validation results).

## Risk and Safety Conventions
- Apply a risk loop inspired by NIST AI RMF: `Govern -> Map -> Measure -> Manage`.
- Gate high-impact actions with explicit checks (build/smoke/tests before handoff).
- Treat missing telemetry as a failure mode, not as an implicit pass.
- Keep human escalation points explicit for destructive or uncertain operations.

## Reporting Conventions
Every completion report should include:
- Commits (`hash subject`)
- Tests/gates run with pass/fail
- Artifacts/log paths
- PR title/body/checklist draft
- Known limitations and open risks

## Migration Rule
Legacy files that do not yet match the standard should add a `## Conventions` section pointing to this document until fully migrated.
