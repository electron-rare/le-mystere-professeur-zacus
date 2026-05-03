import { useEffect } from 'react';
import { useEditorStore } from '../stores/editorStore.js';
import { useRuntimeStore } from '../stores/runtimeStore.js';

const DEBOUNCE_MS = 500;

/**
 * Live-diff hook: subscribes to editor YAML changes, debounces 500ms,
 * and updates runtimeStore.pendingIr. The 3D stage's stale badge is
 * driven by runtimeStore.isStale, which the store derives from
 * pendingIr vs currentIr equality.
 *
 * Run is a separate user action (StagePane button) — this hook only
 * declares "the user has paused editing for 500ms, here is the latest
 * snapshot to consider". It never auto-replays the simulation, by
 * design (Three.js teardown is too expensive for keystroke-rate).
 */
export function useLiveDiff(): void {
  const blocklyJson = useEditorStore((s) => s.blocklyJson);
  const setPendingIr = useRuntimeStore((s) => s.setPendingIr);

  useEffect(() => {
    if (blocklyJson === null) {
      setPendingIr(null);
      return;
    }
    const handle = setTimeout(() => {
      setPendingIr(blocklyJson);
    }, DEBOUNCE_MS);
    return () => clearTimeout(handle);
  }, [blocklyJson, setPendingIr]);
}
