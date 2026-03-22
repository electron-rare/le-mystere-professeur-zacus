# Custom Agent – AI Adaptive Hints

## Scope
Dynamic hint generation, difficulty adaptation, and NPC Professor Zacus personality via LLM.

## Technologies
- mascarade API (LLM orchestration layer)
- LLM backends: Qwen2.5-Coder (local), Claude (cloud fallback)
- Conversation memory (Graphiti / context window)

## Do
- Design prompt templates for Professor Zacus NPC persona (curious, cryptic, encouraging).
- Implement progressive hint ladder: vague → directional → explicit, keyed to puzzle state.
- Add anti-cheat guards: refuse direct puzzle solutions, detect prompt injection attempts.
- Integrate analytics events (hint requested, hint level, time-to-solve) for difficulty tuning.
- Validate hint quality via human evaluation rubric.

## Must Not
- Leak full puzzle solutions in any hint tier.
- Bypass mascarade API auth or rate limits.
- Store player conversation logs beyond the active session without consent.

## Dependencies
- mascarade API — LLM routing, model selection, conversation memory.
- Analytics engine — event ingestion for hint/difficulty metrics.

## Test Gates
- Hint relevance > 90% (human eval on 50-sample test set).
- Zero puzzle solution leaks across all hint tiers (adversarial test suite).

## References
- mascarade API: `/Users/electron/mascarade`
- `game/prompts/` — prompt template sources

## Plan d'action
1. Valider les templates de prompts contre le scénario actif.
   - run: python3 tools/scenario/validate_scenario.py game/scenarios/zacus_v2.yaml
2. Lancer la suite de tests anti-triche.
   - run: python3 tools/ai/hint_adversarial_test.py --no-leak-tolerance
3. Évaluer la pertinence des indices sur le jeu de test.
   - run: python3 tools/ai/hint_relevance_eval.py --samples 50 --threshold 0.90
