/** A scene in the scenario graph */
export interface SceneNode {
  /** Unique scene identifier (e.g. SCENE_INTRO) */
  name: string;
  /** Human-readable description */
  description: string;
  /** Maximum duration in seconds (0 = unlimited) */
  durationMax: number;
  /** Actions attached to this scene (timers, variable sets, etc.) */
  actions: SceneAction[];
  /** Outgoing transitions */
  transitions: TransitionEdge[];
}

/** A transition from one scene to another */
export interface TransitionEdge {
  /** Target scene name */
  targetScene: string;
  /** Optional condition description (from connected value block) */
  condition: string;
}

/** A timer action inside a scene */
export interface TimerAction {
  kind: 'timer';
  seconds: number;
  /** Actions to execute on expiry (nested transitions, variable sets, etc.) */
  onExpire: SceneAction[];
}

/** A variable set action */
export interface VariableSetAction {
  kind: 'variable_set';
  name: string;
  value: string;
}

export type SceneAction = TimerAction | VariableSetAction;

/** A puzzle definition in the scenario */
export interface PuzzleNode {
  id: string;
  name: string;
  type: 'qr' | 'button' | 'sequence' | 'free';
  solution?: string;
  hints: Array<{ level: number; text: string }>;
}

/** An NPC action (say, mood change, hint, react, conversation) */
export interface NPCAction {
  type: 'say' | 'mood' | 'hint' | 'react' | 'conversation';
  text?: string;
  mood?: string;
  level?: number;
  puzzleId?: string;
  condition?: string;
  systemPrompt?: string;
}

/** A hardware action (GPIO, LED, buzzer, audio, QR) */
export interface HardwareAction {
  type: 'gpio_write' | 'gpio_read' | 'led_set' | 'buzzer' | 'play_audio' | 'qr_scan';
  pin?: number;
  state?: 'HIGH' | 'LOW';
  variable?: string;
  color?: string;
  animation?: string;
  frequency?: number;
  duration_ms?: number;
  filename?: string;
}

/** Deploy configuration for ESP32 targets */
export interface DeployConfig {
  wifi?: { ssid: string; password: string };
  tts?: { url: string; voice: string };
  llm?: { url: string; model: string };
}

/** The full scenario graph extracted from the Blockly workspace */
export interface ScenarioGraph {
  scenes: SceneNode[];
  puzzles: PuzzleNode[];
  npcActions: NPCAction[];
  hardwareActions: HardwareAction[];
  deploy: DeployConfig;
}
