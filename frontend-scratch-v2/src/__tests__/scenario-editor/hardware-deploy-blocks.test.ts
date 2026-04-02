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

// ─── Hardware blocks ───

describe('hw_gpio_write block', () => {
  it('creates a valid HardwareAction for gpio_write', () => {
    const ws = createWorkspace();
    const block = ws.newBlock('hw_gpio_write');
    block.setFieldValue('12', 'PIN');
    block.setFieldValue('LOW', 'STATE');

    const graph = buildScenarioGraph(ws);
    const gpioActions = graph.hardwareActions.filter((a) => a.type === 'gpio_write');
    expect(gpioActions).toHaveLength(1);
    expect(gpioActions[0].pin).toBe(12);
    expect(gpioActions[0].state).toBe('LOW');

    ws.dispose();
  });
});

describe('hw_gpio_read block', () => {
  it('creates a valid HardwareAction for gpio_read', () => {
    const ws = createWorkspace();
    const block = ws.newBlock('hw_gpio_read');
    block.setFieldValue('7', 'PIN');
    block.setFieldValue('sensor_val', 'VARIABLE');

    const graph = buildScenarioGraph(ws);
    const readActions = graph.hardwareActions.filter((a) => a.type === 'gpio_read');
    expect(readActions).toHaveLength(1);
    expect(readActions[0].pin).toBe(7);
    expect(readActions[0].variable).toBe('sensor_val');

    ws.dispose();
  });
});

describe('hw_led_set block', () => {
  it('creates HardwareAction with color and animation', () => {
    const ws = createWorkspace();
    const block = ws.newBlock('hw_led_set');
    block.setFieldValue('#FF0000', 'COLOR');
    block.setFieldValue('blink', 'ANIMATION');

    const graph = buildScenarioGraph(ws);
    const ledActions = graph.hardwareActions.filter((a) => a.type === 'led_set');
    expect(ledActions).toHaveLength(1);
    expect(ledActions[0].color).toBe('#FF0000');
    expect(ledActions[0].animation).toBe('blink');

    ws.dispose();
  });
});

describe('hw_buzzer_tone block', () => {
  it('creates HardwareAction with frequency and duration', () => {
    const ws = createWorkspace();
    const block = ws.newBlock('hw_buzzer_tone');
    block.setFieldValue('880', 'FREQUENCY');
    block.setFieldValue('1000', 'DURATION_MS');

    const graph = buildScenarioGraph(ws);
    const buzzerActions = graph.hardwareActions.filter((a) => a.type === 'buzzer');
    expect(buzzerActions).toHaveLength(1);
    expect(buzzerActions[0].frequency).toBe(880);
    expect(buzzerActions[0].duration_ms).toBe(1000);

    ws.dispose();
  });
});

describe('hw_play_audio block', () => {
  it('creates HardwareAction with filename', () => {
    const ws = createWorkspace();
    const block = ws.newBlock('hw_play_audio');
    block.setFieldValue('intro.mp3', 'FILENAME');

    const graph = buildScenarioGraph(ws);
    const audioActions = graph.hardwareActions.filter((a) => a.type === 'play_audio');
    expect(audioActions).toHaveLength(1);
    expect(audioActions[0].filename).toBe('intro.mp3');

    ws.dispose();
  });
});

describe('hw_qr_scan block', () => {
  it('creates HardwareAction for qr_scan with no extra fields', () => {
    const ws = createWorkspace();
    ws.newBlock('hw_qr_scan');

    const graph = buildScenarioGraph(ws);
    const qrActions = graph.hardwareActions.filter((a) => a.type === 'qr_scan');
    expect(qrActions).toHaveLength(1);

    ws.dispose();
  });
});

// ─── Deploy blocks ───

describe('deploy_config_wifi block', () => {
  it('produces DeployConfig with wifi settings', () => {
    const ws = createWorkspace();
    const block = ws.newBlock('deploy_config_wifi');
    block.setFieldValue('ZacusNet', 'SSID');
    block.setFieldValue('s3cr3t', 'PASSWORD');

    const graph = buildScenarioGraph(ws);
    expect(graph.deploy.wifi).toBeDefined();
    expect(graph.deploy.wifi!.ssid).toBe('ZacusNet');
    expect(graph.deploy.wifi!.password).toBe('s3cr3t');

    ws.dispose();
  });
});

describe('deploy_config_tts block', () => {
  it('produces DeployConfig with TTS settings', () => {
    const ws = createWorkspace();
    const block = ws.newBlock('deploy_config_tts');
    block.setFieldValue('http://tower:8001', 'URL');
    block.setFieldValue('siwis', 'VOICE');

    const graph = buildScenarioGraph(ws);
    expect(graph.deploy.tts).toBeDefined();
    expect(graph.deploy.tts!.url).toBe('http://tower:8001');
    expect(graph.deploy.tts!.voice).toBe('siwis');

    ws.dispose();
  });
});

describe('deploy_config_llm block', () => {
  it('produces DeployConfig with LLM settings', () => {
    const ws = createWorkspace();
    const block = ws.newBlock('deploy_config_llm');
    block.setFieldValue('http://localhost:11434', 'URL');
    block.setFieldValue('qwen2.5', 'MODEL');

    const graph = buildScenarioGraph(ws);
    expect(graph.deploy.llm).toBeDefined();
    expect(graph.deploy.llm!.url).toBe('http://localhost:11434');
    expect(graph.deploy.llm!.model).toBe('qwen2.5');

    ws.dispose();
  });
});

describe('deploy_export block', () => {
  it('can be created without errors', () => {
    const ws = createWorkspace();
    const block = ws.newBlock('deploy_export');
    expect(block.type).toBe('deploy_export');
    ws.dispose();
  });
});

// ─── Full scenario integration ───

describe('full scenario with hardware + deploy generates complete YAML', () => {
  it('produces display YAML with hardware and deploy sections', () => {
    const ws = createWorkspace();

    // Scene
    const scene = ws.newBlock('scenario_scene');
    scene.setFieldValue('SCENE_LAB', 'NAME');
    scene.setFieldValue('Laboratory', 'DESCRIPTION');
    scene.setFieldValue('180', 'DURATION_MAX');

    // Hardware: LED + buzzer
    const led = ws.newBlock('hw_led_set');
    led.setFieldValue('#0000FF', 'COLOR');
    led.setFieldValue('pulse', 'ANIMATION');

    const buzzer = ws.newBlock('hw_buzzer_tone');
    buzzer.setFieldValue('660', 'FREQUENCY');
    buzzer.setFieldValue('250', 'DURATION_MS');

    // Deploy: WiFi + TTS
    const wifi = ws.newBlock('deploy_config_wifi');
    wifi.setFieldValue('EscapeRoom', 'SSID');
    wifi.setFieldValue('pass123', 'PASSWORD');

    const tts = ws.newBlock('deploy_config_tts');
    tts.setFieldValue('http://192.168.0.120:8001', 'URL');
    tts.setFieldValue('tom-medium', 'VOICE');

    const graph = buildScenarioGraph(ws);
    expect(graph.scenes).toHaveLength(1);
    expect(graph.hardwareActions).toHaveLength(2);
    expect(graph.deploy.wifi).toBeDefined();
    expect(graph.deploy.tts).toBeDefined();

    // Display YAML
    const displayYaml = scenarioGraphToDisplayYaml(graph);
    const parsed = YAML.parse(displayYaml);
    expect(parsed.scenes).toHaveLength(1);
    expect(parsed.scenes[0].name).toBe('SCENE_LAB');
    expect(parsed.hardware).toHaveLength(2);
    expect(parsed.hardware[0].type).toBe('led_set');
    expect(parsed.hardware[0].color).toBe('#0000FF');
    expect(parsed.hardware[1].type).toBe('buzzer');
    expect(parsed.deploy).toBeDefined();
    expect(parsed.deploy.wifi.ssid).toBe('EscapeRoom');
    expect(parsed.deploy.tts.voice).toBe('tom-medium');

    // Firmware YAML
    const firmwareYaml = scenarioGraphToFirmwareYaml(graph);
    const fwParsed = YAML.parse(firmwareYaml);
    expect(fwParsed.firmware.steps).toHaveLength(1);
    expect(fwParsed.firmware.steps[0].actions).toHaveLength(2);
    expect(fwParsed.firmware.steps[0].actions[0].type).toBe('led_set');
    expect(fwParsed.firmware.deploy).toBeDefined();
    expect(fwParsed.firmware.deploy.wifi.ssid).toBe('EscapeRoom');

    ws.dispose();
  });

  it('produces clean YAML when no hardware or deploy blocks are present', () => {
    const ws = createWorkspace();

    const scene = ws.newBlock('scenario_scene');
    scene.setFieldValue('SCENE_EMPTY', 'NAME');

    const graph = buildScenarioGraph(ws);
    expect(graph.hardwareActions).toHaveLength(0);
    expect(graph.deploy).toEqual({});

    const displayYaml = scenarioGraphToDisplayYaml(graph);
    const parsed = YAML.parse(displayYaml);
    expect(parsed.hardware).toBeUndefined();
    expect(parsed.deploy).toBeUndefined();

    ws.dispose();
  });
});
