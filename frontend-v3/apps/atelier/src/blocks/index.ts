import { javascriptGenerator } from 'blockly/javascript';
import { registerPuzzleBlocks, registerPuzzleGenerators } from './puzzle-blocks.js';
import { registerNpcBlocks, registerNpcGenerators } from './npc-blocks.js';
import { registerFlowBlocks, registerFlowGenerators } from './flow-blocks.js';

let registered = false;

/**
 * Register all 12 Blockly block definitions and their YAML generators.
 * Safe to call multiple times — only registers once.
 */
export function registerAllBlocks(): void {
  if (registered) return;
  registered = true;

  // Register block shapes
  registerPuzzleBlocks();
  registerNpcBlocks();
  registerFlowBlocks();

  // Register YAML generators (use javascriptGenerator as the vehicle)
  registerPuzzleGenerators(javascriptGenerator);
  registerNpcGenerators(javascriptGenerator);
  registerFlowGenerators(javascriptGenerator);
}

// Re-export individual registrars for targeted use
export { registerPuzzleBlocks, registerPuzzleGenerators } from './puzzle-blocks.js';
export { registerNpcBlocks, registerNpcGenerators } from './npc-blocks.js';
export { registerFlowBlocks, registerFlowGenerators } from './flow-blocks.js';
