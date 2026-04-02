import { describe, it, expect, beforeAll } from 'vitest';
import * as Blockly from 'blockly';
import { ensureScenarioBlocks } from '../../components/ScenarioEditor/blocks/scene';
import { registerPuzzleBlocks } from '../../components/ScenarioEditor/blocks/puzzle';
import { registerNpcBlocks } from '../../components/ScenarioEditor/blocks/npc';
import { registerHardwareBlocks } from '../../components/ScenarioEditor/blocks/hardware';
import { registerDeployBlocks } from '../../components/ScenarioEditor/blocks/deploy';
import {
  buildScenarioGraph,
  scenarioGraphToFirmwareYaml,
  scenarioGraphToDisplayYaml,
} from '../../components/ScenarioEditor/generators/yaml';
import YAML from 'yaml';

beforeAll(() => {
  ensureScenarioBlocks();
  registerPuzzleBlocks();
  registerNpcBlocks();
  registerHardwareBlocks();
  registerDeployBlocks();
});

function createWorkspace(): Blockly.Workspace {
  return new Blockly.Workspace();
}

// ─── Helper: connect block to a statement input ───

function connectStatement(
  parent: Blockly.Block,
  inputName: string,
  child: Blockly.Block,
): void {
  const input = parent.getInput(inputName);
  if (input?.connection && child.previousConnection) {
    input.connection.connect(child.previousConnection);
  }
}

function connectValue(
  parent: Blockly.Block,
  inputName: string,
  child: Blockly.Block,
): void {
  const input = parent.getInput(inputName);
  if (input?.connection && child.outputConnection) {
    input.connection.connect(child.outputConnection);
  }
}

function chainBlocks(first: Blockly.Block, second: Blockly.Block): void {
  if (first.nextConnection && second.previousConnection) {
    first.nextConnection.connect(second.previousConnection);
  }
}

// ─── Scenario 1: Simple linear ───

describe('YAML snapshot: simple linear scenario', () => {
  it('generates display YAML with 2 scenes, 1 transition, 1 NPC say', () => {
    const ws = createWorkspace();

    // Scene 1: INTRO with transition to FINALE
    const scene1 = ws.newBlock('scenario_scene');
    scene1.setFieldValue('SCENE_INTRO', 'NAME');
    scene1.setFieldValue('Welcome to the escape room', 'DESCRIPTION');
    scene1.setFieldValue('120', 'DURATION_MAX');

    const trans = ws.newBlock('scenario_transition');
    trans.setFieldValue('SCENE_FINALE', 'TARGET_SCENE');
    connectStatement(scene1, 'TRANSITIONS', trans);

    // Scene 2: FINALE
    const scene2 = ws.newBlock('scenario_scene');
    scene2.setFieldValue('SCENE_FINALE', 'NAME');
    scene2.setFieldValue('Congratulations!', 'DESCRIPTION');
    scene2.setFieldValue('60', 'DURATION_MAX');

    // NPC say (standalone top-level block)
    const npcSay = ws.newBlock('npc_say');
    npcSay.setFieldValue('Bienvenue dans le laboratoire!', 'TEXT');
    npcSay.setFieldValue('amused', 'MOOD');

    const graph = buildScenarioGraph(ws);
    const displayYaml = scenarioGraphToDisplayYaml(graph);
    const parsed = YAML.parse(displayYaml);

    // Verify scenes
    expect(parsed.scenes).toHaveLength(2);
    expect(parsed.scenes[0].name).toBe('SCENE_INTRO');
    expect(parsed.scenes[0].duration_max_s).toBe(120);
    expect(parsed.scenes[0].transitions).toHaveLength(1);
    expect(parsed.scenes[0].transitions[0].target).toBe('SCENE_FINALE');
    expect(parsed.scenes[1].name).toBe('SCENE_FINALE');

    // Verify NPC
    expect(parsed.npc).toBeDefined();
    expect(parsed.npc).toHaveLength(1);
    expect(parsed.npc[0].type).toBe('say');
    expect(parsed.npc[0].text).toBe('Bienvenue dans le laboratoire!');
    expect(parsed.npc[0].mood).toBe('amused');

    // Verify YAML string contains key sections
    expect(displayYaml).toContain('scenes:');
    expect(displayYaml).toContain('SCENE_INTRO');
    expect(displayYaml).toContain('SCENE_FINALE');
    expect(displayYaml).toContain('npc:');

    ws.dispose();
  });

  it('generates firmware YAML with correct steps structure', () => {
    const ws = createWorkspace();

    const scene1 = ws.newBlock('scenario_scene');
    scene1.setFieldValue('SCENE_INTRO', 'NAME');

    const trans = ws.newBlock('scenario_transition');
    trans.setFieldValue('SCENE_FINALE', 'TARGET_SCENE');
    connectStatement(scene1, 'TRANSITIONS', trans);

    const scene2 = ws.newBlock('scenario_scene');
    scene2.setFieldValue('SCENE_FINALE', 'NAME');

    const graph = buildScenarioGraph(ws);
    const firmwareYaml = scenarioGraphToFirmwareYaml(graph);
    const parsed = YAML.parse(firmwareYaml);

    expect(parsed.firmware).toBeDefined();
    expect(parsed.firmware.steps).toHaveLength(2);
    expect(parsed.firmware.initial_step).toBe('SCENE_INTRO');
    expect(parsed.firmware.steps[0].step_id).toBe('SCENE_INTRO');
    expect(parsed.firmware.steps[0].transitions[0].target_step_id).toBe('SCENE_FINALE');
    expect(parsed.firmware.steps[1].step_id).toBe('SCENE_FINALE');

    ws.dispose();
  });
});

// ─── Scenario 2: Branching puzzle ───

describe('YAML snapshot: branching puzzle scenario', () => {
  it('generates display YAML with 3 scenes, puzzle with QR, 2 transitions, hints', () => {
    const ws = createWorkspace();

    // Scene 1: LOBBY with two transitions (success → LAB, timeout → GAMEOVER)
    const scene1 = ws.newBlock('scenario_scene');
    scene1.setFieldValue('SCENE_LOBBY', 'NAME');
    scene1.setFieldValue('The lobby area', 'DESCRIPTION');
    scene1.setFieldValue('300', 'DURATION_MAX');

    const transSuccess = ws.newBlock('scenario_transition');
    transSuccess.setFieldValue('SCENE_LAB', 'TARGET_SCENE');

    const transTimeout = ws.newBlock('scenario_transition');
    transTimeout.setFieldValue('SCENE_GAMEOVER', 'TARGET_SCENE');

    connectStatement(scene1, 'TRANSITIONS', transSuccess);
    chainBlocks(transSuccess, transTimeout);

    // Scene 2: LAB
    const scene2 = ws.newBlock('scenario_scene');
    scene2.setFieldValue('SCENE_LAB', 'NAME');
    scene2.setFieldValue('The secret laboratory', 'DESCRIPTION');
    scene2.setFieldValue('600', 'DURATION_MAX');

    // Scene 3: GAMEOVER
    const scene3 = ws.newBlock('scenario_scene');
    scene3.setFieldValue('SCENE_GAMEOVER', 'NAME');
    scene3.setFieldValue('Time is up!', 'DESCRIPTION');
    scene3.setFieldValue('30', 'DURATION_MAX');

    // Puzzle with QR validation and hints
    const puzzle = ws.newBlock('puzzle_definition');
    puzzle.setFieldValue('PUZZLE_ENTRANCE', 'NAME');
    puzzle.setFieldValue('qr', 'PUZZLE_TYPE');

    const qrValidation = ws.newBlock('puzzle_validation_qr');
    qrValidation.setFieldValue('ZACUS_DOOR_KEY', 'EXPECTED');
    connectValue(puzzle, 'SOLUTION', qrValidation);

    // Hints inside the puzzle
    const hint1 = ws.newBlock('npc_hint');
    hint1.setFieldValue('1', 'LEVEL');
    hint1.setFieldValue('PUZZLE_ENTRANCE', 'PUZZLE_ID');
    hint1.setFieldValue('Look at the painting on the wall', 'TEXT');

    const hint2 = ws.newBlock('npc_hint');
    hint2.setFieldValue('2', 'LEVEL');
    hint2.setFieldValue('PUZZLE_ENTRANCE', 'PUZZLE_ID');
    hint2.setFieldValue('The QR code is behind the frame', 'TEXT');

    connectStatement(puzzle, 'HINTS', hint1);
    chainBlocks(hint1, hint2);

    const graph = buildScenarioGraph(ws);
    const displayYaml = scenarioGraphToDisplayYaml(graph);
    const parsed = YAML.parse(displayYaml);

    // Scenes
    expect(parsed.scenes).toHaveLength(3);
    expect(parsed.scenes[0].name).toBe('SCENE_LOBBY');
    expect(parsed.scenes[0].transitions).toHaveLength(2);
    expect(parsed.scenes[0].transitions[0].target).toBe('SCENE_LAB');
    expect(parsed.scenes[0].transitions[1].target).toBe('SCENE_GAMEOVER');

    // Puzzle
    expect(parsed.puzzles).toBeDefined();
    expect(parsed.puzzles).toHaveLength(1);
    expect(parsed.puzzles[0].name).toBe('PUZZLE_ENTRANCE');
    expect(parsed.puzzles[0].type).toBe('qr');
    expect(parsed.puzzles[0].solution).toBe('qr:ZACUS_DOOR_KEY');
    expect(parsed.puzzles[0].hints).toHaveLength(2);
    expect(parsed.puzzles[0].hints[0].level).toBe(1);
    expect(parsed.puzzles[0].hints[0].text).toBe('Look at the painting on the wall');
    expect(parsed.puzzles[0].hints[1].level).toBe(2);

    // YAML string checks
    expect(displayYaml).toContain('puzzles:');
    expect(displayYaml).toContain('PUZZLE_ENTRANCE');
    expect(displayYaml).toContain('qr:ZACUS_DOOR_KEY');

    ws.dispose();
  });

  it('generates firmware YAML with puzzles section', () => {
    const ws = createWorkspace();

    const scene1 = ws.newBlock('scenario_scene');
    scene1.setFieldValue('SCENE_LOBBY', 'NAME');

    const puzzle = ws.newBlock('puzzle_definition');
    puzzle.setFieldValue('PUZZLE_QR', 'NAME');
    puzzle.setFieldValue('qr', 'PUZZLE_TYPE');

    const qr = ws.newBlock('puzzle_validation_qr');
    qr.setFieldValue('KEY_42', 'EXPECTED');
    connectValue(puzzle, 'SOLUTION', qr);

    const hint = ws.newBlock('npc_hint');
    hint.setFieldValue('1', 'LEVEL');
    hint.setFieldValue('PUZZLE_QR', 'PUZZLE_ID');
    hint.setFieldValue('Check under the desk', 'TEXT');
    connectStatement(puzzle, 'HINTS', hint);

    const graph = buildScenarioGraph(ws);
    const firmwareYaml = scenarioGraphToFirmwareYaml(graph);
    const parsed = YAML.parse(firmwareYaml);

    expect(parsed.firmware.steps).toHaveLength(1);
    expect(parsed.firmware.puzzles).toBeDefined();
    expect(parsed.firmware.puzzles).toHaveLength(1);
    expect(parsed.firmware.puzzles[0].puzzle_id).toBe('PUZZLE_QR');
    expect(parsed.firmware.puzzles[0].type).toBe('qr');
    expect(parsed.firmware.puzzles[0].solution).toBe('qr:KEY_42');
    expect(parsed.firmware.puzzles[0].hints_count).toBe(1);

    ws.dispose();
  });
});

// ─── Scenario 3: Full escape room ───

describe('YAML snapshot: full escape room scenario', () => {
  it('generates display YAML with all sections', () => {
    const ws = createWorkspace();

    // Scene 1: INTRO with timer action
    const scene1 = ws.newBlock('scenario_scene');
    scene1.setFieldValue('SCENE_INTRO', 'NAME');
    scene1.setFieldValue('The adventure begins', 'DESCRIPTION');
    scene1.setFieldValue('180', 'DURATION_MAX');

    const timer = ws.newBlock('scenario_timer');
    timer.setFieldValue('60', 'SECONDS');
    const varSet = ws.newBlock('scenario_variable_set');
    varSet.setFieldValue('intro_timeout', 'NAME');
    connectStatement(timer, 'ON_EXPIRE', varSet);
    connectStatement(scene1, 'ACTIONS', timer);

    const trans1 = ws.newBlock('scenario_transition');
    trans1.setFieldValue('SCENE_LAB', 'TARGET_SCENE');
    connectStatement(scene1, 'TRANSITIONS', trans1);

    // Scene 2: LAB
    const scene2 = ws.newBlock('scenario_scene');
    scene2.setFieldValue('SCENE_LAB', 'NAME');
    scene2.setFieldValue('Professor Zacus laboratory', 'DESCRIPTION');
    scene2.setFieldValue('600', 'DURATION_MAX');

    const trans2 = ws.newBlock('scenario_transition');
    trans2.setFieldValue('SCENE_ESCAPE', 'TARGET_SCENE');
    connectStatement(scene2, 'TRANSITIONS', trans2);

    // Scene 3: ESCAPE
    const scene3 = ws.newBlock('scenario_scene');
    scene3.setFieldValue('SCENE_ESCAPE', 'NAME');
    scene3.setFieldValue('Find the exit!', 'DESCRIPTION');
    scene3.setFieldValue('300', 'DURATION_MAX');

    // Puzzle 1: QR
    const puzzle1 = ws.newBlock('puzzle_definition');
    puzzle1.setFieldValue('PUZZLE_DOOR', 'NAME');
    puzzle1.setFieldValue('qr', 'PUZZLE_TYPE');

    const qr = ws.newBlock('puzzle_validation_qr');
    qr.setFieldValue('ZACUS_EXIT', 'EXPECTED');
    connectValue(puzzle1, 'SOLUTION', qr);

    const hint = ws.newBlock('npc_hint');
    hint.setFieldValue('1', 'LEVEL');
    hint.setFieldValue('PUZZLE_DOOR', 'PUZZLE_ID');
    hint.setFieldValue('Search the bookshelf', 'TEXT');
    connectStatement(puzzle1, 'HINTS', hint);

    // Puzzle 2: Button
    const puzzle2 = ws.newBlock('puzzle_definition');
    puzzle2.setFieldValue('PUZZLE_SAFE', 'NAME');
    puzzle2.setFieldValue('button', 'PUZZLE_TYPE');

    const btnVal = ws.newBlock('puzzle_validation_button');
    btnVal.setFieldValue('12', 'PIN');
    connectValue(puzzle2, 'SOLUTION', btnVal);

    // NPC dialogue
    const npcSay = ws.newBlock('npc_say');
    npcSay.setFieldValue('Bienvenue dans mon laboratoire!', 'TEXT');
    npcSay.setFieldValue('impressed', 'MOOD');

    // Hardware: GPIO write
    const gpioWrite = ws.newBlock('hw_gpio_write');
    gpioWrite.setFieldValue('15', 'PIN');
    gpioWrite.setFieldValue('HIGH', 'STATE');

    // Deploy config: WiFi + TTS + LLM
    const wifi = ws.newBlock('deploy_config_wifi');
    wifi.setFieldValue('ZacusNet', 'SSID');
    wifi.setFieldValue('secret123', 'PASSWORD');

    const tts = ws.newBlock('deploy_config_tts');
    tts.setFieldValue('http://192.168.0.120:8001', 'URL');
    tts.setFieldValue('tom-medium', 'VOICE');

    const llm = ws.newBlock('deploy_config_llm');
    llm.setFieldValue('http://kxkm-ai:11434', 'URL');
    llm.setFieldValue('devstral', 'MODEL');

    const graph = buildScenarioGraph(ws);
    const displayYaml = scenarioGraphToDisplayYaml(graph);
    const parsed = YAML.parse(displayYaml);

    // Scenes
    expect(parsed.scenes).toHaveLength(3);
    expect(parsed.scenes[0].name).toBe('SCENE_INTRO');
    expect(parsed.scenes[0].actions).toHaveLength(1);
    expect(parsed.scenes[0].actions[0].type).toBe('timer');
    expect(parsed.scenes[0].actions[0].seconds).toBe(60);

    // Puzzles
    expect(parsed.puzzles).toHaveLength(2);
    expect(parsed.puzzles[0].name).toBe('PUZZLE_DOOR');
    expect(parsed.puzzles[0].solution).toBe('qr:ZACUS_EXIT');
    expect(parsed.puzzles[1].name).toBe('PUZZLE_SAFE');
    expect(parsed.puzzles[1].type).toBe('button');
    expect(parsed.puzzles[1].solution).toBe('button:12');

    // NPC
    expect(parsed.npc).toBeDefined();
    expect(parsed.npc.length).toBeGreaterThanOrEqual(1);
    const sayAction = parsed.npc.find(
      (a: { type: string }) => a.type === 'say',
    );
    expect(sayAction).toBeDefined();
    expect(sayAction.text).toBe('Bienvenue dans mon laboratoire!');

    // Hardware
    expect(parsed.hardware).toBeDefined();
    expect(parsed.hardware.length).toBeGreaterThanOrEqual(1);
    const gpio = parsed.hardware.find(
      (a: { type: string }) => a.type === 'gpio_write',
    );
    expect(gpio).toBeDefined();
    expect(gpio.pin).toBe(15);
    expect(gpio.state).toBe('HIGH');

    // Deploy
    expect(parsed.deploy).toBeDefined();
    expect(parsed.deploy.wifi).toEqual({ ssid: 'ZacusNet', password: 'secret123' });
    expect(parsed.deploy.tts).toEqual({
      url: 'http://192.168.0.120:8001',
      voice: 'tom-medium',
    });
    expect(parsed.deploy.llm).toEqual({
      url: 'http://kxkm-ai:11434',
      model: 'devstral',
    });

    // Verify all display YAML sections exist
    expect(displayYaml).toContain('scenes:');
    expect(displayYaml).toContain('puzzles:');
    expect(displayYaml).toContain('npc:');
    expect(displayYaml).toContain('hardware:');
    expect(displayYaml).toContain('deploy:');

    ws.dispose();
  });

  it('generates firmware YAML with hardware actions attached to first step', () => {
    const ws = createWorkspace();

    const scene1 = ws.newBlock('scenario_scene');
    scene1.setFieldValue('SCENE_BOOT', 'NAME');

    const scene2 = ws.newBlock('scenario_scene');
    scene2.setFieldValue('SCENE_PLAY', 'NAME');

    // Hardware
    const gpio = ws.newBlock('hw_gpio_write');
    gpio.setFieldValue('15', 'PIN');
    gpio.setFieldValue('HIGH', 'STATE');

    const led = ws.newBlock('hw_led_set');
    led.setFieldValue('#FF0000', 'COLOR');
    led.setFieldValue('blink', 'ANIMATION');

    // Deploy
    const wifi = ws.newBlock('deploy_config_wifi');
    wifi.setFieldValue('EscapeNet', 'SSID');
    wifi.setFieldValue('pass', 'PASSWORD');

    const graph = buildScenarioGraph(ws);
    const firmwareYaml = scenarioGraphToFirmwareYaml(graph);
    const parsed = YAML.parse(firmwareYaml);

    // firmware.steps[] structure
    expect(parsed.firmware).toBeDefined();
    expect(parsed.firmware.steps).toHaveLength(2);
    expect(parsed.firmware.initial_step).toBe('SCENE_BOOT');

    // Hardware actions on first step
    expect(parsed.firmware.steps[0].actions).toHaveLength(2);
    expect(parsed.firmware.steps[0].actions[0].type).toBe('gpio_write');
    expect(parsed.firmware.steps[0].actions[0].pin).toBe(15);
    expect(parsed.firmware.steps[0].actions[1].type).toBe('led_set');
    expect(parsed.firmware.steps[0].actions[1].color).toBe('#FF0000');

    // Deploy config in firmware
    expect(parsed.firmware.deploy).toBeDefined();
    expect(parsed.firmware.deploy.wifi.ssid).toBe('EscapeNet');

    ws.dispose();
  });
});
