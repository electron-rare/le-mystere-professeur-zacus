import React, { useEffect, useRef, useCallback } from 'react';

// EditorTab loads the frontend-v3 editor app in a webview.
// In development, it points to the Vite dev server.
// In production, it loads the built app from the renderer bundle.
const EDITOR_URL = import.meta.env.DEV
  ? 'http://localhost:5174'   // frontend-v3/apps/editor dev server
  : './apps/editor/index.html';

export function EditorTab(): React.JSX.Element {
  const webviewRef = useRef<Electron.WebviewTag>(null);

  // Forward menu events into the webview
  useEffect(() => {
    const menuEvents = ['save', 'open', 'compile', 'validate', 'export-sd'];
    menuEvents.forEach(evt => {
      window.zacus.menu.on(evt, () => {
        webviewRef.current?.send(`menu:${evt}`);
      });
    });
  }, []);

  const handleNewWindow = useCallback((e: Event) => {
    e.preventDefault();
  }, []);

  return (
    <div style={{ width: '100%', height: '100%' }}>
      <webview
        ref={webviewRef}
        src={EDITOR_URL}
        style={{ width: '100%', height: '100%' }}
        allowpopups={'false' as unknown as boolean}
        onNewWindow={handleNewWindow as unknown as React.ReactEventHandler}
        nodeintegration={'false' as unknown as boolean}
        partition="persist:editor"
      />
    </div>
  );
}
