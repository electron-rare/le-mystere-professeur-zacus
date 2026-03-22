import { describe, it, expect } from 'vitest';
import {
  buildScenarioFromBlocks,
  validateScenarioDocument,
  normalizeId,
} from '../lib/scenario';
import type { ScenarioStep, ScenarioDocument } from '../types';

// ─── Helpers ───

function makeValidSteps(overrides?: Partial<ScenarioStep>[]): ScenarioStep[] {
  const defaults: ScenarioStep[] = [
    {
      stepId: 'STEP_A',
      sceneId: 'SCENE_A',
      transitions: [
        {
          eventType: 'button',
          eventName: 'BTN_NEXT',
          targetStepId: 'STEP_B',
          priority: 0,
          afterMs: 0,
        },
      ],
    },
    {
      stepId: 'STEP_B',
      sceneId: 'SCENE_B',
      transitions: [],
    },
  ];
  if (!overrides) return defaults;
  return overrides.map((o, i) => ({ ...defaults[i % defaults.length], ...o }));
}

function makeValidDocument(
  steps?: ScenarioStep[],
  scenarioId = 'TEST_SCENARIO',
): ScenarioDocument {
  return buildScenarioFromBlocks(scenarioId, steps ?? makeValidSteps());
}

// ─── Tests ───

describe('scenario validation', () => {
  it('valid scenario passes validation', () => {
    const doc = makeValidDocument();
    const result = validateScenarioDocument(doc);
    expect(result).toEqual({ ok: true });
  });

  it('missing required field (empty id) fails', () => {
    const doc = makeValidDocument(makeValidSteps(), '');
    // buildScenarioFromBlocks normalizes empty to fallback, so force it
    doc.id = '';
    const result = validateScenarioDocument(doc);
    expect(result.ok).toBe(false);
  });

  it('missing title fails', () => {
    const doc = makeValidDocument();
    doc.title = '';
    const result = validateScenarioDocument(doc);
    expect(result.ok).toBe(false);
  });

  it('duplicate step IDs detected', () => {
    const doc = makeValidDocument();
    // Manually inject duplicate step IDs into firmware
    if (doc.firmware?.steps) {
      doc.firmware.steps.push({ ...doc.firmware.steps[0] });
      doc.firmware.steps_reference_order = doc.firmware.steps.map(
        (s) => s.step_id,
      );
    }
    doc.steps_narrative.push({ ...doc.steps_narrative[0] });
    const result = validateScenarioDocument(doc);
    expect(result.ok).toBe(false);
    expect('error' in result && result.error).toContain('duplicate');
  });

  it('transition target pointing to existing step passes', () => {
    const steps: ScenarioStep[] = [
      {
        stepId: 'STEP_1',
        sceneId: 'SCENE_1',
        transitions: [
          {
            eventType: 'serial',
            eventName: 'BTN_GO',
            targetStepId: 'STEP_2',
            priority: 0,
            afterMs: 0,
          },
        ],
      },
      { stepId: 'STEP_2', sceneId: 'SCENE_2', transitions: [] },
    ];
    const doc = makeValidDocument(steps);
    expect(validateScenarioDocument(doc)).toEqual({ ok: true });
  });

  it('transition target pointing to non-existing step fails', () => {
    const steps: ScenarioStep[] = [
      {
        stepId: 'STEP_1',
        sceneId: 'SCENE_1',
        transitions: [
          {
            eventType: 'serial',
            eventName: 'BTN_GO',
            targetStepId: 'STEP_GHOST',
            priority: 0,
            afterMs: 0,
          },
        ],
      },
    ];
    const doc = makeValidDocument(steps);
    const result = validateScenarioDocument(doc);
    expect(result.ok).toBe(false);
    if (!result.ok) {
      expect(result.error).toContain('STEP_GHOST');
    }
  });

  it('players.min > players.max fails', () => {
    const doc = makeValidDocument();
    doc.players = { min: 20, max: 5 };
    const result = validateScenarioDocument(doc);
    expect(result.ok).toBe(false);
    if (!result.ok) {
      expect(result.error).toContain('players');
    }
  });

  it('empty steps array produces fallback STEP_BOOT', () => {
    const doc = makeValidDocument([]);
    expect(doc.firmware?.steps).toHaveLength(1);
    expect(doc.firmware?.steps?.[0].step_id).toBe('STEP_BOOT');
    expect(validateScenarioDocument(doc)).toEqual({ ok: true });
  });
});

describe('normalizeId', () => {
  it('empty string returns fallback', () => {
    expect(normalizeId('', 'FALLBACK')).toBe('FALLBACK');
  });

  it('whitespace-only returns fallback', () => {
    expect(normalizeId('   ', 'FB')).toBe('FB');
  });

  it('special characters are replaced with underscores', () => {
    expect(normalizeId('hello-world!@#', 'X')).toBe('HELLO_WORLD');
  });

  it('unicode characters are stripped', () => {
    const result = normalizeId('etape_cafe', 'X');
    expect(result).toBe('ETAPE_CAFE');
  });

  it('leading/trailing underscores are removed', () => {
    expect(normalizeId('__test__', 'X')).toBe('TEST');
  });

  it('already valid ID is uppercased', () => {
    expect(normalizeId('step_one', 'X')).toBe('STEP_ONE');
  });
});
