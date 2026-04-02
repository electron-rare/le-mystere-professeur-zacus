import type * as Blockly from 'blockly';

export const SCENARIO_TOOLBOX: Blockly.utils.toolbox.ToolboxInfo = {
  kind: 'categoryToolbox',
  contents: [
    {
      kind: 'category',
      name: 'Scenario',
      colour: '270',
      contents: [
        { kind: 'block', type: 'scenario_scene' },
        { kind: 'block', type: 'scenario_transition' },
        { kind: 'block', type: 'scenario_timer' },
        { kind: 'block', type: 'scenario_variable_set' },
        { kind: 'block', type: 'scenario_variable_get' },
      ],
    },
  ],
};
