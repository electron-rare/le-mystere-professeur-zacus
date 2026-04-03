import React, { useEffect, useRef } from 'react';

const DASHBOARD_URL = import.meta.env.DEV
  ? 'http://localhost:5175'
  : './apps/dashboard/index.html';

interface Props {
  onDeviceStatusChange: (status: 'connected' | 'partial' | 'disconnected') => void;
}

export function DashboardTab({ onDeviceStatusChange }: Props): React.JSX.Element {
  const webviewRef = useRef<Electron.WebviewTag>(null);

  useEffect(() => {
    const wv = webviewRef.current;
    if (!wv) return;

    // Listen for device status messages from dashboard app
    const handleIpcMessage = (e: { channel: string; args: unknown[] }) => {
      if (e.channel === 'device-status') {
        onDeviceStatusChange(e.args[0] as 'connected' | 'partial' | 'disconnected');
      }
    };

    wv.addEventListener('ipc-message', handleIpcMessage as EventListener);
    return () => wv.removeEventListener('ipc-message', handleIpcMessage as EventListener);
  }, [onDeviceStatusChange]);

  return (
    <div style={{ width: '100%', height: '100%' }}>
      <webview
        ref={webviewRef}
        src={DASHBOARD_URL}
        style={{ width: '100%', height: '100%' }}
        allowpopups={'false' as unknown as boolean}
        nodeintegration={'false' as unknown as boolean}
        partition="persist:dashboard"
      />
    </div>
  );
}
