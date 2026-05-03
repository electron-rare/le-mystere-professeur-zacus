import type {
  ScenarioEngine,
  EngineState,
  EngineDecision,
  GameEvent,
  ScoreResult,
  Phase,
  GroupProfile,
  NpcMood,
  ScenarioYaml,
  DurationMode,
} from '@zacus/shared';
import { parseScenarioYaml } from '@zacus/shared';

// Internal engine config — not exported; use ZacusScenarioEngine below.
interface EngineConfig {
  targetDuration: number;
  mode: DurationMode;
}

export class ZacusScenarioEngine implements ScenarioEngine {
  private scenario: ScenarioYaml | null = null;
  private config: EngineConfig | null = null;
  private state: EngineState = this.#initialState();
  private startMs = 0;

  // ----------------------------------------------------------------
  load(yaml: string): void {
    this.scenario = parseScenarioYaml(yaml);
    this.state = this.#initialState();
  }

  // ----------------------------------------------------------------
  start(config: EngineConfig): void {
    if (!this.scenario) throw new Error('Call load() before start()');
    this.config = config;
    this.startMs = Date.now();
    this.state.phase = 'INTRO';
  }

  // ----------------------------------------------------------------
  tick(nowMs: number): EngineState {
    if (!this.scenario || !this.config) return this.state;
    this.state.elapsedMs = nowMs - this.startMs;
    this.#updatePhase();
    this.#updateMood();
    return { ...this.state };
  }

  // ----------------------------------------------------------------
  onEvent(event: GameEvent): EngineDecision[] {
    if (!this.scenario || !this.config) return [];
    const decisions: EngineDecision[] = [];

    switch (event.type) {
      case 'puzzle_solved': {
        const puzzleId = event.data['puzzle_id'] as string;
        this.state.solvedPuzzles.push(puzzleId);
        this.#assembleCode(puzzleId);
        decisions.push({
          action: 'change_mood',
          data: { mood: 'impressed' satisfies NpcMood },
        });
        decisions.push({
          action: 'speak',
          data: { category: 'congratulations', puzzle: puzzleId },
        });
        break;
      }

      case 'profile_detected': {
        this.state.groupProfile = event.data['profile'] as GroupProfile;
        decisions.push({
          action: 'speak',
          data: { category: 'adaptation', profile: this.state.groupProfile },
        });
        break;
      }

      case 'hint_given': {
        const puzzleId = event.data['puzzle_id'] as string;
        this.state.hintsGiven[puzzleId] = (this.state.hintsGiven[puzzleId] ?? 0) + 1;
        decisions.push({
          action: 'speak',
          data: {
            category: 'hints',
            puzzle: puzzleId,
            level: this.state.hintsGiven[puzzleId],
          },
        });
        break;
      }

      case 'puzzle_skipped': {
        const puzzleId = event.data['puzzle_id'] as string;
        this.state.skippedPuzzles.push(puzzleId);
        decisions.push({
          action: 'skip_puzzle',
          data: { puzzle_id: puzzleId },
        });
        break;
      }

      case 'phase_changed': {
        const phase = event.data['phase'] as Phase;
        this.state.phase = phase;
        decisions.push({
          action: 'ring_phone',
          data: { reason: 'phase_transition', phase },
        });
        break;
      }

      case 'game_start': {
        this.state.phase = 'PROFILING';
        decisions.push({
          action: 'speak',
          data: { category: 'narrative', scene: 'intro' },
        });
        break;
      }

      case 'game_end': {
        this.state.phase = 'OUTRO';
        this.state.completed = true;
        decisions.push({
          action: 'speak',
          data: { category: 'narrative', scene: 'outro' },
        });
        break;
      }
    }

    return decisions;
  }

  // ----------------------------------------------------------------
  getState(): EngineState {
    return { ...this.state };
  }

  // ----------------------------------------------------------------
  getScore(): ScoreResult {
    if (!this.scenario || !this.config) {
      return { baseScore: 0, timePenalty: 0, hintPenalty: 0, bonus: 0, total: 0, rank: 'D' };
    }
    const { scoring } = this.scenario;
    const elapsedMin = this.state.elapsedMs / 60_000;
    const targetMin = this.config.targetDuration;
    const totalHints = Object.values(this.state.hintsGiven).reduce((a, b) => a + b, 0);

    const timePenalty = Math.max(0, Math.round((elapsedMin - targetMin) * scoring.time_penalty_per_minute));
    const hintPenalty = totalHints * scoring.hint_penalty;
    // Bonus only counts when the game is actually finished — otherwise
    // an in-progress game would always show inflated scores at t=0.
    const bonus =
      this.state.completed && elapsedMin < targetMin * 0.8
        ? scoring.bonus_fast_completion
        : 0;
    const total = Math.max(0, scoring.base_score - timePenalty - hintPenalty + bonus);

    return {
      baseScore: scoring.base_score,
      timePenalty,
      hintPenalty,
      bonus,
      total,
      rank: this.#scoreRank(total, scoring.base_score),
    };
  }

  // ----------------------------------------------------------------
  // Private helpers
  // ----------------------------------------------------------------

  #initialState(): EngineState {
    return {
      phase: 'INTRO',
      groupProfile: null,
      activePuzzle: null,
      solvedPuzzles: [],
      skippedPuzzles: [],
      hintsGiven: {},
      npcMood: 'neutral',
      elapsedMs: 0,
      codeAssembled: '',
      completed: false,
    };
  }

  #updatePhase(): void {
    if (!this.scenario || !this.config) return;
    const { duration } = this.scenario;
    const elapsedMin = this.state.elapsedMs / 60_000;
    const profilingMin = duration.profiling_minutes ?? 10;
    const climaxMin = duration.climax_minutes ?? 10;
    const outroMin = duration.outro_minutes ?? 5;

    if (elapsedMin < profilingMin) {
      this.state.phase = 'PROFILING';
    } else if (elapsedMin < this.config.targetDuration - climaxMin - outroMin) {
      this.state.phase = 'ADAPTIVE';
    } else if (elapsedMin < this.config.targetDuration - outroMin) {
      this.state.phase = 'CLIMAX';
    } else {
      this.state.phase = 'OUTRO';
    }
  }

  #updateMood(): void {
    const { solvedPuzzles, hintsGiven, elapsedMs } = this.state;
    if (!this.config) return;
    const elapsedMin = elapsedMs / 60_000;
    const totalHints = Object.values(hintsGiven).reduce((a, b) => a + b, 0);

    if (elapsedMin < this.config.targetDuration * 0.6 && solvedPuzzles.length >= 3) {
      this.state.npcMood = 'impressed';
    } else if (totalHints > 5 || elapsedMin > this.config.targetDuration * 0.9) {
      this.state.npcMood = 'worried';
    } else {
      this.state.npcMood = 'neutral';
    }
  }

  #assembleCode(puzzleId: string): void {
    if (!this.scenario) return;
    const puzzle = this.scenario.puzzles.find((p) => p.id === puzzleId);
    if (!puzzle) return;
    // Append the code digit(s) contributed by this puzzle
    const digits = puzzle.code_digits ?? (puzzle.code_digit != null ? [puzzle.code_digit] : []);
    this.state.codeAssembled += digits.join('');
  }

  #scoreRank(total: number, base: number): ScoreResult['rank'] {
    const ratio = total / base;
    if (ratio >= 0.95) return 'S';
    if (ratio >= 0.8) return 'A';
    if (ratio >= 0.65) return 'B';
    if (ratio >= 0.5) return 'C';
    return 'D';
  }
}
