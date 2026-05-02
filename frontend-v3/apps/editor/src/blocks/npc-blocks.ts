import * as Blockly from 'blockly/core';

// 3 NPC block definitions: profiling, duration, adaptive rule

export const NPC_BLOCK_DEFS = [
  {
    type: 'npc_profiling',
    message0: 'NPC Profiling | seuil tech %1 s | seuil non-tech %2 s',
    args0: [
      { type: 'field_number', name: 'TECH_THRESHOLD_S', value: 300, min: 60, max: 600 },
      { type: 'field_number', name: 'NONTECH_THRESHOLD_S', value: 480, min: 60, max: 900 },
    ],
    colour: '#5B4FD6',
    tooltip: 'Configure la logique de profilage du groupe par le NPC',
    previousStatement: null,
    nextStatement: null,
  },
  {
    type: 'npc_duration',
    message0: 'NPC Durée | cible %1 min | mode %2',
    args0: [
      { type: 'field_number', name: 'TARGET_MINUTES', value: 60, min: 30, max: 90 },
      {
        type: 'field_dropdown',
        name: 'MODE',
        options: [
          ['30 min', '30'],
          ['45 min', '45'],
          ['60 min', '60'],
          ['90 min', '90'],
        ],
      },
    ],
    colour: '#5B4FD6',
    tooltip: 'Configure la durée cible de la session',
    previousStatement: null,
    nextStatement: null,
  },
  {
    type: 'npc_adaptive_rule',
    message0: 'NPC Règle | si %1 → %2',
    args0: [
      {
        type: 'field_dropdown',
        name: 'CONDITION',
        options: [
          ['trop rapide', 'fast'],
          ['trop lent', 'slow'],
        ],
      },
      {
        type: 'field_dropdown',
        name: 'ACTION',
        options: [
          ['ajouter puzzle', 'add'],
          ['passer puzzle', 'skip'],
          ['donner indice', 'hint'],
        ],
      },
    ],
    colour: '#5B4FD6',
    tooltip: 'Règle adaptative NPC (ajustement en cours de jeu)',
    previousStatement: null,
    nextStatement: null,
  },
] as const;

export function registerNpcBlocks(): void {
  for (const def of NPC_BLOCK_DEFS) {
    Blockly.Blocks[def.type] = {
      init(this: Blockly.Block) {
        this.jsonInit(def);
      },
    };
  }
}

// eslint-disable-next-line @typescript-eslint/no-explicit-any
export function registerNpcGenerators(gen: any): void {
  gen['npc_profiling'] = (block: Blockly.Block) => {
    const tech = block.getFieldValue('TECH_THRESHOLD_S') as string;
    const nontech = block.getFieldValue('NONTECH_THRESHOLD_S') as string;
    return `\n  profiling:\n    tech_threshold_s: ${tech}\n    nontech_threshold_s: ${nontech}`;
  };

  gen['npc_duration'] = (block: Blockly.Block) => {
    const target = block.getFieldValue('TARGET_MINUTES') as string;
    const mode = block.getFieldValue('MODE') as string;
    return `\n  duration:\n    target_minutes: ${target}\n    mode: "${mode}"`;
  };

  gen['npc_adaptive_rule'] = (block: Blockly.Block) => {
    const cond = block.getFieldValue('CONDITION') as string;
    const action = block.getFieldValue('ACTION') as string;
    return `\n  - condition: ${cond}\n    action: ${action}`;
  };
}
