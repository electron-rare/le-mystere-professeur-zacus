# Copilot Agent Index

This index groups active `.github/agents/**/*.md` files by intent so contributors pick the right file quickly.
All cards should follow `.github/agents/core/conventions-pm-ai-agents.md` and `.github/agents/core/plan-template.md`.

## 1) Governance Layer

| File | Role |
|---|---|
| `core/conventions-pm-ai-agents.md` | Canonical PM + AI agent conventions (structure, risk, reporting, evidence). |
| `core/plan-template.md` | Canonical `## Plan d'action` template with `- run:` automation format. |
| `core/agent-briefings.md` | Shared implementation policies (notably cockpit git safeguards and evidence). |
| `domains/global.md` | Repo-wide guardrails, mandatory checkpoint, and default gates. |
| `core/copilot-index.md` | Entry map for all agent files in this folder. |

## 2) Domain Agent Cards (`Custom Agent`)

| File | Primary scope |
|---|---|
| `domains/hardware.md` | Hardware integration (BOM/enclosure/wiring + firmware validation gates). |
| `domains/audio.md` | Audio manifests and audio-related scenario checks. |
| `domains/game.md` | Scenario YAML and content pipeline validation/export. |
| `domains/printables.md` | Printables manifests and deterministic naming/validation. |
| `domains/tools.md` | Tooling scripts, CLI ergonomics, portability of commands. |
| `domains/docs.md` | Documentation and onboarding integrity. |
| `domains/kit.md` | Game master kit content and packaging consistency. |
| `domains/ci.md` | CI/workflow/template updates with minimal blast radius. |
| `domains/firmware-core.md` | Firmware-wide implementation and gate ownership. |
| `domains/firmware-tooling.md` | Firmware `tools/dev` conventions and script behavior. |
| `domains/firmware-copilot.md` | Firmware copilot execution workflow and artifact discipline. |
| `domains/firmware-tests.md` | Smoke/stress testing and evidence metadata. |
| `domains/firmware-docs.md` | Firmware docs/runbook synchronization. |
| `core/alignment-complete.md` | Final consistency pass before major handoff/release. |
| `core/phase-launch-plan.md` | Phase launch readiness checks and evidence capture. |

## 3) Active Phase Playbooks

| File | Phase objective |
|---|---|
| `phases/phase-2-esp-http-ws.md` | ESP HTTP API + WebSocket implementation/validation. |
| `phases/phase-2b-firmware-rtos.md` | Firmware RTOS/WiFi resilience and telemetry. |

## 4) Operational Status Artifacts

| File | Purpose |
|---|---|
| `reports/firmware-health-baseline.md` | Baseline evidence report and observed health metrics. |

Historical archive playbooks were purged from the active tree.
Use `git log -- .github/agents/archive/` if legacy context is needed.

## Selection Workflow

1. Start with `domains/global.md` for guardrails.
2. Open the domain `Custom Agent` card for your scope.
3. If work is phase-driven, open the corresponding file under `phases/`.
4. Build/update plan using `core/plan-template.md`.
5. Ensure outputs satisfy `core/conventions-pm-ai-agents.md`.
