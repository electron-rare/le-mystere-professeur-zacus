import { lazy, Suspense } from 'react';

const BlocklyWorkspaceLazy = lazy(async () => {
  await import('blockly');
  return {
    default: function BlocklyWorkspacePlaceholder() {
      return (
        <div className="atelier-pane atelier-pane--editor">
          <span>Blockly workspace (P3)</span>
        </div>
      );
    },
  };
});

export function EditorPane() {
  return (
    <Suspense
      fallback={
        <div className="atelier-pane atelier-pane--editor">
          <span>Loading Blockly…</span>
        </div>
      }
    >
      <BlocklyWorkspaceLazy />
    </Suspense>
  );
}
