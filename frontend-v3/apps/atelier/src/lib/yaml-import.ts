import * as Blockly from 'blockly/core';
import { parseScenarioYaml } from '@zacus/shared';
import type { PuzzleConfig } from '@zacus/shared';

/**
 * Import a zacus_v3_complete.yaml string into the Blockly workspace.
 * Clears the existing workspace and recreates all blocks from the scenario.
 */
export function importYamlToWorkspace(workspace: Blockly.Workspace, yaml: string): void {
  const scenario = parseScenarioYaml(yaml);
  workspace.clear();

  let yOffset = 20;

  // Create phase_container blocks
  for (const phase of scenario.phases) {
    const phaseBlock = workspace.newBlock('phase_container') as Blockly.Block;
    phaseBlock.setFieldValue(phase.type, 'PHASE_TYPE');
    initBlock(phaseBlock, 20, yOffset);
    yOffset += 120;

    // Nested puzzle_selector blocks
    let prevBlock: Blockly.Block | null = null;
    for (const p of phase.puzzles) {
      const selBlock = workspace.newBlock('puzzle_selector') as Blockly.Block;
      selBlock.setFieldValue(p.puzzle_ref, 'PUZZLE_REF');
      selBlock.setFieldValue(p.profile_filter, 'PROFILE_FILTER');
      initBlock(selBlock, 0, 0);
      if (prevBlock === null) {
        phaseBlock.getInput('PUZZLES')?.connection?.connect(selBlock.previousConnection!);
      } else {
        prevBlock.nextConnection?.connect(selBlock.previousConnection!);
      }
      prevBlock = selBlock;
    }
  }

  // Create puzzle config blocks in a second column
  let xOffset = 420;
  yOffset = 20;
  for (const puzzle of scenario.puzzles) {
    const block = createPuzzleBlock(workspace, puzzle);
    if (block) {
      initBlock(block, xOffset, yOffset);
      yOffset += 80;
    }
  }

  if (workspace instanceof Blockly.WorkspaceSvg) {
    Blockly.svgResize(workspace);
  }
}

function initBlock(block: Blockly.Block, x: number, y: number): void {
  if (typeof (block as Blockly.BlockSvg).initSvg === 'function') {
    (block as Blockly.BlockSvg).initSvg();
  }
  if (typeof (block as Blockly.BlockSvg).render === 'function') {
    (block as Blockly.BlockSvg).render();
  }
  if (x !== 0 || y !== 0) {
    (block as Blockly.BlockSvg).moveBy(x, y);
  }
}

const PUZZLE_TYPE_MAP: Record<string, string> = {
  P1_SON: 'puzzle_sequence_sonore',
  P2_CIRCUIT: 'puzzle_circuit_led',
  P3_QR: 'puzzle_qr_treasure',
  P4_RADIO: 'puzzle_radio',
  P5_MORSE: 'puzzle_morse',
  P6_SYMBOLES: 'puzzle_symboles_nfc',
  P7_COFFRE: 'puzzle_coffre_final',
};

function createPuzzleBlock(workspace: Blockly.Workspace, puzzle: PuzzleConfig): Blockly.Block | null {
  const blockType = PUZZLE_TYPE_MAP[puzzle.id];
  if (!blockType) return null;

  const block = workspace.newBlock(blockType) as Blockly.Block;

  switch (puzzle.id) {
    case 'P1_SON':
      block.setFieldValue(puzzle.melody_notes?.join(',') ?? '', 'MELODY_NOTES');
      block.setFieldValue(String(puzzle.difficulty ?? 1), 'DIFFICULTY');
      block.setFieldValue(puzzle.code_digits?.join(',') ?? '', 'CODE_DIGITS');
      break;
    case 'P2_CIRCUIT':
      block.setFieldValue(puzzle.components?.join(',') ?? '', 'COMPONENTS');
      block.setFieldValue(puzzle.valid_circuit ?? '', 'VALID_CIRCUIT');
      block.setFieldValue(String(puzzle.code_digit ?? 0), 'CODE_DIGIT');
      break;
    case 'P3_QR':
      block.setFieldValue(puzzle.qr_codes?.join(',') ?? '', 'QR_CODES');
      block.setFieldValue(puzzle.correct_order?.join(',') ?? '', 'CORRECT_ORDER');
      block.setFieldValue(String(puzzle.code_digit ?? 0), 'CODE_DIGIT');
      break;
    case 'P4_RADIO':
      block.setFieldValue(String(puzzle.target_freq ?? 93.5), 'TARGET_FREQ');
      block.setFieldValue(String(puzzle.range_min ?? 93.0), 'RANGE_MIN');
      block.setFieldValue(String(puzzle.range_max ?? 94.0), 'RANGE_MAX');
      block.setFieldValue(String(puzzle.code_digit ?? 0), 'CODE_DIGIT');
      break;
    case 'P5_MORSE':
      block.setFieldValue(puzzle.message ?? 'SOS', 'MESSAGE');
      block.setFieldValue(puzzle.mode ?? 'tech', 'MODE');
      block.setFieldValue(puzzle.code_digits?.join(',') ?? '', 'CODE_DIGITS');
      break;
    case 'P6_SYMBOLES':
      block.setFieldValue(puzzle.symbols?.join(',') ?? '', 'SYMBOLS');
      block.setFieldValue(puzzle.correct_order?.join(',') ?? '', 'CORRECT_ORDER');
      block.setFieldValue(puzzle.code_digits?.join(',') ?? '', 'CODE_DIGITS');
      break;
    case 'P7_COFFRE':
      block.setFieldValue(String(puzzle.code_length ?? 8), 'CODE_LENGTH');
      block.setFieldValue(String(puzzle.max_attempts ?? 3), 'MAX_ATTEMPTS');
      break;
  }

  return block;
}
