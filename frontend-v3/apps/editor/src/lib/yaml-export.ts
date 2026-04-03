import * as Blockly from 'blockly/core';
import { javascriptGenerator } from 'blockly/javascript';
import { parseScenarioYaml } from '@zacus/shared';

/**
 * Export the current Blockly workspace to a zacus_v3_complete.yaml string.
 * Runs all block generators and wraps the output in a minimal YAML skeleton,
 * then validates by parsing. Throws if invalid.
 */
export function exportWorkspaceToYaml(workspace: Blockly.Workspace): string {
  const fragments = javascriptGenerator.workspaceToCode(workspace);
  const skeleton = buildYamlSkeleton(fragments);
  // Validate by parsing (throws if schema is invalid)
  parseScenarioYaml(skeleton);
  return skeleton;
}

function buildYamlSkeleton(generatedFragments: string): string {
  return `id: zacus_v3_complete
version: "3.1"
title: "Le Mystère du Professeur Zacus — V3 Complete"
duration:
  target_minutes: 60
  min_minutes: 30
  max_minutes: 90
  profiling_minutes: 10
  climax_minutes: 10
  outro_minutes: 5
hardware:
  puzzles: []
scoring:
  base_score: 1000
  time_penalty_per_minute: 10
  hint_penalty: 50
  bonus_fast_completion: 200
npc:
  adaptive_rules: []
phases: []
puzzles:
${generatedFragments}
`;
}
