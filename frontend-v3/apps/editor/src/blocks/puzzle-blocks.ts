import * as Blockly from 'blockly/core';

// 7 puzzle block definitions with JSON defs.
// YAML generators are in ../yaml/generators.ts

export const PUZZLE_BLOCK_DEFS = [
  {
    type: 'puzzle_sequence_sonore',
    message0: 'P1 Séquence Sonore | notes %1 | difficulté %2 | code digits %3',
    args0: [
      { type: 'field_input', name: 'MELODY_NOTES', text: 'do,re,mi,fa' },
      { type: 'field_number', name: 'DIFFICULTY', value: 1, min: 1, max: 5 },
      { type: 'field_input', name: 'CODE_DIGITS', text: '1,2' },
    ],
    colour: '#FF6B35',
    tooltip: 'P1: Séquence Sonore — joueur reproduit une mélodie',
    previousStatement: null,
    nextStatement: null,
  },
  {
    type: 'puzzle_circuit_led',
    message0: 'P2 Circuit LED | composants %1 | circuit valide %2 | code digit %3',
    args0: [
      { type: 'field_input', name: 'COMPONENTS', text: 'resistor,led,capacitor' },
      { type: 'field_input', name: 'VALID_CIRCUIT', text: 'series_parallel' },
      { type: 'field_number', name: 'CODE_DIGIT', value: 3, min: 0, max: 9 },
    ],
    colour: '#FF6B35',
    tooltip: 'P2: Circuit LED — joueur câble un circuit électronique',
    previousStatement: null,
    nextStatement: null,
  },
  {
    type: 'puzzle_qr_treasure',
    message0: 'P3 QR Treasure | QR codes %1 | ordre correct %2 | code digit %3',
    args0: [
      { type: 'field_input', name: 'QR_CODES', text: 'qr1,qr2,qr3,qr4,qr5,qr6' },
      { type: 'field_input', name: 'CORRECT_ORDER', text: '3,1,4,1,5,9' },
      { type: 'field_number', name: 'CODE_DIGIT', value: 5, min: 0, max: 9 },
    ],
    colour: '#FF6B35',
    tooltip: 'P3: QR Treasure — scanner 6 QR codes dans le bon ordre',
    previousStatement: null,
    nextStatement: null,
  },
  {
    type: 'puzzle_radio',
    message0: 'P4 Radio | fréquence cible %1 | min %2 | max %3 | code digit %4',
    args0: [
      { type: 'field_number', name: 'TARGET_FREQ', value: 93.5, min: 88, max: 108 },
      { type: 'field_number', name: 'RANGE_MIN', value: 93.0 },
      { type: 'field_number', name: 'RANGE_MAX', value: 94.0 },
      { type: 'field_number', name: 'CODE_DIGIT', value: 7, min: 0, max: 9 },
    ],
    colour: '#FF6B35',
    tooltip: 'P4: Fréquence Radio — syntoniser une fréquence précise',
    previousStatement: null,
    nextStatement: null,
  },
  {
    type: 'puzzle_morse',
    message0: 'P5 Morse | message %1 | mode %2 | code digits %3',
    args0: [
      { type: 'field_input', name: 'MESSAGE', text: 'SOS' },
      {
        type: 'field_dropdown',
        name: 'MODE',
        options: [
          ['tech (clé télégraphe)', 'tech'],
          ['light (lampe)', 'light'],
        ],
      },
      { type: 'field_input', name: 'CODE_DIGITS', text: '4,2' },
    ],
    colour: '#FF6B35',
    tooltip: 'P5: Code Morse — transmettre un message en morse',
    previousStatement: null,
    nextStatement: null,
  },
  {
    type: 'puzzle_symboles_nfc',
    message0: 'P6 Symboles NFC | symboles %1 | ordre correct %2 | code digits %3',
    args0: [
      {
        type: 'field_input',
        name: 'SYMBOLS',
        text: 'alpha,beta,gamma,delta,sigma,omega,phi,psi,chi,rho,mu,nu',
      },
      { type: 'field_input', name: 'CORRECT_ORDER', text: '1,4,7,2' },
      { type: 'field_input', name: 'CODE_DIGITS', text: '6,1' },
    ],
    colour: '#FF6B35',
    tooltip: 'P6: Symboles Alchimiques NFC — placer des tuiles dans le bon ordre',
    previousStatement: null,
    nextStatement: null,
  },
  {
    type: 'puzzle_coffre_final',
    message0: 'P7 Coffre Final | longueur code %1 | max tentatives %2',
    args0: [
      { type: 'field_number', name: 'CODE_LENGTH', value: 8, min: 4, max: 12 },
      { type: 'field_number', name: 'MAX_ATTEMPTS', value: 3, min: 1, max: 10 },
    ],
    colour: '#FF6B35',
    tooltip: 'P7: Coffre Final — entrer le code assemblé depuis tous les puzzles',
    previousStatement: null,
    nextStatement: null,
  },
] as const;

export function registerPuzzleBlocks(): void {
  for (const def of PUZZLE_BLOCK_DEFS) {
    Blockly.Blocks[def.type] = {
      init(this: Blockly.Block) {
        this.jsonInit(def);
      },
    };
  }
}

// YAML generators for puzzle blocks
export function registerPuzzleGenerators(
  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  gen: any,
): void {
  gen['puzzle_sequence_sonore'] = (block: Blockly.Block) => {
    const notes = block.getFieldValue('MELODY_NOTES') as string;
    const diff = block.getFieldValue('DIFFICULTY') as string;
    const digits = block.getFieldValue('CODE_DIGITS') as string;
    return `\n  - id: P1_SON\n    name: "Séquence Sonore"\n    melody_notes: [${notes
      .split(',')
      .map((n) => `"${n.trim()}"`)
      .join(', ')}]\n    difficulty: ${diff}\n    code_digits: [${digits}]`;
  };

  gen['puzzle_circuit_led'] = (block: Blockly.Block) => {
    const components = block.getFieldValue('COMPONENTS') as string;
    const circuit = block.getFieldValue('VALID_CIRCUIT') as string;
    const digit = block.getFieldValue('CODE_DIGIT') as string;
    return `\n  - id: P2_CIRCUIT\n    name: "Circuit LED"\n    components: [${components
      .split(',')
      .map((c) => `"${c.trim()}"`)
      .join(', ')}]\n    valid_circuit: "${circuit}"\n    code_digit: ${digit}`;
  };

  gen['puzzle_qr_treasure'] = (block: Blockly.Block) => {
    const qrs = block.getFieldValue('QR_CODES') as string;
    const order = block.getFieldValue('CORRECT_ORDER') as string;
    const digit = block.getFieldValue('CODE_DIGIT') as string;
    return `\n  - id: P3_QR\n    name: "QR Treasure"\n    qr_codes: [${qrs
      .split(',')
      .map((q) => `"${q.trim()}"`)
      .join(', ')}]\n    correct_order: [${order}]\n    code_digit: ${digit}`;
  };

  gen['puzzle_radio'] = (block: Blockly.Block) => {
    const freq = block.getFieldValue('TARGET_FREQ') as string;
    const min = block.getFieldValue('RANGE_MIN') as string;
    const max = block.getFieldValue('RANGE_MAX') as string;
    const digit = block.getFieldValue('CODE_DIGIT') as string;
    return `\n  - id: P4_RADIO\n    name: "Fréquence Radio"\n    target_freq: ${freq}\n    range_min: ${min}\n    range_max: ${max}\n    code_digit: ${digit}`;
  };

  gen['puzzle_morse'] = (block: Blockly.Block) => {
    const msg = block.getFieldValue('MESSAGE') as string;
    const mode = block.getFieldValue('MODE') as string;
    const digits = block.getFieldValue('CODE_DIGITS') as string;
    return `\n  - id: P5_MORSE\n    name: "Code Morse"\n    message: "${msg}"\n    mode: ${mode}\n    code_digits: [${digits}]`;
  };

  gen['puzzle_symboles_nfc'] = (block: Blockly.Block) => {
    const symbols = block.getFieldValue('SYMBOLS') as string;
    const order = block.getFieldValue('CORRECT_ORDER') as string;
    const digits = block.getFieldValue('CODE_DIGITS') as string;
    return `\n  - id: P6_SYMBOLES\n    name: "Symboles Alchimiques"\n    symbols: [${symbols
      .split(',')
      .map((s) => `"${s.trim()}"`)
      .join(', ')}]\n    correct_order: [${order}]\n    code_digits: [${digits}]`;
  };

  gen['puzzle_coffre_final'] = (block: Blockly.Block) => {
    const len = block.getFieldValue('CODE_LENGTH') as string;
    const attempts = block.getFieldValue('MAX_ATTEMPTS') as string;
    return `\n  - id: P7_COFFRE\n    name: "Coffre Final"\n    code_length: ${len}\n    max_attempts: ${attempts}`;
  };
}
