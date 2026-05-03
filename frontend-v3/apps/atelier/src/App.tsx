import { Layout } from './components/Layout.js';
import { useLiveDiff } from './lib/useLiveDiff.js';
import { useEditorStore } from './stores/editorStore.js';
import { useRuntimeStore } from './stores/runtimeStore.js';
import { useSimStore } from './stores/simStore.js';
import { useValidationStore } from './stores/validationStore.js';

// Dev-only test hooks — used by e2e/ specs to drive store state without
// the full Blockly drag-and-drop dance (which is brittle in headless).
// Stripped from production by Vite's import.meta.env.DEV check.
if (import.meta.env.DEV) {
  (window as unknown as { __atelierStores?: unknown }).__atelierStores = {
    editor: useEditorStore,
    runtime: useRuntimeStore,
    sim: useSimStore,
    validation: useValidationStore,
  };
}

export function App() {
  useLiveDiff();
  return <Layout />;
}
