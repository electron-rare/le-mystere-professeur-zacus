import { describe, it, expect } from 'vitest';
import {
  compileScenarioDocumentToRuntime3,
  validateRuntime3Document,
} from '../lib/runtime3';
import type { ScenarioDocument } from '../types';

// ─── Helpers ───

function makeDocument(overrides?: Partial<ScenarioDocument>): ScenarioDocument {
  return {
    id: 'test-scenario',
    version: 1,
    title: 'Test Scenario',
    players: { min: 2, max: 6 },
    duration: { total_minutes: 45 },
    canon: { introduction: 'Intro', stakes: 'Stakes' },
    stations: [],
    puzzles: [],
    steps_narrative: [{ step_id: 'intro', scene: 'scene1', narrative: 'Start' }],
    firmware: {
      initial_step: 'intro',
      steps: [
        {
          step_id: 'intro',
          screen_scene_id: 'scene_welcome',
          audio_pack_id: 'pack_intro',
          actions: ['play_music'],
          apps: [],
          transitions: [
            {
              event_type: 'button',
              event_name: 'btn_next',
              target_step_id: 'puzzle_1',
              priority: 10,
              after_ms: 0,
            },
            {
              event_type: 'timer',
              event_name: 'timeout',
              target_step_id: 'puzzle_1',
              priority: 5,
              after_ms: 30000,
            },
          ],
        },
        {
          step_id: 'puzzle_1',
          screen_scene_id: 'scene_puzzle',
          audio_pack_id: 'pack_puzzle',
          actions: [],
          apps: ['puzzle_app'],
          transitions: [
            {
              event_type: 'serial',
              event_name: 'solved',
              target_step_id: 'intro',
              priority: 0,
              after_ms: 0,
            },
          ],
        },
      ],
    },
    ...overrides,
  };
}

// ─── Tests ───

describe('compileScenarioDocumentToRuntime3', () => {
  it('produces correct schema_version', () => {
    const result = compileScenarioDocumentToRuntime3(makeDocument());
    expect(result.schema_version).toBe('zacus.runtime3.v1');
  });

  it('sets metadata correctly', () => {
    const result = compileScenarioDocumentToRuntime3(makeDocument());
    expect(result.metadata.generated_by).toBe('frontend-scratch-v2');
    expect(result.metadata.migration_mode).toBe('native');
  });

  it('normalizes step IDs to uppercase', () => {
    const result = compileScenarioDocumentToRuntime3(makeDocument());
    for (const step of result.steps) {
      expect(step.id).toBe(step.id.toUpperCase());
    }
    expect(result.steps[0].id).toBe('INTRO');
    expect(result.steps[1].id).toBe('PUZZLE_1');
  });

  it('normalizes transition target_step_id to uppercase', () => {
    const result = compileScenarioDocumentToRuntime3(makeDocument());
    for (const step of result.steps) {
      for (const t of step.transitions) {
        expect(t.target_step_id).toBe(t.target_step_id.toUpperCase());
      }
    }
  });

  it('sorts transitions are preserved with priority values', () => {
    const result = compileScenarioDocumentToRuntime3(makeDocument());
    const introTransitions = result.steps[0].transitions;
    expect(introTransitions).toHaveLength(2);
    // Priorities are preserved as-is from the source
    expect(introTransitions[0].priority).toBe(10);
    expect(introTransitions[1].priority).toBe(5);
  });

  it('sets entry_step_id correctly from firmware.initial_step', () => {
    const result = compileScenarioDocumentToRuntime3(makeDocument());
    expect(result.scenario.entry_step_id).toBe('INTRO');
  });

  it('falls back to first firmware step if initial_step is missing', () => {
    const doc = makeDocument();
    delete doc.firmware!.initial_step;
    const result = compileScenarioDocumentToRuntime3(doc);
    expect(result.scenario.entry_step_id).toBe('INTRO');
  });

  it('falls back to STEP_BOOT when no steps exist', () => {
    const doc = makeDocument({
      firmware: { steps: [] },
      steps_narrative: [],
    });
    const result = compileScenarioDocumentToRuntime3(doc);
    expect(result.scenario.entry_step_id).toBe('STEP_BOOT');
  });

  it('normalizes scenario id', () => {
    const doc = makeDocument({ id: 'my cool scenario!' });
    const result = compileScenarioDocumentToRuntime3(doc);
    expect(result.scenario.id).toBe('MY_COOL_SCENARIO');
  });

  it('handles unknown event_type by falling back to serial', () => {
    const doc = makeDocument();
    doc.firmware!.steps![0].transitions[0].event_type = 'unknown_event';
    const result = compileScenarioDocumentToRuntime3(doc);
    expect(result.steps[0].transitions[0].event_type).toBe('serial');
  });
});

describe('validateRuntime3Document', () => {
  it('validates a correct document', () => {
    const doc = compileScenarioDocumentToRuntime3(makeDocument());
    const result = validateRuntime3Document(doc);
    expect(result).toEqual({ ok: true });
  });

  it('rejects empty steps', () => {
    const doc = compileScenarioDocumentToRuntime3(
      makeDocument({ firmware: { steps: [] }, steps_narrative: [] }),
    );
    doc.steps = [];
    const result = validateRuntime3Document(doc);
    expect(result).toEqual({ ok: false, error: 'runtime3 requires at least one step' });
  });

  it('rejects duplicate step IDs', () => {
    const doc = compileScenarioDocumentToRuntime3(makeDocument());
    doc.steps.push({ ...doc.steps[0] }); // duplicate
    const result = validateRuntime3Document(doc);
    expect(result.ok).toBe(false);
    expect('error' in result && result.error).toContain('duplicate');
  });
});
