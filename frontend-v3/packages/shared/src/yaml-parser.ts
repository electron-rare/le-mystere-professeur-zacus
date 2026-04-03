import yaml from 'js-yaml';
import type { ScenarioYaml } from './types.js';

/**
 * Parse a zacus_v3_complete.yaml string into a typed ScenarioYaml object.
 * Throws if the YAML is malformed or missing required top-level keys.
 */
export function parseScenarioYaml(raw: string): ScenarioYaml {
  const doc = yaml.load(raw);
  if (!doc || typeof doc !== 'object') {
    throw new Error('Invalid YAML: expected object at root');
  }
  const scenario = doc as Record<string, unknown>;
  const required = ['id', 'version', 'duration', 'hardware', 'scoring', 'phases', 'puzzles'];
  for (const key of required) {
    if (!(key in scenario)) {
      throw new Error(`Invalid scenario YAML: missing key "${key}"`);
    }
  }
  return scenario as unknown as ScenarioYaml;
}

/**
 * Serialize a ScenarioYaml back to YAML string.
 * Used by the Blockly editor YAML export.
 */
export function serializeScenarioYaml(scenario: ScenarioYaml): string {
  return yaml.dump(scenario, { lineWidth: 120, noRefs: true });
}
