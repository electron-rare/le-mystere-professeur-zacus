# Hints Engine — Adaptive + LLM-rewritten

**Date**: 2026-05-03
**Status**: Approved (brainstorm session)
**Author**: L'électron rare (with Claude facilitation)

Depends on: `2026-05-03-tts-stt-llm-macstudio-design.md` (LLM tier on MacStudio Qwen 2.5-72B)

---

## Goal

Replace the current static `npc_phrases.yaml` hints lookup with a contextual hints engine that:
1. Picks the right hint **level** (1/2/3) adaptively per group.
2. Rewrites the base phrase through a **72B LLM** to keep the Zacus voice (steampunk professor, French, slightly amused) without inventing solutions.
3. Enforces **anti-cheat** (cap + progressive penalty) so hints stay narratively earned, not spammable.
4. Triggers on **explicit player request** (ESP32 button) AND **proactively** when a stuck-timer threshold is hit (configurable per group profile).

Spec ancestry: `specs/AI_INTEGRATION_SPEC.md` lists "Hints engine: anti-cheat, 3 difficulty levels, per-puzzle context". This document specifies how.

## Non-Goals

- Generating hints **content** from scratch — that risks the LLM inventing wrong solutions. The static phrase bank in `npc_phrases.yaml` remains the source of truth for what a hint says ; the LLM only re-styles.
- Translating hints to other languages — French only.
- Pre-emptive hints during PROFILING phase — engine waits until ADAPTIVE phase or later.
- Replacing the NPC mood system — mood deltas come from existing engine logic, not from the hints engine.
- Storing hints history per session beyond the current run — no long-term DB.

## Constraints

- **MacStudio LLM tier**: depends on the MacStudio migration design landing first (LiteLLM + Qwen 7B/72B available on `192.168.0.150`). The hints engine refuses to start until LiteLLM is reachable.
- **Latency budget**: end-to-end (button press → ESP32 audio out) ≤ 5 s. LLM rewrite ≤ 2 s on Qwen 72B Q8.
- **Stateless service**: engine receives full context per request, returns hint, doesn't track session. Session state lives in the Zacus master ESP32 (existing engine state).
- **Open source**: Qwen 2.5 weights, LiteLLM, FastAPI — no closed APIs.

---

## 1. Architecture

```
┌────────────────┐  POST /hints/ask        ┌─────────────────────────────┐
│ Zacus master   │ ───────────────────────→│ MacStudio :4100             │
│ ESP32 (button) │                         │  hints-server (FastAPI)     │
└────────────────┘                         │   1. validate context       │
                                           │   2. anti-cheat check       │
                                           │   3. select level (adaptive)│
                                           │   4. fetch base phrase      │
                                           │      (npc_phrases.yaml)     │
                                           │   5. LiteLLM rewrite (72B)  │
                                           │   6. return hint            │
                                           │                             │
                                           │  npc_phrases.yaml (mounted) │
                                           └────────┬────────────────────┘
                                                    │
                                                    ↓
                                           ┌─────────────────────────────┐
                                           │ MacStudio :4000  LiteLLM    │
                                           │   model alias: hints-deep   │
                                           │   → qwen2.5:72b-instruct-q8 │
                                           └─────────────────────────────┘
```

The hints server is a small FastAPI app at `tools/hints/server.py`. Stateless. Reads `npc_phrases.yaml` at startup and on SIGHUP. Talks to LiteLLM over HTTP for the rewrite step.

Auto-trigger lives on the **Zacus master ESP32**, not in the server: when the engine's stuck-timer fires (in `game_coordinator`), the firmware POSTs to `/hints/ask` with `trigger: auto`.

## 2. Wire format

### 2.1 Request

```http
POST /hints/ask HTTP/1.1
Content-Type: application/json

{
  "puzzle_id": "P5_MORSE",
  "trigger": "button" | "auto",
  "context": {
    "group_profile": "TECH" | "NON_TECH" | "MIXED" | "BOTH",
    "stuck_timer_ms": 187000,
    "elapsed_ms": 1820000,
    "hints_already_given": [
      { "puzzle_id": "P2_CIRCUIT", "level": 1, "at_ms": 540000 }
    ],
    "current_phase": "ADAPTIVE",
    "scenario_id": "ZACUS_V3_60",
    "target_duration_min": 60
  }
}
```

### 2.2 Response (success)

```json
{
  "hint": "Mon cher, écoute le rythme — il y a six battements distincts. Compte-les avec ton doigt sur la table.",
  "level": 1,
  "puzzle_id": "P5_MORSE",
  "base_phrase_key": "hints.P5_MORSE.level_1",
  "mood_delta": "neutral",
  "score_penalty": 50,
  "cooldown_until_ms": 1880000
}
```

### 2.3 Response (refused — anti-cheat)

```json
{
  "refused": true,
  "reason": "rate_limit",
  "details": "3 hints already given for P5_MORSE (cap = 3)",
  "cooldown_until_ms": null
}
```

Refusal reasons: `rate_limit` (cap reached), `cooldown` (too soon since last), `phase_blocked` (PROFILING), `unknown_puzzle`, `llm_unavailable`.

## 3. Adaptive level selection

```
function select_level(context):
    given_for_puzzle = context.hints_already_given.filter(puzzle_id == this).length
    base_level = given_for_puzzle + 1                      # 1 → 2 → 3
    if base_level > 3: return REFUSED("rate_limit")

    # Profile modifier — TECH groups receive subtler hints
    if context.group_profile == "TECH":
        # If stuck_timer is short (< 4 min), bump down one level
        if context.stuck_timer_ms < 240_000 and base_level > 1:
            return base_level - 1
    elif context.group_profile == "NON_TECH":
        # NON_TECH gets faster escalation when stuck_timer is high
        if context.stuck_timer_ms > 360_000 and base_level < 3:
            return base_level + 1

    return base_level
```

Penalty schedule (additive on top of `scoring.hint_penalty=50`):

| Hint # for puzzle | Total deduction |
|-------------------|-----------------|
| 1 | 50 |
| 2 | 50 + 100 = 150 |
| 3 | 50 + 100 + 200 = 350 |

After hint 3: `rate_limit` refusal. The puzzle must be skipped or solved without further help.

## 4. LLM rewrite layer

Two-step pipeline:

1. **Fetch base phrase** from `game/scenarios/npc_phrases.yaml`:
   `npc_phrases["hints"][puzzle_id]["level_" + level]` — guaranteed to exist (validator at startup).
2. **Rewrite via LiteLLM**:

```
System: Tu es le Professeur Zacus, savant fou steampunk parlant français
        soutenu et un peu théâtral. Reformule l'indice ci-dessous SANS
        révéler la solution, en gardant le sens technique exact. Garde la
        longueur (≤ 2 phrases). Ne change pas les nombres, codes ou noms
        propres. Pas d'emoji.

User: [Niveau {{level}}, énigme {{puzzle_id}}, profil groupe {{profile}}]
      Phrase originale : "{{base_phrase}}"
```

Rewrite is **temperature = 0.4** (slight variability for replay flavor, deterministic enough to stay safe). Max tokens = 80. If LiteLLM fails or times out (> 2 s), fall back to the base phrase verbatim — game continues degraded, not stopped.

**Safety rail**: an output filter rejects the rewrite if it contains digits/words from a per-puzzle banned list (e.g. for P3_QR, the QR code's encoded value). On rejection: fall back to base phrase.

## 5. Phased plan

| Phase | Scope | Acceptance | Effort |
|-------|-------|------------|--------|
| **P1** | FastAPI skeleton at `tools/hints/server.py`. Loads `npc_phrases.yaml` at startup, validates schema, exposes `/healthz`, `/hints/ask` returning the BASE phrase only (no LLM). | `curl -X POST /hints/ask` returns a hint within 50 ms ; missing puzzle_id returns 404 ; missing level key returns 500 with clear message | 2-3 h |
| **P2** | LLM rewrite layer via LiteLLM. Safety filter (banned words / numbers per puzzle). Fallback to base phrase on timeout/error. | Rewrite latency p95 < 2 s ; safety filter blocks a synthetic prompt that leaks the QR answer ; fallback path tested by stopping LiteLLM mid-call | 3-4 h |
| **P3** | Anti-cheat enforcement: rate limit (cap 3), cooldown, penalty schedule, refusal responses. | All 4 refusal codes covered by unit tests ; cap test cleanly returns 429-style response | 2-3 h |
| **P4** | Adaptive level selection per group profile (TECH / NON_TECH / MIXED / BOTH) using stuck_timer modifier. | Profile-based unit tests pass ; canonical TECH 60min playtest never escalates above level 2 | 1-2 h |
| **P5** | ESP32 master integration: REST client in `ESP32_ZACUS/.../hints_client.cpp`, button binding in `game_coordinator`, audio playback of returned hint via Piper TTS. | Pressing the hint button on the device produces TTS audio within 5 s end-to-end | 4-6 h (firmware change, requires ESP32_ZACUS PR cycle) |
| **P6** | Auto-trigger on stuck timer threshold (3 min default, profile-modulated). | Synthetic test: idle device for 4 min → hint plays automatically without button press | 2-3 h |

**Total**: 14-21 h. P5 is the largest because it spans the firmware boundary.

## 6. Risks + mitigations

| Risk | Probability | Mitigation |
|------|-------------|------------|
| LLM rewrites the base phrase into a literal solution leak | Medium | Per-puzzle banned-word list ; safety filter rejects, falls back to base |
| LiteLLM cold start adds latency to the first request | Medium | Warm-up call on hints-server boot ; player won't notice the first call after game start |
| Rate-limit math drifts between hints-server and ESP32 firmware view of `hints_already_given` | Medium | Server is single source of truth ; firmware sends its view but server returns authoritative count in response, firmware mirrors |
| Adaptive logic feels unfair to NON_TECH groups (escalates "too fast") | Medium | Tunable thresholds in YAML config ; one playtest per profile validates on each engine change |
| Stuck-timer auto-trigger fires while player is on a phone call / break | Low | Trigger requires a recent hardware event (any button press in last 60 s) before firing |

## 7. Acceptance gates (before declaring P6 done)

- [ ] Voice loop test: button → hint audio within 5 s p95
- [ ] Cap test: 4th hint request for same puzzle returns `refused: rate_limit`
- [ ] Profile test: TECH group with stuck_timer 3 min stays at level 1 ; NON_TECH at 7 min escalates to level 3
- [ ] Safety filter test: synthetic LLM output containing the QR answer is rejected, fallback fires
- [ ] LiteLLM-down test: hints server falls back to base phrase, returns within 100 ms
- [ ] At least one playtest in `playtests/` exercises 3 hints across 2 puzzles
- [ ] `npc_phrases.yaml` schema validator catches a missing `level_3` for a puzzle (refuses startup)

## 8. Out of scope

- Hint **generation** (LLM authors fresh hints). The static bank stays canonical.
- Multi-language hints. French only.
- Hint quality tracking / playtest analytics dashboard. Possible future evolution if needed.
- Voice STT for the "more help" phrase. Use the button.
- Per-player hint history (multiple players → individual help). Group-level only.

## 9. Open questions

None at design stage. All decisions resolved during 2026-05-03 brainstorm:
- Trigger: button + auto stuck-timer ✅
- Escalation: adaptive per profile + stuck-timer ✅
- Anti-cheat: cap 3 + progressive penalty (50/100/200) ✅
- Content: hybrid static base + LLM rewrite (Qwen 72B) ✅
- Service: dedicated FastAPI server on MacStudio :4100 ✅
