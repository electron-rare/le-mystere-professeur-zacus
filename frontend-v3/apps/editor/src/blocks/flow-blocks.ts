import * as Blockly from 'blockly/core';
import { javascriptGenerator } from 'blockly/javascript';

// 2 flow block definitions: phase container and puzzle selector

export const FLOW_BLOCK_DEFS = [
  {
    type: 'phase_container',
    message0: 'PHASE %1',
    args0: [
      {
        type: 'field_dropdown',
        name: 'PHASE_TYPE',
        options: [
          ['INTRO', 'INTRO'],
          ['PROFILING', 'PROFILING'],
          ['ADAPTIVE', 'ADAPTIVE'],
          ['CLIMAX', 'CLIMAX'],
          ['OUTRO', 'OUTRO'],
        ],
      },
    ],
    message1: '%1',
    args1: [{ type: 'input_statement', name: 'PUZZLES' }],
    colour: '#2ECC71',
    tooltip: 'Conteneur de phase du scénario',
    previousStatement: null,
    nextStatement: null,
  },
  {
    type: 'puzzle_selector',
    message0: 'Puzzle %1 | profil %2',
    args0: [
      {
        type: 'field_dropdown',
        name: 'PUZZLE_REF',
        options: [
          ['P1 Séquence Sonore', 'P1_SON'],
          ['P2 Circuit LED', 'P2_CIRCUIT'],
          ['P3 QR Treasure', 'P3_QR'],
          ['P4 Radio', 'P4_RADIO'],
          ['P5 Morse', 'P5_MORSE'],
          ['P6 Symboles NFC', 'P6_SYMBOLES'],
          ['P7 Coffre Final', 'P7_COFFRE'],
        ],
      },
      {
        type: 'field_dropdown',
        name: 'PROFILE_FILTER',
        options: [
          ['tous (BOTH)', 'BOTH'],
          ['tech', 'TECH'],
          ['non-tech', 'NON_TECH'],
        ],
      },
    ],
    colour: '#2ECC71',
    tooltip: 'Référence à un puzzle dans une phase',
    previousStatement: null,
    nextStatement: null,
  },
] as const;

export function registerFlowBlocks(): void {
  for (const def of FLOW_BLOCK_DEFS) {
    Blockly.Blocks[def.type] = {
      init(this: Blockly.Block) {
        this.jsonInit(def as Blockly.blocks.BlockDefinition);
      },
    };
  }
}

// eslint-disable-next-line @typescript-eslint/no-explicit-any
export function registerFlowGenerators(gen: any): void {
  gen['phase_container'] = (block: Blockly.Block) => {
    const phaseType = block.getFieldValue('PHASE_TYPE') as string;
    const inner = javascriptGenerator.statementToCode(block, 'PUZZLES');
    return `\n- type: ${phaseType}\n  puzzles:${inner || '\n    []'}`;
  };

  gen['puzzle_selector'] = (block: Blockly.Block) => {
    const ref = block.getFieldValue('PUZZLE_REF') as string;
    const filter = block.getFieldValue('PROFILE_FILTER') as string;
    return `\n  - puzzle_ref: ${ref}\n    profile_filter: ${filter}`;
  };
}
