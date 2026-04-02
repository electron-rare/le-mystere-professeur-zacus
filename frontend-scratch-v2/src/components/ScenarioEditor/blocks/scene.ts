import * as Blockly from 'blockly';

let registered = false;

export function ensureScenarioBlocks(): void {
  if (registered) return;

  Blockly.defineBlocksWithJsonArray([
    {
      type: 'scenario_scene',
      message0: 'Scene %1 description %2',
      args0: [
        { type: 'field_input', name: 'NAME', text: 'SCENE_NEW' },
        { type: 'field_input', name: 'DESCRIPTION', text: '' },
      ],
      message1: 'duration max %1 s',
      args1: [
        { type: 'field_number', name: 'DURATION_MAX', value: 300, min: 0, precision: 1 },
      ],
      message2: 'actions %1',
      args2: [{ type: 'input_statement', name: 'ACTIONS' }],
      message3: 'transitions %1',
      args3: [{ type: 'input_statement', name: 'TRANSITIONS' }],
      colour: 270,
      tooltip: 'A scenario scene with actions and transitions',
    },
    {
      type: 'scenario_transition',
      message0: 'go to %1 when %2',
      args0: [
        { type: 'field_input', name: 'TARGET_SCENE', text: 'SCENE_NEXT' },
        { type: 'input_value', name: 'CONDITION' },
      ],
      previousStatement: null,
      nextStatement: null,
      colour: 160,
      tooltip: 'Transition to another scene when condition is met',
    },
    {
      type: 'scenario_timer',
      message0: 'timer %1 s',
      args0: [
        { type: 'field_number', name: 'SECONDS', value: 10, min: 0, precision: 1 },
      ],
      message1: 'on expire %1',
      args1: [{ type: 'input_statement', name: 'ON_EXPIRE' }],
      previousStatement: null,
      nextStatement: null,
      colour: 60,
      tooltip: 'Wait for a duration then execute actions',
    },
    {
      type: 'scenario_variable_set',
      message0: 'set %1 to %2',
      args0: [
        { type: 'field_input', name: 'NAME', text: 'my_var' },
        { type: 'input_value', name: 'VALUE' },
      ],
      previousStatement: null,
      nextStatement: null,
      colour: 330,
      tooltip: 'Set a scenario variable',
    },
    {
      type: 'scenario_variable_get',
      message0: 'get %1',
      args0: [
        { type: 'field_input', name: 'NAME', text: 'my_var' },
      ],
      output: null,
      colour: 330,
      tooltip: 'Get a scenario variable value',
    },
  ]);

  registered = true;
}
