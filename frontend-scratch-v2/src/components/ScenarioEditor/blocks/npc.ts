import * as Blockly from 'blockly';

let registered = false;

const MOOD_OPTIONS: Array<[string, string]> = [
  ['neutral', 'neutral'],
  ['impressed', 'impressed'],
  ['worried', 'worried'],
  ['amused', 'amused'],
];

export function registerNpcBlocks(): void {
  if (registered) return;

  Blockly.defineBlocksWithJsonArray([
    {
      type: 'npc_say',
      message0: 'NPC says %1 mood %2',
      args0: [
        { type: 'field_input', name: 'TEXT', text: '' },
        {
          type: 'field_dropdown',
          name: 'MOOD',
          options: MOOD_OPTIONS,
        },
      ],
      previousStatement: null,
      nextStatement: null,
      colour: 160,
      tooltip: 'Professor Zacus says something with a mood',
    },
    {
      type: 'npc_mood_set',
      message0: 'set NPC mood %1',
      args0: [
        {
          type: 'field_dropdown',
          name: 'MOOD',
          options: MOOD_OPTIONS,
        },
      ],
      previousStatement: null,
      nextStatement: null,
      colour: 160,
      tooltip: 'Change the NPC mood state',
    },
    {
      type: 'npc_hint',
      message0: 'hint level %1 for puzzle %2 text %3',
      args0: [
        {
          type: 'field_dropdown',
          name: 'LEVEL',
          options: [
            ['1', '1'],
            ['2', '2'],
            ['3', '3'],
          ],
        },
        { type: 'field_input', name: 'PUZZLE_ID', text: '' },
        { type: 'field_input', name: 'TEXT', text: '' },
      ],
      previousStatement: null,
      nextStatement: null,
      colour: 160,
      tooltip: 'A hint for a specific puzzle at a given difficulty level',
    },
    {
      type: 'npc_react',
      message0: 'NPC reacts to %1 with %2',
      args0: [
        { type: 'input_value', name: 'CONDITION' },
        { type: 'field_input', name: 'RESPONSE', text: '' },
      ],
      previousStatement: null,
      nextStatement: null,
      colour: 160,
      tooltip: 'NPC reacts when a condition is met',
    },
    {
      type: 'npc_conversation',
      message0: 'conversation prompt %1 context %2',
      args0: [
        { type: 'field_input', name: 'SYSTEM_PROMPT', text: '' },
        { type: 'field_input', name: 'CONTEXT', text: '' },
      ],
      output: null,
      colour: 160,
      tooltip: 'An LLM-powered NPC conversation with system prompt and context',
    },
  ]);

  registered = true;
}
