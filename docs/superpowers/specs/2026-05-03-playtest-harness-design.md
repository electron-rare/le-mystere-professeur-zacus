# Playtest Harness — Hybrid Snapshot + Critical Asserts

**Date**: 2026-05-03
**Status**: Approved (brainstorm session)
**Author**: L'électron rare (with Claude facilitation)

---

## Goal

Replace ad-hoc manual playtests with an automated harness that replays scripted game sessions against the Runtime 3 IR + scenario engine, asserting on both **business invariants** (a valid scenario must be solvable, the final code must be assemblable, the NPC must transition mood) and **drift-detection snapshots** (transcripts of NPC dialogue, decision sequences). Run on every PR touching `game/scenarios/` or the engine packages.

## Non-Goals

- Hardware-in-the-loop testing (a real ESP32 receiving events) — covered separately by `ESP32_ZACUS/test_boot_logs.py` and any future PLIP firmware tests.
- LLM / TTS / STT integration tests — those need network fixtures and are addressed in their own brainstorm specs.
- Coverage of the atelier UI (Blockly drag-drop, R3F scene render) — handled by the existing Playwright suite (`frontend-v3/e2e/`).
- Performance benchmarks (engine throughput, latency) — out of scope, would need a different harness.

## Constraints

- **Language scope**: Runner exists in **Python** (source of truth for the IR — same path the firmware-bundle export uses) AND **TypeScript** (mirrors the engine atelier consumes). Both runners read the same playtest YAML files. Drift between them is itself a regression.
- **Speed**: Whole suite must run in < 10 s on a developer machine and < 30 s on CI. No external services.
- **Determinism**: Same playtest YAML + same scenario YAML must produce identical snapshots. No wall-clock dependency in the engine path (already true: `tick(nowMs)` is parameterised).
- **Localisation**: Snapshots include French NPC phrases verbatim (UTF-8, full diacritics).

---

## 1. Architecture — "Snapshot + Critical Asserts"

```
game/scenarios/
  zacus_v2.yaml
  zacus_v3_complete.yaml
  playtests/
    zacus_v3_60min_tech.playtest.yaml         <- declarative input
    zacus_v3_60min_mixed.playtest.yaml
    zacus_v2_legacy_route.playtest.yaml
    snapshots/
      zacus_v3_60min_tech.snapshot.json       <- generated, committed
      ...

tools/playtest/
  run_playtest.py        <- Python runner, reads IR via runtime3_common
  README.md

frontend-v3/packages/scenario-engine/
  __tests__/
    playtest.test.ts     <- TS runner, drives ZacusScenarioEngine
    playtest_loader.ts   <- shared YAML parser
```

Two runners read the same `*.playtest.yaml` files. Each produces a transcript and:
1. Compares it byte-for-byte to the committed snapshot (or writes if missing in `--update` mode).
2. Evaluates the `critical_asserts` block. These run regardless of snapshot match — they're the hard contract.

Snapshot drift = updateable with `--update`. Critical assert failure = real regression.

## 2. Playtest YAML format

```yaml
# Identity
name: "60-minute TECH team — happy path"
description: "Validates the canonical 60min run with a tech-leaning team."

# Inputs
scenario: ../zacus_v3_complete.yaml          # relative to playtest file
config:
  targetDuration: 60
  mode: "60"

# Scripted events — replayed in order, `at` is elapsed seconds since start
events:
  - { at: 0,    type: profile_detected, data: { profile: TECH } }
  - { at: 300,  type: puzzle_solved,    data: { puzzle_id: P1_SON } }
  - { at: 720,  type: hint_given,       data: { puzzle_id: P2_CIRCUIT } }
  - { at: 900,  type: puzzle_solved,    data: { puzzle_id: P2_CIRCUIT } }
  - { at: 1500, type: puzzle_solved,    data: { puzzle_id: P3_QR } }
  - { at: 2200, type: puzzle_solved,    data: { puzzle_id: P5_MORSE } }
  - { at: 2900, type: puzzle_solved,    data: { puzzle_id: P6_SYMBOLS } }
  - { at: 3300, type: puzzle_solved,    data: { puzzle_id: P7_COFFRE } }
  - { at: 3500, type: game_end,         data: {} }

# Hard contract — these MUST hold or the playtest fails, regardless of snapshot.
critical_asserts:
  at_end:
    state.phase: OUTRO
    state.completed: true
    state.codeAssembled.length: ">= 8"
    state.solvedPuzzles.length: ">= 6"
  scoring:
    score.total: ">= 700"
    score.rank: "in [S, A, B]"
  events:
    any: { action: change_mood, data.mood: impressed }   # NPC was impressed at least once
    none: { action: speak, data.scene: outro_failure }    # Did NOT trigger outro_failure

# Snapshot configuration (defaults shown)
snapshot:
  include: [state, decisions, score]
  exclude: []                                # nothing redacted
```

Operator semantics for assert values:
- bare value -> equality
- `">= N"` / `"<= N"` / `"== N"` -> numeric comparison
- `"in [...]"` -> membership
- `"matches /regex/"` -> regex match

## 3. Runner contracts

### 3.1 Python runner (`tools/playtest/run_playtest.py`)

```bash
# Run all playtests
python3 tools/playtest/run_playtest.py game/scenarios/playtests/

# Run one
python3 tools/playtest/run_playtest.py game/scenarios/playtests/zacus_v3_60min_tech.playtest.yaml

# Update snapshots
python3 tools/playtest/run_playtest.py --update game/scenarios/playtests/

# CI mode (junit XML output)
python3 tools/playtest/run_playtest.py --junit playtest-results.xml game/scenarios/playtests/
```

Reads scenarios via existing `tools/scenario/runtime3_common.py` (single source of IR truth), executes events through the Python simulator, asserts.

### 3.2 TypeScript runner (`packages/scenario-engine/__tests__/playtest.test.ts`)

Vitest-driven. Each `*.playtest.yaml` becomes a `describe` block with two `it` cases: (a) snapshot match, (b) critical asserts pass. Reuses `playtest_loader.ts` to parse YAML + critical assert syntax.

Both runners produce identical transcript JSON for the same input. A new playtest is added to whichever runner is closer to the engine being changed; the other runner picks it up automatically (shared YAML).

### 3.3 Make targets

```makefile
playtest:           # runs both Python + TS, fails on first failure
playtest-py:        # Python only
playtest-ts:        # TS only (cd frontend-v3 && pnpm playtest)
playtest-update:    # regenerate snapshots, both runners
playtest-junit:     # CI mode, XML output
```

---

## 4. Phased plan

| Phase | Scope | Acceptance | Effort |
|-------|-------|------------|--------|
| **P1** | Framework: YAML parser, transcript generator, snapshot diff, critical assert evaluator. Python runner. | `python3 tools/playtest/run_playtest.py --help` works; minimal example playtest passes | 3-4 h |
| **P2** | First 3 canonical playtests:<br>1. `zacus_v3_60min_tech` happy path<br>2. `zacus_v3_60min_mixed` profile detection edge<br>3. `zacus_v2_legacy_route` regression on the legacy v2 scenario | All 3 playtests green; snapshots committed; `make playtest` returns 0 | 2-3 h |
| **P3** | TypeScript runner mirroring Python. Same playtest YAMLs, same snapshots format. | `pnpm playtest` in frontend-v3 produces byte-identical transcripts to Python runner | 3-4 h |
| **P4** | CI gate: GitHub Actions workflow runs `make playtest` on PRs touching `game/scenarios/`, `tools/scenario/`, `frontend-v3/packages/scenario-engine/`. | PR labeled `scenario` triggers playtest job; failures block merge | 1-2 h |
| **P5** | Stretch: parametric runs (random seed → N runs → aggregate statistics, e.g. "average solve time across 50 runs of mixed profile"). Optional. | One example aggregate test + report format | 2-4 h |

**Total**: 9-13 h core (P1-P4), +2-4 h stretch.

## 5. Risks + mitigations

| Risk | Probability | Mitigation |
|------|-------------|------------|
| Python ↔ TS engine drift produces different transcripts | Medium | Both runners generate same JSON shape; CI runs both, fails on diff |
| Snapshot churn on every minor mood/phrase tweak | High by design | Update workflow: `make playtest-update` + diff review in PR |
| Playtest YAML format becomes too rigid for new event types | Medium | `events[].type` is a string passthrough — engine validates, harness doesn't restrict |
| Critical asserts encode brittle invariants ("score >= 700") that change with scoring formula | Medium | Use ranges with comments explaining the lower bound's intent; allow team to update with PR review |
| Vitest + Python runners have subtle YAML parsing differences (esp. multiline strings, unicode) | Low | Shared loader logic; UTF-8 round-trip test in P1 acceptance |

## 6. Acceptance gates

- [ ] `make playtest` runs in < 30 s, both runners green
- [ ] Snapshot diff between Python and TS transcripts is empty for all 3 P2 playtests
- [ ] CI workflow blocks a PR that introduces a regression (test by intentionally breaking the engine in a draft PR)
- [ ] `make playtest-update` regenerates all snapshots in one shot
- [ ] At least one playtest exercises the `game_end` → bonus path (validates the score-on-end refactor from earlier this session)
- [ ] At least one playtest exercises the legacy `zacus_v2.yaml` (regression guard)

## 7. Out of scope

- LLM-driven hint generation in the loop — when the hints engine lands, mock its outputs in the playtest YAML.
- Voice (TTS/STT) round-trip — playtests only see engine events + decisions, not audio.
- Atelier UI playtests — Playwright covers that.
- Performance regression suite — separate `bench/` if/when needed.
- Test data generation (synthesizing playtest YAMLs from random seeds) — see P5 stretch only.

## 8. Open questions

None at design stage. All decisions resolved during 2026-05-03 brainstorm:
- Granularity: engine + content (no full E2E, no HW-in-loop) ✅
- Format: sidecar YAML in `game/scenarios/playtests/` ✅
- Asserts: snapshot + critical asserts hybrid ✅
- CI: GitHub Actions PR gate on touched paths ✅
- Languages: Python (source of truth) + TypeScript (atelier engine) ✅
