import { useEffect, useMemo, useRef, useState } from 'react';
import * as Blockly from 'blockly';
import 'blockly/blocks';
import {
  buildScenarioFromBlocks,
  scenarioToYaml,
  validateScenarioDocument,
} from '../lib/scenario';
import type { ScenarioStep } from '../types';

const TOOLBOX: Blockly.utils.toolbox.ToolboxInfo = {
  kind: 'categoryToolbox',
  contents: [
    {
      kind: 'category',
      name: 'Scenario',
      colour: '#3b82f6',
      contents: [
        {
          kind: 'block',
          type: 'zacus_step',
        },
      ],
    },
  ],
};

let blocksRegistered = false;

function ensureCustomBlocks(): void {
  if (blocksRegistered) {
    return;
  }

  Blockly.defineBlocksWithJsonArray([
    {
      type: 'zacus_step',
      message0: 'step %1 scene %2',
      args0: [
        {
          type: 'field_input',
          name: 'STEP_ID',
          text: 'STEP_NEW',
        },
        {
          type: 'field_input',
          name: 'SCENE_ID',
          text: 'SCENE_NEW',
        },
      ],
      previousStatement: null,
      nextStatement: null,
      colour: 210,
    },
  ]);

  blocksRegistered = true;
}

function readScenarioSteps(workspace: Blockly.WorkspaceSvg): ScenarioStep[] {
  const steps: ScenarioStep[] = [];
  const seenBlocks = new Set<string>();

  const collectChain = (startBlock: Blockly.Block): void => {
    let cursor: Blockly.Block | null = startBlock;
    while (cursor) {
      if (seenBlocks.has(cursor.id)) {
        break;
      }
      seenBlocks.add(cursor.id);

      if (cursor.type === 'zacus_step') {
        steps.push({
          stepId: cursor.getFieldValue('STEP_ID') ?? '',
          sceneId: cursor.getFieldValue('SCENE_ID') ?? '',
        });
      }
      cursor = cursor.getNextBlock();
    }
  };

  for (const block of workspace.getTopBlocks(true)) {
    if (block.type === 'zacus_step') {
      collectChain(block);
    }
  }

  if (steps.length === 0) {
    for (const block of workspace.getAllBlocks(false)) {
      if (block.type === 'zacus_step') {
        collectChain(block);
      }
    }
  }

  return steps;
}

function addStarterBlocks(workspace: Blockly.WorkspaceSvg): void {
  const first = workspace.newBlock('zacus_step');
  first.setFieldValue('STEP_U_SON_PROTO', 'STEP_ID');
  first.setFieldValue('SCENE_U_SON_PROTO', 'SCENE_ID');
  first.initSvg();
  first.render();
  first.moveBy(40, 40);

  const second = workspace.newBlock('zacus_step');
  second.setFieldValue('STEP_LA_DETECTOR', 'STEP_ID');
  second.setFieldValue('SCENE_LA_DETECTOR', 'SCENE_ID');
  second.initSvg();
  second.render();

  if (first.nextConnection && second.previousConnection) {
    first.nextConnection.connect(second.previousConnection);
  }
}

type BlocklyDesignerProps = {
  onYamlChange: (yaml: string) => void;
};

export function BlocklyDesigner({ onYamlChange }: BlocklyDesignerProps) {
  const hostRef = useRef<HTMLDivElement | null>(null);
  const workspaceRef = useRef<Blockly.WorkspaceSvg | null>(null);
  const [scenarioId, setScenarioId] = useState('zacus_v2_new');
  const [steps, setSteps] = useState<ScenarioStep[]>([]);
  const [copyInfo, setCopyInfo] = useState('');

  useEffect(() => {
    if (!hostRef.current) {
      return;
    }

    ensureCustomBlocks();
    const workspace = Blockly.inject(hostRef.current, {
      toolbox: TOOLBOX,
      trashcan: true,
      grid: {
        spacing: 20,
        length: 3,
        colour: '#d0d7de',
        snap: true,
      },
      zoom: {
        controls: true,
        wheel: true,
        startScale: 0.95,
      },
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
    return {
      yaml: scenarioToYaml(scenarioDocument),
      validation: validateScenarioDocument(scenarioDocument),
    };
  }, [scenarioId, steps]);

  useEffect(() => {
    onYamlChange(generated.yaml);
  }, [generated.yaml, onYamlChange]);

  const handleReset = () => {
    if (!workspaceRef.current) {
      return;
    }
    workspaceRef.current.clear();
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
          Reset blocks
        </button>
        <button type="button" onClick={handleCopy}>
          Copy YAML
        </button>
      </div>

      <div ref={hostRef} className="blockly-host" />

      <div
        className={`status-line ${generated.validation.ok ? '' : 'error'}`}
        role="status"
      >
        {generated.validation.ok
          ? `Ready - ${steps.length} step(s) detected. ${copyInfo}`
          : `Invalid draft: ${generated.validation.error}`}
      </div>
    </>
  );
}
