export interface ScenarioStep {
  stepId: string;
  sceneId: string;
  audioPack?: string;
  actions?: string[];
  apps?: string[];
  transitions?: StepTransition[];
}

export interface StepTransition {
  eventType:
    | 'button'
    | 'serial'
    | 'timer'
    | 'audio_done'
    | 'unlock'
    | 'espnow'
    | 'action';
  eventName: string;
  targetStepId: string;
  priority: number;
  afterMs: number;
}

export interface ScenarioDocument {
  id: string;
  version: number;
  title: string;
  players: {
    min: number;
    max: number;
  };
  duration: {
    total_minutes: number;
  };
  canon: {
    introduction: string;
    stakes: string;
  };
  stations: Array<Record<string, unknown>>;
  puzzles: Array<Record<string, unknown>>;
  steps_narrative: Array<{
    step_id: string;
    scene: string;
    narrative: string;
  }>;
  firmware?: {
    initial_step?: string;
    steps_reference_order?: string[];
    steps?: Array<{
      step_id: string;
      screen_scene_id: string;
      audio_pack_id: string;
      actions: string[];
      apps: string[];
      transitions: Array<{
        trigger?: string;
        event_type: string;
        event_name: string;
        target_step_id: string;
        priority: number;
        after_ms: number;
      }>;
    }>;
  };
}
