import React, { useEffect, useRef, useState } from 'react';

type Status = 'running' | 'paused' | 'idle';

interface StoryStatus {
  status: Status;
  scenario_id: string;
  current_step: string;
  progress_pct: number;
}

interface AuditEvent {
  type: string;
  timestamp?: number;
  data?: any;
}

const LiveOrchestrator: React.FC = () => {
  const [status, setStatus] = useState<StoryStatus | null>(null);
  const [log, setLog] = useState<AuditEvent[]>([]);
  const [wsConnected, setWsConnected] = useState(false);
  const logRef = useRef<HTMLDivElement>(null);

  // Initial fetch
  useEffect(() => {
    fetch('/api/story/status')
      .then((res) => res.json())
      .then(setStatus)
      .catch(() => setStatus(null));
  }, []);

  // WebSocket
  useEffect(() => {
    const ws = new WebSocket(`ws://${window.location.hostname}:8080/api/story/stream`);
    ws.onopen = () => setWsConnected(true);
    ws.onclose = () => setWsConnected(false);
    ws.onerror = () => setWsConnected(false);
    ws.onmessage = (e) => {
      try {
        const msg = JSON.parse(e.data);
        if (msg.type === 'step_change' || msg.type === 'transition' || msg.type === 'status') {
          setStatus((prev) => ({ ...prev, ...msg.data }));
        }
        setLog((prev) => [...prev.slice(-99), msg]);
      } catch {}
    };
    return () => ws.close();
  }, []);

  // Auto-scroll log
  useEffect(() => {
    if (logRef.current) {
      logRef.current.scrollTop = logRef.current.scrollHeight;
    }
  }, [log]);

  const sendControl = async (endpoint: string) => {
    await fetch(`/api/story/${endpoint}`, { method: 'POST' });
  };

  return (
    <section aria-labelledby="orchestrator-title">
      <h2 id="orchestrator-title">Orchestrateur</h2>
      <div style={{ display: 'flex', gap: 24, flexWrap: 'wrap' }}>
        <div style={{ minWidth: 220 }}>
          <div>Status : <strong>{status?.status || '...'}</strong> {wsConnected ? '🟢' : '🔴'}</div>
          <div>Scénario : {status?.scenario_id || '-'}</div>
          <div>Étape : {status?.current_step || '-'}</div>
          <div style={{ margin: '8px 0' }}>
            <progress value={status?.progress_pct || 0} max={100} style={{ width: '100%' }} />
            <span>{status?.progress_pct ?? 0}%</span>
          </div>
          <div style={{ display: 'flex', gap: 8, marginTop: 8 }}>
            <button onClick={() => sendControl('pause')} disabled={status?.status !== 'running'}>Pause</button>
            <button onClick={() => sendControl('resume')} disabled={status?.status !== 'paused'}>Resume</button>
            <button onClick={() => sendControl('skip')} disabled={status?.status !== 'running'}>Skip</button>
          </div>
        </div>
        <div style={{ flex: 1, minWidth: 220 }}>
          <div>Audit log :</div>
          <div ref={logRef} style={{ height: 180, overflowY: 'auto', background: '#f8f8f8', border: '1px solid #ddd', borderRadius: 4, padding: 8, fontSize: 13 }}>
            {log.map((evt, i) => (
              <div key={i} style={{ color: evt.type === 'error' ? 'red' : undefined }}>
                [{evt.type}] {evt.timestamp ? new Date(evt.timestamp).toLocaleTimeString() : ''} {JSON.stringify(evt.data)}
              </div>
            ))}
          </div>
        </div>
      </div>
    </section>
  );
};

export default LiveOrchestrator;
