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

/** The full scenario graph extracted from the Blockly workspace */
export interface ScenarioGraph {
  scenes: SceneNode[];
}
