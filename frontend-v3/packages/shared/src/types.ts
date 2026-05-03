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

// ============================================================
// HINTS ENGINE TYPES (mirror tools/hints/server.py Pydantic schema)
// ============================================================

/** Group profile sent to /hints/ask. Mirrors GROUP_PROFILES in server.py. */
export type HintsGroupProfile = 'TECH' | 'NON_TECH' | 'MIXED' | 'BOTH';

/** Per-puzzle entry as returned by GET /hints/sessions. */
export interface HintsSessionEntry {
  puzzle_id: string;
  count: number;
  last_at_ms: number;
  total_penalty: number;
  cooldown_until_ms: number;
  puzzle_started_at_ms: number;
  failed_attempts_for_puzzle: number;
}

/** Per-session aggregate as returned by GET /hints/sessions. */
export interface HintsSession {
  session_id: string;
  puzzles: HintsSessionEntry[];
  total_penalty: number;
  total_hints: number;
}

/** Adaptive profile config (mirror compute_level_floor inputs). */
export interface HintsAdaptiveProfile {
  base_modifier: number;
  stuck_minutes_per_bump: number;
  max_auto_bump: number;
}

export interface HintsAdaptiveConfig {
  enabled: boolean;
  profiles: Record<string, HintsAdaptiveProfile>;
  failed_attempts: { bump_every: number; max_bump: number };
}

export interface HintsTrackerConfig {
  cooldown_s: number;
  max_per_puzzle: number;
  penalty_per_level: Record<string, number>;
  adaptive: HintsAdaptiveConfig;
}

/** GET /hints/sessions envelope. */
export interface HintsSessionsResponse {
  sessions: HintsSession[];
  total_sessions: number;
  config: HintsTrackerConfig;
  now_ms: number;
}

/** GET /healthz envelope (subset relevant for the dashboard). */
export interface HintsHealthz {
  status: string;
  phrases_loaded: number;
  puzzles_loaded: number;
  phrases_path: string;
  safety_puzzles_loaded: number;
  safety_path: string;
  litellm_url: string;
  llm_model: string;
  adaptive_enabled: boolean;
  adaptive_path: string;
  adaptive_profiles: string[];
}

/** POST /hints/ask response — mirrors HintAskResponse in server.py. */
export interface HintAskResponse {
  refused: boolean;
  reason: string | null;
  hint: string | null;
  hint_static: string | null;
  hint_rewritten: string | null;
  level: number | null;
  level_requested: number | null;
  level_served: number | null;
  puzzle_id: string;
  source: string;
  model_used: string;
  latency_ms: number;
  count: number;
  score_penalty: number;
  total_penalty: number;
  cooldown_until_ms: number;
  retry_in_s: number;
  details: string | null;
  // Adaptive surface (P4)
  level_floor_adaptive: number;
  stuck_minutes: number;
  failed_attempts: number;
  group_profile_used: HintsGroupProfile | null;
}

// ============================================================
// VOICE BRIDGE TYPES (mirror tools/voice/bridge.py FastAPI surface)
// ============================================================

/** GET /health/ready envelope. F5-TTS warmup status. */
export interface VoiceBridgeReady {
  ready: boolean;
  f5_loaded: boolean;
  warmup_ms: number;
  cache_size: number;
}

/** GET /tts/cache/stats envelope. */
export interface VoiceBridgeCacheStats {
  count: number;
  size_mb: number;
  hits: number;
  misses: number;
  hit_rate_since_boot: number;
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
