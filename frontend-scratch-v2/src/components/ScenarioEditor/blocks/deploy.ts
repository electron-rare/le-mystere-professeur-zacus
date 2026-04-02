import * as Blockly from 'blockly';

let registered = false;

export function registerDeployBlocks(): void {
  if (registered) return;

  Blockly.defineBlocksWithJsonArray([
    {
      type: 'deploy_config_wifi',
      message0: 'WiFi SSID %1 password %2',
      args0: [
        { type: 'field_input', name: 'SSID', text: '' },
        { type: 'field_input', name: 'PASSWORD', text: '' },
      ],
      colour: 0,
      tooltip: 'Configure WiFi credentials for ESP32 deployment',
    },
    {
      type: 'deploy_config_tts',
      message0: 'TTS server %1 voice %2',
      args0: [
        { type: 'field_input', name: 'URL', text: 'http://192.168.0.120:8001' },
        {
          type: 'field_dropdown',
          name: 'VOICE',
          options: [
            ['tom-medium', 'tom-medium'],
            ['siwis', 'siwis'],
            ['upmc', 'upmc'],
          ],
        },
      ],
      colour: 0,
      tooltip: 'Configure Piper TTS server URL and voice for deployment',
    },
    {
      type: 'deploy_config_llm',
      message0: 'LLM server %1 model %2',
      args0: [
        { type: 'field_input', name: 'URL', text: 'http://kxkm-ai:11434' },
        { type: 'field_input', name: 'MODEL', text: 'devstral' },
      ],
      colour: 0,
      tooltip: 'Configure LLM server URL and model for deployment',
    },
    {
      type: 'deploy_export',
      message0: 'Export pour ESP32',
      colour: 0,
      tooltip: 'Export the full scenario configuration for ESP32 firmware flashing',
    },
  ]);

  registered = true;
}
