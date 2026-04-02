import { describe, it, expect } from 'vitest';
import {
  validateScenarioGraph,
  formatValidationSummary,
} from '../../components/ScenarioEditor/validators/workspace';
import type { ScenarioGraph } from '../../components/ScenarioEditor/types';

function makeGraph(partial: Partial<ScenarioGraph>): ScenarioGraph {
  return {
    scenes: [],
    puzzles: [],
    npcActions: [],
    hardwareActions: [],
    deploy: {},
    ...partial,
  };
}

describe('validateScenarioGraph', () => {
  it('returns error for empty scenario', () => {
    const graph = makeGraph({});
    const issues = validateScenarioGraph(graph);
    expect(issues.some((i) => i.message === 'No scenes defined')).toBe(true);
  });

  it('detects duplicate scene IDs', () => {
    const graph = makeGraph({
      scenes: [
        { name: 'SCENE_1', description: '', durationMax: 300, actions: [], transitions: [] },
        { name: 'SCENE_1', description: '', durationMax: 300, actions: [], transitions: [] },
      ],
    });
    const issues = validateScenarioGraph(graph);
    expect(issues.some((i) => i.message.includes('Duplicate scene ID'))).toBe(true);
  });

  it('detects dangling transition target', () => {
    const graph = makeGraph({
      scenes: [
        {
          name: 'SCENE_1',
          description: '',
          durationMax: 300,
          actions: [],
          transitions: [{ targetScene: 'NONEXISTENT', condition: '' }],
        },
      ],
    });
    const issues = validateScenarioGraph(graph);
    expect(issues.some((i) => i.message.includes('not found'))).toBe(true);
  });

  it('warns about unreachable scenes', () => {
    const graph = makeGraph({
      scenes: [
        { name: 'SCENE_1', description: '', durationMax: 300, actions: [], transitions: [] },
        { name: 'SCENE_2', description: '', durationMax: 300, actions: [], transitions: [] },
      ],
    });
    const issues = validateScenarioGraph(graph);
    expect(issues.some((i) => i.message.includes('unreachable'))).toBe(true);
  });

  it('warns about puzzles without hints', () => {
    const graph = makeGraph({
      scenes: [
        { name: 'SCENE_1', description: '', durationMax: 300, actions: [], transitions: [] },
      ],
      puzzles: [{ id: 'p1', name: 'Test', type: 'qr', hints: [] }],
    });
    const issues = validateScenarioGraph(graph);
    expect(issues.some((i) => i.message.includes('no hints'))).toBe(true);
  });

  it('detects duplicate puzzle IDs', () => {
    const graph = makeGraph({
      scenes: [
        { name: 'SCENE_1', description: '', durationMax: 300, actions: [], transitions: [] },
      ],
      puzzles: [
        { id: 'p1', name: 'A', type: 'qr', hints: [{ level: 1, text: 'h' }] },
        { id: 'p1', name: 'B', type: 'button', hints: [{ level: 1, text: 'h' }] },
      ],
    });
    const issues = validateScenarioGraph(graph);
    expect(issues.some((i) => i.message.includes('Duplicate puzzle ID'))).toBe(true);
  });

  it('returns no errors for valid scenario', () => {
    const graph = makeGraph({
      scenes: [
        {
          name: 'SCENE_INTRO',
          description: 'Intro',
          durationMax: 300,
          actions: [],
          transitions: [{ targetScene: 'SCENE_END', condition: '' }],
        },
        {
          name: 'SCENE_END',
          description: 'End',
          durationMax: 300,
          actions: [],
          transitions: [],
        },
      ],
      puzzles: [{ id: 'p1', name: 'Test', type: 'qr', hints: [{ level: 1, text: 'hint' }] }],
      npcActions: [{ type: 'say', text: 'Hello' }],
    });
    const issues = validateScenarioGraph(graph);
    expect(issues.filter((i) => i.severity === 'error')).toHaveLength(0);
  });
});

describe('formatValidationSummary', () => {
  it('shows Ready for valid graph', () => {
    const graph = makeGraph({
      scenes: [
        { name: 'S', description: '', durationMax: 300, actions: [], transitions: [] },
      ],
    });
    const summary = formatValidationSummary(graph, []);
    expect(summary).toContain('Ready');
    expect(summary).toContain('1 scenes');
  });

  it('shows error count when errors exist', () => {
    const graph = makeGraph({
      scenes: [
        { name: 'S', description: '', durationMax: 300, actions: [], transitions: [] },
      ],
    });
    const issues = [
      { severity: 'error' as const, message: 'bad' },
      { severity: 'warning' as const, message: 'meh' },
    ];
    const summary = formatValidationSummary(graph, issues);
    expect(summary).toContain('1 error');
    expect(summary).toContain('1 warning');
  });

  it('shows warning count when only warnings', () => {
    const graph = makeGraph({
      scenes: [
        { name: 'S', description: '', durationMax: 300, actions: [], transitions: [] },
      ],
    });
    const issues = [{ severity: 'warning' as const, message: 'meh' }];
    const summary = formatValidationSummary(graph, issues);
    expect(summary).toContain('1 warning');
    expect(summary).not.toContain('error');
  });
});
