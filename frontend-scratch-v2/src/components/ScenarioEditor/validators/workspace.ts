import type { ScenarioGraph } from '../types';

export interface ValidationIssue {
  severity: 'error' | 'warning';
  message: string;
  blockId?: string;
}

export function validateScenarioGraph(graph: ScenarioGraph): ValidationIssue[] {
  const issues: ValidationIssue[] = [];

  // Check: at least one scene
  if (graph.scenes.length === 0) {
    issues.push({ severity: 'error', message: 'No scenes defined' });
  }

  // Check: duplicate scene names
  const sceneNames = graph.scenes.map((s) => s.name);
  const dupes = sceneNames.filter((name, i) => sceneNames.indexOf(name) !== i);
  for (const d of new Set(dupes)) {
    issues.push({ severity: 'error', message: `Duplicate scene ID: "${d}"` });
  }

  // Check: transitions point to existing scenes
  for (const scene of graph.scenes) {
    for (const t of scene.transitions) {
      if (!sceneNames.includes(t.targetScene)) {
        issues.push({
          severity: 'error',
          message: `Transition target "${t.targetScene}" not found`,
          blockId: scene.name,
        });
      }
    }
  }

  // Check: unreachable scenes (no incoming transitions, except first)
  if (graph.scenes.length > 1) {
    const targets = new Set(
      graph.scenes.flatMap((s) => s.transitions.map((t) => t.targetScene)),
    );
    for (const scene of graph.scenes.slice(1)) {
      if (!targets.has(scene.name)) {
        issues.push({
          severity: 'warning',
          message: `Scene "${scene.name}" is unreachable (no incoming transitions)`,
          blockId: scene.name,
        });
      }
    }
  }

  // Check: puzzles without hints
  for (const puzzle of graph.puzzles) {
    if (puzzle.hints.length === 0) {
      issues.push({
        severity: 'warning',
        message: `Puzzle "${puzzle.name}" has no hints`,
      });
    }
  }

  // Check: duplicate puzzle IDs
  const puzzleIds = graph.puzzles.map((p) => p.id);
  const puzzleDupes = puzzleIds.filter((id, i) => puzzleIds.indexOf(id) !== i);
  for (const d of new Set(puzzleDupes)) {
    issues.push({ severity: 'error', message: `Duplicate puzzle ID: "${d}"` });
  }

  return issues;
}

export function formatValidationSummary(
  graph: ScenarioGraph,
  issues: ValidationIssue[],
): string {
  const errors = issues.filter((i) => i.severity === 'error').length;
  const warnings = issues.filter((i) => i.severity === 'warning').length;
  const stats = `${graph.scenes.length} scenes, ${graph.puzzles.length} puzzles, ${graph.npcActions.length} NPC lines`;

  if (errors > 0) {
    return `${stats} | ${errors} error${errors > 1 ? 's' : ''}, ${warnings} warning${warnings > 1 ? 's' : ''}`;
  }
  if (warnings > 0) {
    return `${stats} | ${warnings} warning${warnings > 1 ? 's' : ''}`;
  }
  return `${stats} | Ready`;
}
