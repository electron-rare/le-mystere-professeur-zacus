export interface ScenarioStep {
  stepId: string;
  sceneId: string;
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
  steps_reference_order: string[];
  steps_narrative: Array<{
    step_id: string;
    scene: string;
    narrative: string;
  }>;
}
