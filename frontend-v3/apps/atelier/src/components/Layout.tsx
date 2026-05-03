import { useEffect, useRef } from 'react';
import {
  Panel,
  PanelGroup,
  PanelResizeHandle,
  type ImperativePanelHandle,
} from 'react-resizable-panels';
import { EditorPane } from './EditorPane.js';
import { StagePane } from './StagePane.js';
import { ConsolePane } from './ConsolePane.js';

/**
 * Atelier shell: 2 columns top + 1 console row bottom.
 * ⌘B / Ctrl+B toggles the stage panel for fullscreen Blockly debug.
 */
export function Layout() {
  const stageRef = useRef<ImperativePanelHandle>(null);

  useEffect(() => {
    const toggleStage = () => {
      const panel = stageRef.current;
      if (!panel) return;
      if (panel.isCollapsed()) panel.expand();
      else panel.collapse();
    };

    const handler = (e: KeyboardEvent) => {
      const cmd = e.metaKey || e.ctrlKey;
      if (cmd && (e.key === 'b' || e.key === 'B')) {
        e.preventDefault();
        toggleStage();
      }
    };
    window.addEventListener('keydown', handler);

    // Dev-only test hook: lets e2e specs trigger the toggle without
    // relying on cross-platform modifier-key behavior in headless Chromium.
    if (import.meta.env.DEV) {
      (window as unknown as { __atelierToggleStage?: () => void }).__atelierToggleStage =
        toggleStage;
    }

    return () => {
      window.removeEventListener('keydown', handler);
      if (import.meta.env.DEV) {
        delete (window as unknown as { __atelierToggleStage?: () => void })
          .__atelierToggleStage;
      }
    };
  }, []);

  return (
    <div className="atelier-shell">
      <PanelGroup direction="vertical">
        <Panel defaultSize={75} minSize={40}>
          <PanelGroup direction="horizontal">
            <Panel defaultSize={55} minSize={25}>
              <EditorPane />
            </Panel>
            <PanelResizeHandle className="atelier-resizer atelier-resizer--vertical" />
            <Panel
              ref={stageRef}
              defaultSize={45}
              minSize={20}
              collapsible
              collapsedSize={0}
            >
              <StagePane />
            </Panel>
          </PanelGroup>
        </Panel>
        <PanelResizeHandle className="atelier-resizer atelier-resizer--horizontal" />
        <Panel defaultSize={25} minSize={10}>
          <ConsolePane />
        </Panel>
      </PanelGroup>
    </div>
  );
}
