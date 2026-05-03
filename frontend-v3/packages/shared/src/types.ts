// ============================================================
// CORE EVENT / COMMAND TYPES (WebSocket protocol BOX-3 <-> Dashboard)
// ============================================================

export type GameEventType =
  | 'puzzle_solved'
  | 'puzzle_skipped'
  | 'hint_given'
  | 'npc_spoke'
  | 'phone_rang'
  | 'phase_changed'
  | 'timer_update'
  | 'profile_detected'
  | 'game_start'
  | 'game_end'
  | 'hw_failure';

export interface GameEvent {
  type: GameEventType;
  timestamp: number;
  data: Record<string, unknown>;
}

export type GameCommandType =
  | 'force_hint'
  | 'skip_puzzle'
  | 'add_bonus'
  | 'ring_phone'
  | 'set_duration'
  | 'manual_tts'
  | 'override_mood'
  | 'pause'
  | 'resume'
  | 'end_game';

export interface GameCommand {
  type: GameCommandType;
  data: Record<string, unknown>;
}

// ============================================================
// ENGINE STATE TYPES
// ============================================================

export type Phase = 'INTRO' | 'PROFILING' | 'ADAPTIVE' | 'CLIMAX' | 'OUTRO';
export type GroupProfile = 'TECH' | 'NON_TECH' | 'MIXED';
export type NpcMood = 'neutral' | 'impressed' | 'worried' | 'amused';
export type DurationMode = '30' | '45' | '60' | '90';
export type PuzzleProfile = 'TECH' | 'NON_TECH' | 'BOTH' | 'MIXED';

export interface EngineState {
  phase: Phase;
  groupProfile: GroupProfile | null;
  activePuzzle: string | null;
  solvedPuzzles: string[];
  skippedPuzzles: string[];
  hintsGiven: Record<string, number>;
  npcMood: NpcMood;
  elapsedMs: number;
  codeAssembled: string;
  /** True once the game_end event has fired. Drives bonus eligibility in getScore(). */
  completed: boolean;
}

export interface ScoreResult {
  baseScore: number;
  timePenalty: number;
  hintPenalty: number;
  bonus: number;
  total: number;
  rank: 'S' | 'A' | 'B' | 'C' | 'D';
}

export interface EngineDecision {
  action: 'speak' | 'ring_phone' | 'play_sound' | 'add_puzzle' | 'skip_puzzle' | 'change_mood';
  data: Record<string, unknown>;
}

// ============================================================
// SCENARIO ENGINE INTERFACE
// ============================================================

export interface ScenarioEngine {
  load(yaml: string): void;
  start(config: { targetDuration: number; mode: DurationMode }): void;
  tick(nowMs: number): EngineState;
  onEvent(event: GameEvent): EngineDecision[];
  getState(): EngineState;
  getScore(): ScoreResult;
}

// ============================================================
// SCENARIO YAML SCHEMA TYPES
// ============================================================

export interface PuzzleHardware {
  id: string;
  name: string;
  type: string;
  profile: PuzzleProfile;
  phase: 'profiling' | 'adaptive' | 'climax';
}

export interface PuzzleConfig {
  id: string;
  name: string;
  // P1
  melody_notes?: string[];
  difficulty?: number;
  code_digits?: number[];
  // P2
  components?: string[];
  valid_circuit?: string;
  code_digit?: number;
  // P3
  qr_codes?: string[];
  correct_order?: number[];
  // P4
  target_freq?: number;
  range_min?: number;
  range_max?: number;
  // P5
  message?: string;
  mode?: 'tech' | 'light';
  // P6
  symbols?: string[];
  // P7
  code_length?: number;
  max_attempts?: number;
}

export interface NpcProfilingConfig {
  tech_threshold_s: number;
  nontech_threshold_s: number;
}

export interface NpcDurationConfig {
  target_minutes: number;
  mode: DurationMode;
}

export interface NpcAdaptiveRule {
  condition: 'fast' | 'slow';
  action: 'add' | 'skip' | 'hint';
}

export interface PhaseDefinition {
  type: Phase;
  puzzles: Array<{
    puzzle_ref: string;
    profile_filter: PuzzleProfile;
  }>;
}

export interface ScenarioYaml {
  id: string;
  version: string;
  title: string;
  duration: {
    target_minutes: number;
    min_minutes: number;
    max_minutes: number;
    profiling_minutes?: number;
    climax_minutes?: number;
    outro_minutes?: number;
  };
  hardware: {
    puzzles: PuzzleHardware[];
  };
  scoring: {
    base_score: number;
    time_penalty_per_minute: number;
    hint_penalty: number;
    bonus_fast_completion: number;
  };
  npc: {
    profiling?: NpcProfilingConfig;
    adaptive_rules: NpcAdaptiveRule[];
  };
  phases: PhaseDefinition[];
  puzzles: PuzzleConfig[];
}
