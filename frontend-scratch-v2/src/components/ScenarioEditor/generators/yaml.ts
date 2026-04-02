import * as Blockly from 'blockly';
import YAML from 'yaml';
import type {
  ScenarioGraph,
  SceneNode,
  TransitionEdge,
  SceneAction,
  TimerAction,
  VariableSetAction,
  PuzzleNode,
  NPCAction,
} from '../types';

// ─── Workspace → ScenarioGraph ───

function readConditionText(block: Blockly.Block, inputName: string): string {
  const target = block.getInputTargetBlock(inputName);
  if (!target) return '';
  if (target.type === 'scenario_variable_get') {
    return `var:${target.getFieldValue('NAME') ?? 'unknown'}`;
  }
  return target.type;
}

function readActions(block: Blockly.Block, inputName: string): SceneAction[] {
  const actions: SceneAction[] = [];
  let cursor = block.getInputTargetBlock(inputName);
  while (cursor) {
    if (cursor.type === 'scenario_timer') {
      const timerAction: TimerAction = {
        kind: 'timer',
        seconds: Number(cursor.getFieldValue('SECONDS') ?? 10),
        onExpire: readActions(cursor, 'ON_EXPIRE'),
      };
      actions.push(timerAction);
    } else if (cursor.type === 'scenario_variable_set') {
      const varAction: VariableSetAction = {
        kind: 'variable_set',
        name: String(cursor.getFieldValue('NAME') ?? 'my_var'),
        value: readConditionText(cursor, 'VALUE'),
      };
      actions.push(varAction);
    }
    cursor = cursor.getNextBlock();
  }
  return actions;
}

function readTransitions(block: Blockly.Block): TransitionEdge[] {
  const transitions: TransitionEdge[] = [];
  let cursor = block.getInputTargetBlock('TRANSITIONS');
  while (cursor) {
    if (cursor.type === 'scenario_transition') {
      transitions.push({
        targetScene: String(cursor.getFieldValue('TARGET_SCENE') ?? 'SCENE_NEXT'),
        condition: readConditionText(cursor, 'CONDITION'),
      });
    }
    cursor = cursor.getNextBlock();
  }
  return transitions;
}

// ─── Puzzle blocks → PuzzleNode ───

function readPuzzleHints(block: Blockly.Block): Array<{ level: number; text: string }> {
  const hints: Array<{ level: number; text: string }> = [];
  let cursor = block.getInputTargetBlock('HINTS');
  while (cursor) {
    if (cursor.type === 'npc_hint') {
      hints.push({
        level: Number(cursor.getFieldValue('LEVEL') ?? 1),
        text: String(cursor.getFieldValue('TEXT') ?? ''),
      });
    }
    cursor = cursor.getNextBlock();
  }
  return hints;
}

function readPuzzleSolution(block: Blockly.Block): string | undefined {
  const target = block.getInputTargetBlock('SOLUTION');
  if (!target) return undefined;
  if (target.type === 'puzzle_validation_qr') {
    return `qr:${target.getFieldValue('EXPECTED') ?? ''}`;
  }
  if (target.type === 'puzzle_validation_button') {
    return `button:${target.getFieldValue('PIN') ?? 4}`;
  }
  return target.type;
}

// ─── NPC blocks → NPCAction[] ───

function readNpcActions(workspace: Blockly.Workspace): NPCAction[] {
  const actions: NPCAction[] = [];
  for (const block of workspace.getAllBlocks(true)) {
    switch (block.type) {
      case 'npc_say':
        actions.push({
          type: 'say',
          text: String(block.getFieldValue('TEXT') ?? ''),
          mood: String(block.getFieldValue('MOOD') ?? 'neutral'),
        });
        break;
      case 'npc_mood_set':
        actions.push({
          type: 'mood',
          mood: String(block.getFieldValue('MOOD') ?? 'neutral'),
        });
        break;
      case 'npc_hint':
        // Only collect standalone hints (not those inside puzzle_definition)
        if (!block.getParent() || block.getParent()?.type !== 'puzzle_definition') {
          actions.push({
            type: 'hint',
            level: Number(block.getFieldValue('LEVEL') ?? 1),
            text: String(block.getFieldValue('TEXT') ?? ''),
            puzzleId: String(block.getFieldValue('PUZZLE_ID') ?? ''),
          });
        }
        break;
      case 'npc_react':
        actions.push({
          type: 'react',
          condition: readConditionText(block, 'CONDITION'),
          text: String(block.getFieldValue('RESPONSE') ?? ''),
        });
        break;
      case 'npc_conversation':
        actions.push({
          type: 'conversation',
          systemPrompt: String(block.getFieldValue('SYSTEM_PROMPT') ?? ''),
          text: String(block.getFieldValue('CONTEXT') ?? ''),
        });
        break;
    }
  }
  return actions;
}

/**
 * Walk workspace top blocks and build a ScenarioGraph.
 * Works with both WorkspaceSvg and headless Workspace.
 */
export function buildScenarioGraph(workspace: Blockly.Workspace): ScenarioGraph {
  const scenes: SceneNode[] = [];
  const puzzles: PuzzleNode[] = [];
  const seen = new Set<string>();

  for (const block of workspace.getTopBlocks(true)) {
    if (block.type === 'scenario_scene') {
      const id = block.id;
      if (seen.has(id)) continue;
      seen.add(id);

      scenes.push({
        name: String(block.getFieldValue('NAME') ?? 'SCENE_NEW'),
        description: String(block.getFieldValue('DESCRIPTION') ?? ''),
        durationMax: Number(block.getFieldValue('DURATION_MAX') ?? 300),
        actions: readActions(block, 'ACTIONS'),
        transitions: readTransitions(block),
      });
    } else if (block.type === 'puzzle_definition') {
      const id = block.id;
      if (seen.has(id)) continue;
      seen.add(id);

      puzzles.push({
        id,
        name: String(block.getFieldValue('NAME') ?? 'PUZZLE_NEW'),
        type: String(block.getFieldValue('PUZZLE_TYPE') ?? 'free') as PuzzleNode['type'],
        solution: readPuzzleSolution(block),
        hints: readPuzzleHints(block),
      });
    }
  }

  const npcActions = readNpcActions(workspace);

  return { scenes, puzzles, npcActions };
}

// ─── ScenarioGraph → Firmware YAML ───

function actionsToFirmwareTransitions(
  actions: SceneAction[],
  sceneIndex: number,
): Array<{
  event_type: string;
  event_name: string;
  target_step_id: string;
  priority: number;
  after_ms: number;
}> {
  const result: Array<{
    event_type: string;
    event_name: string;
    target_step_id: string;
    priority: number;
    after_ms: number;
  }> = [];
  for (const action of actions) {
    if (action.kind === 'timer') {
      // A timer with on_expire transitions generates a timer-type firmware transition
      for (const child of action.onExpire) {
        if (child.kind === 'variable_set') {
          // Variable sets in on_expire map to action-type transitions
          result.push({
            event_type: 'action',
            event_name: `SET_${child.name.toUpperCase()}`,
            target_step_id: '',
            priority: 0,
            after_ms: action.seconds * 1000,
          });
        }
      }
    }
  }
  void sceneIndex;
  return result;
}

/**
 * Project ScenarioGraph to firmware.steps[] format compatible with compile_runtime3.py
 */
export function scenarioGraphToFirmwareYaml(graph: ScenarioGraph): string {
  const steps = graph.scenes.map((scene, index) => {
    const stepId = scene.name.toUpperCase().replace(/[^A-Z0-9_]/g, '_');

    // Build transitions from scene transitions + timer actions
    const transitions = scene.transitions.map((t, ti) => ({
      event_type: t.condition ? 'serial' : 'button',
      event_name: t.condition || `TR_${ti + 1}`,
      target_step_id: t.targetScene.toUpperCase().replace(/[^A-Z0-9_]/g, '_'),
      priority: 0,
      after_ms: 0,
    }));

    // Add timer-derived transitions
    transitions.push(...actionsToFirmwareTransitions(scene.actions, index));

    return {
      step_id: stepId,
      screen_scene_id: stepId,
      audio_pack_id: '',
      actions: [],
      apps: [],
      transitions,
    };
  });

  // Map puzzles to firmware events
  const puzzleEvents = graph.puzzles.map((p) => ({
    puzzle_id: p.name.toUpperCase().replace(/[^A-Z0-9_]/g, '_'),
    type: p.type,
    solution: p.solution ?? '',
    hints_count: p.hints.length,
  }));

  const firmware = {
    initial_step: steps[0]?.step_id ?? 'STEP_BOOT',
    steps,
    ...(puzzleEvents.length > 0 ? { puzzles: puzzleEvents } : {}),
  };

  return YAML.stringify({ firmware });
}

// ─── ScenarioGraph → Display YAML ───

function actionToDisplay(action: SceneAction): Record<string, unknown> {
  if (action.kind === 'timer') {
    return {
      type: 'timer',
      seconds: action.seconds,
      on_expire: action.onExpire.map(actionToDisplay),
    };
  }
  return {
    type: 'variable_set',
    name: action.name,
    value: action.value,
  };
}

/**
 * Project ScenarioGraph to a rich human-readable YAML format
 */
export function scenarioGraphToDisplayYaml(graph: ScenarioGraph): string {
  const scenes = graph.scenes.map((scene) => ({
    name: scene.name,
    description: scene.description,
    duration_max_s: scene.durationMax,
    actions: scene.actions.map(actionToDisplay),
    transitions: scene.transitions.map((t) => ({
      target: t.targetScene,
      condition: t.condition || undefined,
    })),
  }));

  const puzzles = graph.puzzles.map((p) => ({
    name: p.name,
    type: p.type,
    solution: p.solution ?? undefined,
    hints: p.hints.map((h) => ({ level: h.level, text: h.text })),
  }));

  const npc = graph.npcActions.map((a) => {
    const entry: Record<string, unknown> = { type: a.type };
    if (a.text !== undefined) entry.text = a.text;
    if (a.mood !== undefined) entry.mood = a.mood;
    if (a.level !== undefined) entry.level = a.level;
    if (a.puzzleId !== undefined) entry.puzzle_id = a.puzzleId;
    if (a.condition) entry.condition = a.condition;
    if (a.systemPrompt) entry.system_prompt = a.systemPrompt;
    return entry;
  });

  const doc: Record<string, unknown> = { scenes };
  if (puzzles.length > 0) doc.puzzles = puzzles;
  if (npc.length > 0) doc.npc = npc;

  return YAML.stringify(doc);
}
