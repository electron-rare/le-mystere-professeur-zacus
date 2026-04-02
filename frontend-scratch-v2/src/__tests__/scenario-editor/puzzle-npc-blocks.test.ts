import { describe, it, expect, beforeAll } from 'vitest';
import * as Blockly from 'blockly';
import { ensureScenarioBlocks } from '../../components/ScenarioEditor/blocks/scene';
import { registerPuzzleBlocks } from '../../components/ScenarioEditor/blocks/puzzle';
import { registerNpcBlocks } from '../../components/ScenarioEditor/blocks/npc';
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
});

function createWorkspace(): Blockly.Workspace {
  return new Blockly.Workspace();
}

describe('puzzle_definition block', () => {
  it('creates a valid PuzzleNode from a puzzle block', () => {
    const ws = createWorkspace();
    const block = ws.newBlock('puzzle_definition');
    block.setFieldValue('PUZZLE_QR_1', 'NAME');
    block.setFieldValue('qr', 'PUZZLE_TYPE');

    const graph = buildScenarioGraph(ws);
    expect(graph.puzzles).toHaveLength(1);
    expect(graph.puzzles[0].name).toBe('PUZZLE_QR_1');
    expect(graph.puzzles[0].type).toBe('qr');
    expect(graph.puzzles[0].hints).toEqual([]);

    ws.dispose();
  });

  it('reads hints nested inside puzzle_definition', () => {
    const ws = createWorkspace();
    const puzzle = ws.newBlock('puzzle_definition');
    puzzle.setFieldValue('PUZZLE_DOOR', 'NAME');
    puzzle.setFieldValue('button', 'PUZZLE_TYPE');

    const hint1 = ws.newBlock('npc_hint');
    hint1.setFieldValue('1', 'LEVEL');
    hint1.setFieldValue('Look at the door', 'TEXT');
    hint1.setFieldValue('PUZZLE_DOOR', 'PUZZLE_ID');

    const hint2 = ws.newBlock('npc_hint');
    hint2.setFieldValue('2', 'LEVEL');
    hint2.setFieldValue('Try the red button', 'TEXT');
    hint2.setFieldValue('PUZZLE_DOOR', 'PUZZLE_ID');

    // Connect hints to puzzle HINTS input
    const hintsInput = puzzle.getInput('HINTS');
    if (hintsInput?.connection && hint1.previousConnection) {
      hintsInput.connection.connect(hint1.previousConnection);
    }
    if (hint1.nextConnection && hint2.previousConnection) {
      hint1.nextConnection.connect(hint2.previousConnection);
    }

    const graph = buildScenarioGraph(ws);
    expect(graph.puzzles).toHaveLength(1);
    expect(graph.puzzles[0].hints).toHaveLength(2);
    expect(graph.puzzles[0].hints[0].level).toBe(1);
    expect(graph.puzzles[0].hints[0].text).toBe('Look at the door');
    expect(graph.puzzles[0].hints[1].level).toBe(2);

    ws.dispose();
  });
});

describe('puzzle_validation_qr block', () => {
  it('produces correct solution field when connected to puzzle', () => {
    const ws = createWorkspace();
    const puzzle = ws.newBlock('puzzle_definition');
    puzzle.setFieldValue('QR_PUZZLE', 'NAME');
    puzzle.setFieldValue('qr', 'PUZZLE_TYPE');

    const qrVal = ws.newBlock('puzzle_validation_qr');
    qrVal.setFieldValue('ZACUS_KEY_1', 'EXPECTED');

    // Connect QR validation to SOLUTION input
    const solutionInput = puzzle.getInput('SOLUTION');
    if (solutionInput?.connection && qrVal.outputConnection) {
      solutionInput.connection.connect(qrVal.outputConnection);
    }

    const graph = buildScenarioGraph(ws);
    expect(graph.puzzles[0].solution).toBe('qr:ZACUS_KEY_1');

    ws.dispose();
  });
});

describe('puzzle_validation_button block', () => {
  it('produces correct solution field for button validation', () => {
    const ws = createWorkspace();
    const puzzle = ws.newBlock('puzzle_definition');
    puzzle.setFieldValue('BTN_PUZZLE', 'NAME');
    puzzle.setFieldValue('button', 'PUZZLE_TYPE');

    const btnVal = ws.newBlock('puzzle_validation_button');
    btnVal.setFieldValue('7', 'PIN');

    const solutionInput = puzzle.getInput('SOLUTION');
    if (solutionInput?.connection && btnVal.outputConnection) {
      solutionInput.connection.connect(btnVal.outputConnection);
    }

    const graph = buildScenarioGraph(ws);
    expect(graph.puzzles[0].solution).toBe('button:7');

    ws.dispose();
  });
});

describe('puzzle_condition block', () => {
  it('creates a condition with type and reference', () => {
    const ws = createWorkspace();
    const cond = ws.newBlock('puzzle_condition');
    cond.setFieldValue('puzzle_solved', 'CONDITION_TYPE');
    cond.setFieldValue('PUZZLE_QR_1', 'REFERENCE');

    // puzzle_condition is a value block (output), verify it exists
    expect(cond.outputConnection).toBeTruthy();

    ws.dispose();
  });
});

describe('npc_say block', () => {
  it('creates NPCAction with text and mood', () => {
    const ws = createWorkspace();
    const say = ws.newBlock('npc_say');
    say.setFieldValue('Welcome, adventurer!', 'TEXT');
    say.setFieldValue('amused', 'MOOD');

    const graph = buildScenarioGraph(ws);
    const sayActions = graph.npcActions.filter((a) => a.type === 'say');
    expect(sayActions).toHaveLength(1);
    expect(sayActions[0].text).toBe('Welcome, adventurer!');
    expect(sayActions[0].mood).toBe('amused');

    ws.dispose();
  });
});

describe('npc_hint block (standalone)', () => {
  it('links hint to puzzle ID', () => {
    const ws = createWorkspace();
    const hint = ws.newBlock('npc_hint');
    hint.setFieldValue('2', 'LEVEL');
    hint.setFieldValue('Check under the table', 'TEXT');
    hint.setFieldValue('PUZZLE_DOOR', 'PUZZLE_ID');

    const graph = buildScenarioGraph(ws);
    const hintActions = graph.npcActions.filter((a) => a.type === 'hint');
    expect(hintActions).toHaveLength(1);
    expect(hintActions[0].level).toBe(2);
    expect(hintActions[0].text).toBe('Check under the table');
    expect(hintActions[0].puzzleId).toBe('PUZZLE_DOOR');

    ws.dispose();
  });
});

describe('npc_react block', () => {
  it('creates a react action with condition', () => {
    const ws = createWorkspace();
    const react = ws.newBlock('npc_react');
    react.setFieldValue('Well done!', 'RESPONSE');

    const cond = ws.newBlock('puzzle_condition');
    cond.setFieldValue('puzzle_solved', 'CONDITION_TYPE');
    cond.setFieldValue('QR_1', 'REFERENCE');

    const condInput = react.getInput('CONDITION');
    if (condInput?.connection && cond.outputConnection) {
      condInput.connection.connect(cond.outputConnection);
    }

    const graph = buildScenarioGraph(ws);
    const reactActions = graph.npcActions.filter((a) => a.type === 'react');
    expect(reactActions).toHaveLength(1);
    expect(reactActions[0].text).toBe('Well done!');
    expect(reactActions[0].condition).toBe('puzzle_condition');

    ws.dispose();
  });
});

describe('npc_conversation block', () => {
  it('creates a conversation action with system prompt', () => {
    const ws = createWorkspace();
    const conv = ws.newBlock('npc_conversation');
    conv.setFieldValue('You are Professor Zacus', 'SYSTEM_PROMPT');
    conv.setFieldValue('escape room intro', 'CONTEXT');

    const graph = buildScenarioGraph(ws);
    const convActions = graph.npcActions.filter((a) => a.type === 'conversation');
    expect(convActions).toHaveLength(1);
    expect(convActions[0].systemPrompt).toBe('You are Professor Zacus');
    expect(convActions[0].text).toBe('escape room intro');

    ws.dispose();
  });
});

describe('full scenario with scenes + puzzles + NPC generates valid YAML', () => {
  it('produces display YAML with scenes, puzzles, and npc sections', () => {
    const ws = createWorkspace();

    // Scene
    const scene = ws.newBlock('scenario_scene');
    scene.setFieldValue('SCENE_INTRO', 'NAME');
    scene.setFieldValue('Welcome', 'DESCRIPTION');
    scene.setFieldValue('120', 'DURATION_MAX');

    // Puzzle
    const puzzle = ws.newBlock('puzzle_definition');
    puzzle.setFieldValue('PUZZLE_QR', 'NAME');
    puzzle.setFieldValue('qr', 'PUZZLE_TYPE');

    const qrVal = ws.newBlock('puzzle_validation_qr');
    qrVal.setFieldValue('KEY_42', 'EXPECTED');
    const solInput = puzzle.getInput('SOLUTION');
    if (solInput?.connection && qrVal.outputConnection) {
      solInput.connection.connect(qrVal.outputConnection);
    }

    // NPC say (standalone, not inside scene actions)
    const say = ws.newBlock('npc_say');
    say.setFieldValue('Bienvenue!', 'TEXT');
    say.setFieldValue('impressed', 'MOOD');

    const graph = buildScenarioGraph(ws);
    expect(graph.scenes).toHaveLength(1);
    expect(graph.puzzles).toHaveLength(1);
    expect(graph.npcActions.length).toBeGreaterThanOrEqual(1);

    // Display YAML
    const displayYaml = scenarioGraphToDisplayYaml(graph);
    const parsed = YAML.parse(displayYaml);
    expect(parsed.scenes).toHaveLength(1);
    expect(parsed.scenes[0].name).toBe('SCENE_INTRO');
    expect(parsed.puzzles).toHaveLength(1);
    expect(parsed.puzzles[0].name).toBe('PUZZLE_QR');
    expect(parsed.puzzles[0].type).toBe('qr');
    expect(parsed.puzzles[0].solution).toBe('qr:KEY_42');
    expect(parsed.npc).toBeDefined();
    expect(parsed.npc.length).toBeGreaterThanOrEqual(1);

    // Firmware YAML
    const firmwareYaml = scenarioGraphToFirmwareYaml(graph);
    const fwParsed = YAML.parse(firmwareYaml);
    expect(fwParsed.firmware.steps).toHaveLength(1);
    expect(fwParsed.firmware.puzzles).toHaveLength(1);
    expect(fwParsed.firmware.puzzles[0].puzzle_id).toBe('PUZZLE_QR');
    expect(fwParsed.firmware.puzzles[0].type).toBe('qr');

    ws.dispose();
  });
});
