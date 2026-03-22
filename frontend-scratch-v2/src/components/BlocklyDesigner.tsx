import { useEffect, useMemo, useRef, useState } from 'react';
import * as Blockly from 'blockly';
import 'blockly/blocks';
import {
  buildScenarioFromBlocks,
  scenarioToYaml,
  validateScenarioDocument,
  parseYamlToSteps,
} from '../lib/scenario';
import {
  compileScenarioDocumentToRuntime3,
  runtime3ToJson,
  validateRuntime3Document,
} from '../lib/runtime3';
import type { ScenarioStep } from '../types';

const TOOLBOX: Blockly.utils.toolbox.ToolboxInfo = {
  kind: 'categoryToolbox',
  contents: [
    {
      kind: 'category',
      name: 'Steps',
      colour: '#3b82f6',
      contents: [{ kind: 'block', type: 'zacus_step' }],
    },
    {
      kind: 'category',
      name: 'Transitions',
      colour: '#22c55e',
      contents: [{ kind: 'block', type: 'zacus_transition' }],
    },
  ],
};

let blocksRegistered = false;

function ensureCustomBlocks(): void {
  if (blocksRegistered) return;

  Blockly.defineBlocksWithJsonArray([
    {
      type: 'zacus_step',
      message0: 'Step %1 Scene %2 Audio %3',
      args0: [
        { type: 'field_input', name: 'STEP_ID', text: 'STEP_NEW' },
        { type: 'field_input', name: 'SCENE_ID', text: 'SCENE_NEW' },
        { type: 'field_input', name: 'AUDIO_PACK', text: '' },
      ],
      message1: '%1',
      args1: [{ type: 'input_statement', name: 'TRANSITIONS' }],
      previousStatement: null,
      nextStatement: null,
      colour: 210,
      tooltip: 'A scenario step with its screen scene and audio pack',
    },
    {
      type: 'zacus_transition',
      message0: 'on %1 : %2 → %3 after %4 ms prio %5',
      args0: [
        {
          type: 'field_dropdown',
          name: 'EVENT_TYPE',
          options: [
            ['button', 'button'],
            ['serial', 'serial'],
            ['timer', 'timer'],
            ['audio_done', 'audio_done'],
            ['unlock', 'unlock'],
            ['espnow', 'espnow'],
            ['action', 'action'],
          ],
        },
        { type: 'field_input', name: 'EVENT_NAME', text: 'BTN_NEXT' },
        { type: 'field_input', name: 'TARGET', text: 'STEP_NEXT' },
        { type: 'field_number', name: 'AFTER_MS', value: 0, min: 0, precision: 1 },
        { type: 'field_number', name: 'PRIORITY', value: 0, min: 0, precision: 1 },
      ],
      previousStatement: null,
      nextStatement: null,
      colour: 140,
      tooltip: 'A transition triggered by an event',
    },
  ]);

  blocksRegistered = true;
}

function readScenarioSteps(workspace: Blockly.WorkspaceSvg): ScenarioStep[] {
  const steps: ScenarioStep[] = [];
  const seenBlocks = new Set<string>();

  const readTransitions = (block: Blockly.Block): ScenarioStep['transitions'] => {
    const transitions: NonNullable<ScenarioStep['transitions']> = [];
    let transitionBlock = block.getInputTargetBlock('TRANSITIONS');
    while (transitionBlock) {
      if (transitionBlock.type === 'zacus_transition') {
        transitions.push({
          eventType: transitionBlock.getFieldValue('EVENT_TYPE') ?? 'serial',
          eventName: transitionBlock.getFieldValue('EVENT_NAME') ?? 'BTN_NEXT',
          targetStepId: transitionBlock.getFieldValue('TARGET') ?? 'STEP_NEXT',
          afterMs: Number(transitionBlock.getFieldValue('AFTER_MS') ?? 0),
          priority: Number(transitionBlock.getFieldValue('PRIORITY') ?? 0),
        });
      }
      transitionBlock = transitionBlock.getNextBlock();
    }
    return transitions;
  };

  const collectChain = (startBlock: Blockly.Block): void => {
    let cursor: Blockly.Block | null = startBlock;
    while (cursor) {
      if (seenBlocks.has(cursor.id)) break;
      seenBlocks.add(cursor.id);

      if (cursor.type === 'zacus_step') {
        steps.push({
          stepId: cursor.getFieldValue('STEP_ID') ?? '',
          sceneId: cursor.getFieldValue('SCENE_ID') ?? '',
          audioPack: cursor.getFieldValue('AUDIO_PACK') ?? '',
          transitions: readTransitions(cursor),
        });
      }
      cursor = cursor.getNextBlock();
    }
  };

  for (const block of workspace.getTopBlocks(true)) {
    if (block.type === 'zacus_step') collectChain(block);
  }

  if (steps.length === 0) {
    for (const block of workspace.getAllBlocks(false)) {
      if (block.type === 'zacus_step') collectChain(block);
    }
  }

  // Deduplicate step IDs — keep first occurrence, warn on duplicates
  const seenStepIds = new Set<string>();
  const deduped: ScenarioStep[] = [];
  for (const step of steps) {
    if (seenStepIds.has(step.stepId)) {
      console.warn(
        `[BlocklyDesigner] Duplicate step ID "${step.stepId}" detected — skipping duplicate.`,
      );
      continue;
    }
    seenStepIds.add(step.stepId);
    deduped.push(step);
  }

  return deduped;
}

function loadStepsIntoWorkspace(
  workspace: Blockly.WorkspaceSvg,
  steps: ScenarioStep[],
): void {
  workspace.clear();
  let prevBlock: Blockly.Block | null = null;
  const Y_START = 40;
  const X_START = 40;

  for (const step of steps) {
    const block = workspace.newBlock('zacus_step');
    block.setFieldValue(step.stepId || 'STEP_NEW', 'STEP_ID');
    block.setFieldValue(step.sceneId || 'SCENE_NEW', 'SCENE_ID');
    block.setFieldValue(step.audioPack || '', 'AUDIO_PACK');
    block.initSvg();
    block.render();

    const transitions = step.transitions ?? [];
    let previousTransitionBlock: Blockly.Block | null = null;
    for (const transition of transitions) {
      const transitionBlock = workspace.newBlock('zacus_transition');
      transitionBlock.setFieldValue(transition.eventType, 'EVENT_TYPE');
      transitionBlock.setFieldValue(transition.eventName, 'EVENT_NAME');
      transitionBlock.setFieldValue(transition.targetStepId, 'TARGET');
      transitionBlock.setFieldValue(String(transition.afterMs), 'AFTER_MS');
      transitionBlock.setFieldValue(String(transition.priority), 'PRIORITY');
      transitionBlock.initSvg();
      transitionBlock.render();

      if (!previousTransitionBlock) {
        const inputConnection = block.getInput('TRANSITIONS')?.connection;
        if (inputConnection && transitionBlock.previousConnection) {
          inputConnection.connect(transitionBlock.previousConnection);
        }
      } else if (
        previousTransitionBlock.nextConnection &&
        transitionBlock.previousConnection
      ) {
        previousTransitionBlock.nextConnection.connect(
          transitionBlock.previousConnection,
        );
      }

      previousTransitionBlock = transitionBlock;
    }

    if (prevBlock) {
      if (prevBlock.nextConnection && block.previousConnection) {
        prevBlock.nextConnection.connect(block.previousConnection);
      }
    } else {
      block.moveBy(X_START, Y_START);
    }
    prevBlock = block;
  }
}

function addStarterBlocks(workspace: Blockly.WorkspaceSvg): void {
  loadStepsIntoWorkspace(workspace, [
    {
      stepId: 'STEP_U_SON_PROTO',
      sceneId: 'SCENE_U_SON_PROTO',
      transitions: [
        {
          eventType: 'button',
          eventName: 'ANY',
          targetStepId: 'STEP_LA_DETECTOR',
          priority: 0,
          afterMs: 0,
        },
      ],
    },
    { stepId: 'STEP_LA_DETECTOR', sceneId: 'SCENE_LA_DETECTOR', transitions: [] },
  ]);
}

type BlocklyDesignerProps = {
  onDraftChange: (draft: { yaml: string; runtime3Json: string }) => void;
};

export function BlocklyDesigner({ onDraftChange }: BlocklyDesignerProps) {
  const hostRef = useRef<HTMLDivElement | null>(null);
  const workspaceRef = useRef<Blockly.WorkspaceSvg | null>(null);
  const [scenarioId, setScenarioId] = useState('zacus_v2_new');
  const [steps, setSteps] = useState<ScenarioStep[]>([]);
  const [copyInfo, setCopyInfo] = useState('');
  const fileInputRef = useRef<HTMLInputElement | null>(null);

  useEffect(() => {
    if (!hostRef.current) return;

    ensureCustomBlocks();
    const workspace = Blockly.inject(hostRef.current, {
      toolbox: TOOLBOX,
      trashcan: true,
      grid: { spacing: 20, length: 3, colour: '#3a3f52', snap: true },
      zoom: { controls: true, wheel: true, startScale: 0.95 },
    });
    workspaceRef.current = workspace;
    addStarterBlocks(workspace);

    const onChange = () => {
      setSteps(readScenarioSteps(workspace));
      setCopyInfo('');
    };

    workspace.addChangeListener(onChange);
    onChange();

    return () => {
      workspace.removeChangeListener(onChange);
      workspace.dispose();
      workspaceRef.current = null;
    };
  }, []);

  const generated = useMemo(() => {
    const scenarioDocument = buildScenarioFromBlocks(scenarioId, steps);
    const runtime3Document = compileScenarioDocumentToRuntime3(scenarioDocument);
    return {
      scenarioDocument,
      yaml: scenarioToYaml(scenarioDocument),
      validation: validateScenarioDocument(scenarioDocument),
      runtime3Json: runtime3ToJson(runtime3Document),
      runtime3Validation: validateRuntime3Document(runtime3Document),
    };
  }, [scenarioId, steps]);

  useEffect(() => {
    onDraftChange({
      yaml: generated.yaml,
      runtime3Json: generated.runtime3Json,
    });
  }, [generated.runtime3Json, generated.yaml, onDraftChange]);

  const handleReset = () => {
    if (!workspaceRef.current) return;
    addStarterBlocks(workspaceRef.current);
    setSteps(readScenarioSteps(workspaceRef.current));
  };

  const handleCopy = async () => {
    try {
      await navigator.clipboard.writeText(generated.yaml);
      setCopyInfo('YAML copied.');
    } catch {
      setCopyInfo('Copy failed.');
    }
  };

  const handleImportYaml = (yamlStr: string) => {
    const result = parseYamlToSteps(yamlStr);
    if ('error' in result) {
      setCopyInfo(`Import error: ${result.error}`);
      return;
    }
    setScenarioId(result.id);
    if (workspaceRef.current) {
      loadStepsIntoWorkspace(workspaceRef.current, result.steps);
      setSteps(readScenarioSteps(workspaceRef.current));
    }
    setCopyInfo(`Imported ${result.steps.length} steps from "${result.id}".`);
  };

  const handleFileImport = () => {
    fileInputRef.current?.click();
  };

  const handleFileChange = (e: React.ChangeEvent<HTMLInputElement>) => {
    const file = e.target.files?.[0];
    if (!file) return;
    const reader = new FileReader();
    reader.onload = () => {
      if (typeof reader.result === 'string') {
        handleImportYaml(reader.result);
      }
    };
    reader.readAsText(file);
    e.target.value = '';
  };

  return (
    <>
      <div className="toolbar">
        <input
          type="text"
          value={scenarioId}
          onChange={(event) => setScenarioId(event.target.value)}
          aria-label="scenario id"
          placeholder="scenario id"
        />
        <button type="button" onClick={handleReset}>
          Reset
        </button>
        <button type="button" onClick={handleCopy}>
          Copy YAML
        </button>
        <button type="button" onClick={handleFileImport}>
          Import YAML
        </button>
        <input
          ref={fileInputRef}
          type="file"
          accept=".yaml,.yml"
          style={{ display: 'none' }}
          onChange={handleFileChange}
        />
      </div>

      <div ref={hostRef} className="blockly-host" />

      <div
        className={`status-line ${
          generated.validation.ok && generated.runtime3Validation.ok ? '' : 'error'
        }`}
        role="status"
      >
        {generated.validation.ok && generated.runtime3Validation.ok
          ? `Ready — ${steps.length} step(s), ${
              steps.reduce(
                (total, step) => total + (step.transitions?.length ?? 0),
                0,
              )
            } transition(s). ${copyInfo}`
          : `Invalid: ${
              !generated.validation.ok
                ? generated.validation.error
                : generated.runtime3Validation.ok
                  ? 'runtime3 validation error'
                  : generated.runtime3Validation.error
            }`}
      </div>
    </>
  );
}
