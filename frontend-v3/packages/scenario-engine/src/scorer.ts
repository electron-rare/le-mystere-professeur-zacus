import type { ScoreResult, ScenarioYaml, EngineState } from '@zacus/shared';

/**
 * Compute the final score for a completed game session.
 */
export function computeScore(
  scenario: ScenarioYaml,
  state: EngineState,
  targetDurationMin: number,
): ScoreResult {
  const { scoring } = scenario;
  const elapsedMin = state.elapsedMs / 60_000;
  const totalHints = Object.values(state.hintsGiven).reduce((a, b) => a + b, 0);

  const timePenalty = Math.max(
    0,
    Math.round((elapsedMin - targetDurationMin) * scoring.time_penalty_per_minute),
  );
  const hintPenalty = totalHints * scoring.hint_penalty;
  const bonus = elapsedMin < targetDurationMin * 0.8 ? scoring.bonus_fast_completion : 0;
  const total = Math.max(0, scoring.base_score - timePenalty - hintPenalty + bonus);

  return {
    baseScore: scoring.base_score,
    timePenalty,
    hintPenalty,
    bonus,
    total,
    rank: scoreRank(total, scoring.base_score),
  };
}

/**
 * Assemble the final unlock code from a list of solved puzzle IDs and their configs.
 */
export function assembleCode(solvedPuzzleIds: string[], puzzles: ScenarioYaml['puzzles']): string {
  let code = '';
  for (const id of solvedPuzzleIds) {
    const puzzle = puzzles.find((p) => p.id === id);
    if (!puzzle) continue;
    const digits = puzzle.code_digits ?? (puzzle.code_digit != null ? [puzzle.code_digit] : []);
    code += digits.join('');
  }
  return code;
}

function scoreRank(total: number, base: number): ScoreResult['rank'] {
  const ratio = total / base;
  if (ratio >= 0.95) return 'S';
  if (ratio >= 0.8) return 'A';
  if (ratio >= 0.65) return 'B';
  if (ratio >= 0.5) return 'C';
  return 'D';
}
