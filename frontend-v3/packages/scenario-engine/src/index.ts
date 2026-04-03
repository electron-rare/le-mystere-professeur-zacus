export { ZacusScenarioEngine } from './engine.js';
export { detectGroupProfile, profileConfidence } from './profiler.js';
export { computeScore, assembleCode } from './scorer.js';
// Re-export the interface so consumers can type-check against it
export type { ScenarioEngine } from '@zacus/shared';
