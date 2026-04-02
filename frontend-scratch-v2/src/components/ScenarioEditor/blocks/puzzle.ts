import * as Blockly from 'blockly';

let registered = false;

export function registerPuzzleBlocks(): void {
  if (registered) return;

  Blockly.defineBlocksWithJsonArray([
    {
      type: 'puzzle_definition',
      message0: 'Puzzle %1 type %2',
      args0: [
        { type: 'field_input', name: 'NAME', text: 'PUZZLE_NEW' },
        {
          type: 'field_dropdown',
          name: 'PUZZLE_TYPE',
          options: [
            ['QR', 'qr'],
            ['Button', 'button'],
            ['Sequence', 'sequence'],
            ['Free', 'free'],
          ],
        },
      ],
      message1: 'solution %1',
      args1: [{ type: 'input_value', name: 'SOLUTION' }],
      message2: 'hints %1',
      args2: [{ type: 'input_statement', name: 'HINTS' }],
      colour: 210,
      tooltip: 'Define a puzzle with a type, solution, and hints',
    },
    {
      type: 'puzzle_condition',
      message0: '%1 %2',
      args0: [
        {
          type: 'field_dropdown',
          name: 'CONDITION_TYPE',
          options: [
            ['puzzle solved', 'puzzle_solved'],
            ['timer expired', 'timer_expired'],
            ['variable equals', 'variable_equals'],
          ],
        },
        { type: 'field_input', name: 'REFERENCE', text: '' },
      ],
      output: 'Boolean',
      colour: 210,
      tooltip: 'A condition that evaluates to true/false',
    },
    {
      type: 'puzzle_validation_qr',
      message0: 'QR expected %1',
      args0: [
        { type: 'field_input', name: 'EXPECTED', text: 'ZACUS_KEY_1' },
      ],
      output: null,
      colour: 210,
      tooltip: 'QR code validation — matches scanned value against expected',
    },
    {
      type: 'puzzle_validation_button',
      message0: 'Button pin %1',
      args0: [
        { type: 'field_number', name: 'PIN', value: 4, min: 0, precision: 1 },
      ],
      output: null,
      colour: 210,
      tooltip: 'Button press validation on a specific GPIO pin',
    },
  ]);

  registered = true;
}
