import { Panel, PanelGroup, PanelResizeHandle } from 'react-resizable-panels';
import { EditorPane } from './EditorPane.js';
import { StagePane } from './StagePane.js';
import { ConsolePane } from './ConsolePane.js';

export function Layout() {
  return (
    <div className="atelier-shell">
      <PanelGroup direction="vertical">
        <Panel defaultSize={75} minSize={40}>
          <PanelGroup direction="horizontal">
            <Panel defaultSize={55} minSize={25}>
              <EditorPane />
            </Panel>
            <PanelResizeHandle className="atelier-resizer atelier-resizer--vertical" />
            <Panel defaultSize={45} minSize={20}>
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
