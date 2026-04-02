import { describe, it, expect, beforeAll } from 'vitest';
import * as Blockly from 'blockly';
import { ensureScenarioBlocks } from '../../components/ScenarioEditor/blocks/scene';
import {
  buildScenarioGraph,
  scenarioGraphToFirmwareYaml,
  scenarioGraphToDisplayYaml,
} from '../../components/ScenarioEditor/generators/yaml';
import YAML from 'yaml';

beforeAll(() => {
  ensureScenarioBlocks();
});

function createWorkspace(): Blockly.Workspace {
  return new Blockly.Workspace();
}

describe('scenario_scene block', () => {
  it('creates a valid graph node from a single scene block', () => {
    const ws = createWorkspace();
    const block = ws.newBlock('scenario_scene');
    block.setFieldValue('SCENE_INTRO', 'NAME');
    block.setFieldValue('Welcome to the game', 'DESCRIPTION');
    block.setFieldValue('120', 'DURATION_MAX');

    const graph = buildScenarioGraph(ws);
    expect(graph.scenes).toHaveLength(1);
    expect(graph.scenes[0].name).toBe('SCENE_INTRO');
    expect(graph.scenes[0].description).toBe('Welcome to the game');
    expect(graph.scenes[0].durationMax).toBe(120);
    expect(graph.scenes[0].transitions).toEqual([]);
    expect(graph.scenes[0].actions).toEqual([]);

    ws.dispose();
  });
});

describe('scenario_transition block', () => {
  it('links two scenes via a transition', () => {
    const ws = createWorkspace();

    // Create scene with a transition in TRANSITIONS input
    const scene = ws.newBlock('scenario_scene');
    scene.setFieldValue('SCENE_A', 'NAME');

    const transition = ws.newBlock('scenario_transition');
    transition.setFieldValue('SCENE_B', 'TARGET_SCENE');

    // Connect transition to scene's TRANSITIONS input
    const input = scene.getInput('TRANSITIONS');
    if (input?.connection && transition.previousConnection) {
      input.connection.connect(transition.previousConnection);
    }

    const graph = buildScenarioGraph(ws);
    expect(graph.scenes).toHaveLength(1);
    expect(graph.scenes[0].transitions).toHaveLength(1);
    expect(graph.scenes[0].transitions[0].targetScene).toBe('SCENE_B');

    ws.dispose();
  });
});

describe('scenario_timer block', () => {
  it('produces correct firmware transition from timer action', () => {
    const ws = createWorkspace();

    const scene = ws.newBlock('scenario_scene');
    scene.setFieldValue('SCENE_TIMED', 'NAME');

    const timer = ws.newBlock('scenario_timer');
    timer.setFieldValue('30', 'SECONDS');

    // Add a variable_set inside timer's ON_EXPIRE
    const varSet = ws.newBlock('scenario_variable_set');
    varSet.setFieldValue('game_over', 'NAME');

    const timerOnExpire = timer.getInput('ON_EXPIRE');
    if (timerOnExpire?.connection && varSet.previousConnection) {
      timerOnExpire.connection.connect(varSet.previousConnection);
    }

    // Connect timer to scene's ACTIONS input
    const actionsInput = scene.getInput('ACTIONS');
    if (actionsInput?.connection && timer.previousConnection) {
      actionsInput.connection.connect(timer.previousConnection);
    }

    const graph = buildScenarioGraph(ws);
    expect(graph.scenes[0].actions).toHaveLength(1);
    expect(graph.scenes[0].actions[0].kind).toBe('timer');

    const timerAction = graph.scenes[0].actions[0];
    if (timerAction.kind === 'timer') {
      expect(timerAction.seconds).toBe(30);
      expect(timerAction.onExpire).toHaveLength(1);
      expect(timerAction.onExpire[0].kind).toBe('variable_set');
    }

    // Check firmware YAML output
    const firmwareYaml = scenarioGraphToFirmwareYaml(graph);
    const parsed = YAML.parse(firmwareYaml);
    const step = parsed.firmware.steps[0];
    expect(step.step_id).toBe('SCENE_TIMED');

    // Timer with variable_set on_expire should produce an action-type transition
    const actionTransition = step.transitions.find(
      (t: { event_type: string }) => t.event_type === 'action',
    );
    expect(actionTransition).toBeDefined();
    expect(actionTransition.after_ms).toBe(30000);

    ws.dispose();
  });
});

describe('full graph to firmware YAML round-trip', () => {
  it('generates valid firmware YAML from a multi-scene graph', () => {
    const ws = createWorkspace();

    // Scene A
    const sceneA = ws.newBlock('scenario_scene');
    sceneA.setFieldValue('SCENE_A', 'NAME');
    sceneA.setFieldValue('First scene', 'DESCRIPTION');
    sceneA.setFieldValue('60', 'DURATION_MAX');

    // Transition A -> B
    const transAB = ws.newBlock('scenario_transition');
    transAB.setFieldValue('SCENE_B', 'TARGET_SCENE');
    const inputA = sceneA.getInput('TRANSITIONS');
    if (inputA?.connection && transAB.previousConnection) {
      inputA.connection.connect(transAB.previousConnection);
    }

    // Scene B
    const sceneB = ws.newBlock('scenario_scene');
    sceneB.setFieldValue('SCENE_B', 'NAME');
    sceneB.setFieldValue('Second scene', 'DESCRIPTION');

    const graph = buildScenarioGraph(ws);
    expect(graph.scenes).toHaveLength(2);

    // Firmware YAML
    const firmwareYaml = scenarioGraphToFirmwareYaml(graph);
    const fwParsed = YAML.parse(firmwareYaml);
    expect(fwParsed.firmware.initial_step).toBe('SCENE_A');
    expect(fwParsed.firmware.steps).toHaveLength(2);
    expect(fwParsed.firmware.steps[0].step_id).toBe('SCENE_A');
    expect(fwParsed.firmware.steps[0].transitions[0].target_step_id).toBe('SCENE_B');
    expect(fwParsed.firmware.steps[1].step_id).toBe('SCENE_B');

    // Display YAML
    const displayYaml = scenarioGraphToDisplayYaml(graph);
    const displayParsed = YAML.parse(displayYaml);
    expect(displayParsed.scenes).toHaveLength(2);
    expect(displayParsed.scenes[0].name).toBe('SCENE_A');
    expect(displayParsed.scenes[0].description).toBe('First scene');
    expect(displayParsed.scenes[0].duration_max_s).toBe(60);
    expect(displayParsed.scenes[0].transitions[0].target).toBe('SCENE_B');
    expect(displayParsed.scenes[1].name).toBe('SCENE_B');

    ws.dispose();
  });
});
