import * as Blockly from 'blockly';

let registered = false;

export function registerHardwareBlocks(): void {
  if (registered) return;

  Blockly.defineBlocksWithJsonArray([
    {
      type: 'hw_gpio_write',
      message0: 'GPIO write pin %1 state %2',
      args0: [
        { type: 'field_number', name: 'PIN', value: 4, min: 0, max: 48, precision: 1 },
        {
          type: 'field_dropdown',
          name: 'STATE',
          options: [
            ['HIGH', 'HIGH'],
            ['LOW', 'LOW'],
          ],
        },
      ],
      previousStatement: null,
      nextStatement: null,
      colour: 30,
      tooltip: 'Write a digital value to a GPIO pin',
    },
    {
      type: 'hw_gpio_read',
      message0: 'GPIO read pin %1 into %2',
      args0: [
        { type: 'field_number', name: 'PIN', value: 4, min: 0, max: 48, precision: 1 },
        { type: 'field_input', name: 'VARIABLE', text: 'pin_value' },
      ],
      previousStatement: null,
      nextStatement: null,
      colour: 30,
      tooltip: 'Read a digital value from a GPIO pin into a variable',
    },
    {
      type: 'hw_led_set',
      message0: 'LED color %1 animation %2',
      args0: [
        { type: 'field_input', name: 'COLOR', text: '#00FF00' },
        {
          type: 'field_dropdown',
          name: 'ANIMATION',
          options: [
            ['solid', 'solid'],
            ['blink', 'blink'],
            ['pulse', 'pulse'],
            ['rainbow', 'rainbow'],
          ],
        },
      ],
      previousStatement: null,
      nextStatement: null,
      colour: 30,
      tooltip: 'Set LED color and animation pattern',
    },
    {
      type: 'hw_buzzer_tone',
      message0: 'Buzzer %1 Hz for %2 ms',
      args0: [
        { type: 'field_number', name: 'FREQUENCY', value: 440, min: 100, max: 5000, precision: 1 },
        { type: 'field_number', name: 'DURATION_MS', value: 500, min: 0, precision: 1 },
      ],
      previousStatement: null,
      nextStatement: null,
      colour: 30,
      tooltip: 'Play a tone on the buzzer at a given frequency and duration',
    },
    {
      type: 'hw_play_audio',
      message0: 'Play audio %1',
      args0: [
        { type: 'field_input', name: 'FILENAME', text: 'audio.mp3' },
      ],
      previousStatement: null,
      nextStatement: null,
      colour: 30,
      tooltip: 'Play an audio file from the SD card',
    },
    {
      type: 'hw_qr_scan',
      message0: 'QR scan',
      previousStatement: null,
      nextStatement: null,
      colour: 30,
      tooltip: 'Activate the QR code scanner and wait for a scan result',
    },
  ]);

  registered = true;
}
