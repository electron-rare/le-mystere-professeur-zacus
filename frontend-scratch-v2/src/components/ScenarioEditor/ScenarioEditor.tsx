import { useEffect, useRef, useState, useCallback, lazy, Suspense } from 'react';
import * as Blockly from 'blockly';
import 'blockly/blocks';
import { ensureScenarioBlocks } from './blocks/scene';
import { registerPuzzleBlocks } from './blocks/puzzle';
import { registerNpcBlocks } from './blocks/npc';
import { registerHardwareBlocks } from './blocks/hardware';
import { registerDeployBlocks } from './blocks/deploy';
import { SCENARIO_TOOLBOX } from './toolbox';
import {
  buildScenarioGraph,
  scenarioGraphToFirmwareYaml,
  scenarioGraphToDisplayYaml,
} from './generators/yaml';

const LazyMonacoEditor = lazy(() => import('@monaco-editor/react'));

type YamlMode = 'display' | 'firmware';

export function ScenarioEditor() {
  const hostRef = useRef<HTMLDivElement | null>(null);
  const workspaceRef = useRef<Blockly.WorkspaceSvg | null>(null);
  const debounceRef = useRef<ReturnType<typeof setTimeout> | null>(null);
  const [scenarioName, setScenarioName] = useState('new_scenario');
  const [yamlOutput, setYamlOutput] = useState('');
  const [yamlMode, setYamlMode] = useState<YamlMode>('display');
  const [sceneCount, setSceneCount] = useState(0);

  const regenerateYaml = useCallback(
    (workspace: Blockly.WorkspaceSvg, mode: YamlMode) => {
      const graph = buildScenarioGraph(workspace);
      setSceneCount(graph.scenes.length);
      const yaml =
        mode === 'firmware'
          ? scenarioGraphToFirmwareYaml(graph)
          : scenarioGraphToDisplayYaml(graph);
      setYamlOutput(yaml);
    },
    [],
  );

  useEffect(() => {
    if (!hostRef.current) return;

    ensureScenarioBlocks();
    registerPuzzleBlocks();
    registerNpcBlocks();
    registerHardwareBlocks();
    registerDeployBlocks();
    const workspace = Blockly.inject(hostRef.current, {
      toolbox: SCENARIO_TOOLBOX,
      trashcan: true,
      grid: { spacing: 20, length: 3, colour: '#3a3f52', snap: true },
      zoom: { controls: true, wheel: true, startScale: 0.95 },
    });
    workspaceRef.current = workspace;

    // Add a starter scene block
    const block = workspace.newBlock('scenario_scene');
    block.setFieldValue('SCENE_INTRO', 'NAME');
    block.setFieldValue('Introduction scene', 'DESCRIPTION');
    block.initSvg();
    block.render();
    block.moveBy(40, 40);

    const onChange = (event: Blockly.Events.Abstract) => {
      // Filter out UI-only events (viewport, click, etc.)
      if (event.isUiEvent) return;
      if (debounceRef.current) clearTimeout(debounceRef.current);
      debounceRef.current = setTimeout(() => {
        regenerateYaml(workspace, yamlMode);
      }, 150);
    };

    workspace.addChangeListener(onChange);
    // Initial generation
    regenerateYaml(workspace, yamlMode);

    return () => {
      if (debounceRef.current) clearTimeout(debounceRef.current);
      workspace.removeChangeListener(onChange);
      workspace.dispose();
      workspaceRef.current = null;
    };
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  // Re-generate when mode changes
  useEffect(() => {
    if (workspaceRef.current) {
      regenerateYaml(workspaceRef.current, yamlMode);
    }
  }, [yamlMode, regenerateYaml]);

  const handleExport = useCallback(() => {
    const blob = new Blob([yamlOutput], { type: 'text/yaml' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = `${scenarioName}.yaml`;
    a.click();
    URL.revokeObjectURL(url);
  }, [yamlOutput, scenarioName]);

  return (
    <div className="scenario-editor">
      <div className="scenario-editor-toolbar">
        <input
          type="text"
          value={scenarioName}
          onChange={(e) => setScenarioName(e.target.value)}
          aria-label="scenario name"
          placeholder="scenario name"
        />
        <select
          value={yamlMode}
          onChange={(e) => setYamlMode(e.target.value as YamlMode)}
          aria-label="YAML mode"
        >
          <option value="display">Display YAML</option>
          <option value="firmware">Firmware YAML</option>
        </select>
        <button type="button" onClick={handleExport}>
          Export YAML
        </button>
        <span className="scenario-editor-status">
          {sceneCount} scene(s)
        </span>
      </div>
      <div className="scenario-editor-split">
        <div ref={hostRef} className="scenario-editor-blockly" />
        <div className="scenario-editor-preview">
          <Suspense
            fallback={
              <div style={{ padding: '1rem', color: 'var(--text-muted)' }}>
                Loading editor...
              </div>
            }
          >
            <LazyMonacoEditor
              height="100%"
              defaultLanguage="yaml"
              value={yamlOutput}
              options={{
                minimap: { enabled: false },
                scrollBeyondLastLine: false,
                fontSize: 13,
                readOnly: true,
                wordWrap: 'on',
              }}
              theme="vs-dark"
            />
          </Suspense>
        </div>
      </div>
    </div>
  );
}
