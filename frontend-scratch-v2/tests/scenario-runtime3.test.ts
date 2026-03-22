import { describe, expect, it } from 'vitest';
import YAML from 'yaml';
import { buildScenarioFromBlocks, parseYamlToSteps, validateScenarioDocument } from '../src/lib/scenario';
import { compileScenarioDocumentToRuntime3, validateRuntime3Document } from '../src/lib/runtime3';
import type { ScenarioDocument } from '../src/types';

function buildGraphFirstDocument(): ScenarioDocument {
  return {
    id: 'zacus_graph_first',
    version: 3,
    title: 'Zacus Graph First',
    players: { min: 6, max: 12 },
    duration: { total_minutes: 90 },
    canon: {
      introduction: 'Intro',
      stakes: 'Stakes',
    },
    stations: [],
    puzzles: [],
    steps_narrative: [
      { step_id: 'STEP_BOOT', scene: 'SCENE_BOOT', narrative: 'Boot' },
      { step_id: 'STEP_GATE', scene: 'SCENE_GATE', narrative: 'Gate' },
      { step_id: 'STEP_DONE', scene: 'SCENE_DONE', narrative: 'Done' },
    ],
    firmware: {
      initial_step: 'STEP_BOOT',
      steps: [
        {
          step_id: 'STEP_BOOT',
          screen_scene_id: 'SCENE_BOOT',
          audio_pack_id: '',
          actions: [],
          apps: [],
          transitions: [
            {
              event_type: 'serial',
              event_name: 'BTN_NEXT',
              target_step_id: 'STEP_GATE',
              priority: 100,
              after_ms: 0,
            },
          ],
        },
        {
          step_id: 'STEP_GATE',
          screen_scene_id: 'SCENE_GATE',
          audio_pack_id: '',
          actions: [],
          apps: [],
          transitions: [
            {
              event_type: 'unlock',
              event_name: 'UNLOCK_GATE',
              target_step_id: 'STEP_DONE',
              priority: 120,
              after_ms: 0,
            },
          ],
        },
        {
          step_id: 'STEP_DONE',
          screen_scene_id: 'SCENE_DONE',
          audio_pack_id: '',
          actions: [],
          apps: [],
          transitions: [],
        },
      ],
    },
  };
}

describe('graph-first scenario handling', () => {
  it('validates a canonical graph-first document without steps_reference_order', () => {
    const document = buildGraphFirstDocument();
    expect(validateScenarioDocument(document)).toEqual({ ok: true });
  });

  it('parses firmware.steps from YAML before older fallback fields', () => {
    const yaml = YAML.stringify(buildGraphFirstDocument());
    const parsed = parseYamlToSteps(yaml);
    if ('error' in parsed) {
      throw new Error(parsed.error);
    }
    expect(parsed.steps.map((step) => step.stepId)).toEqual([
      'STEP_BOOT',
      'STEP_GATE',
      'STEP_DONE',
    ]);
    expect(parsed.steps[1]?.transitions?.[0]?.eventType).toBe('unlock');
  });

  it('compiles runtime3 entry step from firmware.initial_step', () => {
    const runtime3 = compileScenarioDocumentToRuntime3(buildGraphFirstDocument());
    expect(runtime3.scenario.entry_step_id).toBe('STEP_BOOT');
    expect(runtime3.steps[1]?.transitions[0]?.event_type).toBe('unlock');
    expect(validateRuntime3Document(runtime3)).toEqual({ ok: true });
  });

  it('keeps the builder output compatible with the graph-first validator', () => {
    const built = buildScenarioFromBlocks('zacus_builder', [
      {
        stepId: 'STEP_BOOT',
        sceneId: 'SCENE_BOOT',
        transitions: [
          {
            eventType: 'serial',
            eventName: 'BTN_NEXT',
            targetStepId: 'STEP_DONE',
            priority: 50,
            afterMs: 0,
          },
        ],
      },
      {
        stepId: 'STEP_DONE',
        sceneId: 'SCENE_DONE',
        transitions: [],
      },
    ]);
    expect(validateScenarioDocument(built)).toEqual({ ok: true });
    expect(built.firmware?.steps?.[0]?.step_id).toBe('STEP_BOOT');
    expect(built.firmware?.steps_reference_order).toEqual(['STEP_BOOT', 'STEP_DONE']);
    expect('steps_reference_order' in built).toBe(false);
  });
});
