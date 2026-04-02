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
    {
      kind: 'category',
      name: 'Puzzles',
      colour: '210',
      contents: [
        { kind: 'block', type: 'puzzle_definition' },
        { kind: 'block', type: 'puzzle_condition' },
        { kind: 'block', type: 'puzzle_validation_qr' },
        { kind: 'block', type: 'puzzle_validation_button' },
      ],
    },
    {
      kind: 'category',
      name: 'NPC / Dialogue',
      colour: '160',
      contents: [
        { kind: 'block', type: 'npc_say' },
        { kind: 'block', type: 'npc_mood_set' },
        { kind: 'block', type: 'npc_hint' },
        { kind: 'block', type: 'npc_react' },
        { kind: 'block', type: 'npc_conversation' },
      ],
    },
    {
      kind: 'category',
      name: 'Hardware',
      colour: '30',
      contents: [
        { kind: 'block', type: 'hw_gpio_write' },
        { kind: 'block', type: 'hw_gpio_read' },
        { kind: 'block', type: 'hw_led_set' },
        { kind: 'block', type: 'hw_buzzer_tone' },
        { kind: 'block', type: 'hw_play_audio' },
        { kind: 'block', type: 'hw_qr_scan' },
      ],
    },
    {
      kind: 'category',
      name: 'Deploy',
      colour: '0',
      contents: [
        { kind: 'block', type: 'deploy_config_wifi' },
        { kind: 'block', type: 'deploy_config_tts' },
        { kind: 'block', type: 'deploy_config_llm' },
        { kind: 'block', type: 'deploy_export' },
      ],
    },
  ],
};
