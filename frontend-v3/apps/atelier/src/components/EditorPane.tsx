import { lazy, Suspense, useCallback } from 'react';
import type * as Blockly from 'blockly';
import { useEditorStore } from '../stores/editorStore.js';
import { useValidationStore } from '../stores/validationStore.js';

const BlocklyWorkspaceLazy = lazy(async () => {
  const blocksMod = await import('../blocks/index.js');
  blocksMod.registerAllBlocks();
  const wsMod = await import('./BlocklyWorkspace.js');
  return { default: wsMod.BlocklyWorkspace };
});

export function EditorPane() {
  const setBlocklyJson = useEditorStore((s) => s.setBlocklyJson);
  const pushValidation = useValidationStore((s) => s.push);
  const clearValidation = useValidationStore((s) => s.clear);

  const handleWorkspaceChange = useCallback(
    async (ws: Blockly.WorkspaceSvg) => {
      const { exportWorkspaceToYaml } = await import('../lib/yaml-export.js');
      const yaml = exportWorkspaceToYaml(ws);
      setBlocklyJson(yaml);

      clearValidation('schema');
      clearValidation('compile');

      try {
        const { parseScenarioYaml } = await import('@zacus/shared');
        const { validateScenario } = await import('../lib/validator.js');
        const scenario = parseScenarioYaml(yaml);
        const result = validateScenario(scenario);
        result.errors.forEach((msg, i) =>
          pushValidation({
            id: `schema-e-${i}`,
            severity: 'error',
            message: msg,
            source: 'schema',
          }),
        );
        result.warnings.forEach((msg, i) =>
          pushValidation({
            id: `schema-w-${i}`,
            severity: 'warning',
            message: msg,
            source: 'schema',
          }),
        );
      } catch (err) {
        pushValidation({
          id: 'parse-err',
          severity: 'error',
          message: err instanceof Error ? err.message : String(err),
          source: 'compile',
        });
      }
    },
    [setBlocklyJson, pushValidation, clearValidation],
  );

  return (
    <Suspense
      fallback={
        <div className="atelier-pane atelier-pane--editor">
          <span>Loading Blockly…</span>
        </div>
      }
    >
      <BlocklyWorkspaceLazy onWorkspaceChange={handleWorkspaceChange} />
    </Suspense>
  );
}
