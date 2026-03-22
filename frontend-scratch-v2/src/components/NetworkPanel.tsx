import { useState } from 'react';
import { networkReconnect, espnowOn, espnowOff } from '../lib/api';
import type { RuntimeState } from '../lib/useRuntimeStore';

type Props = { runtime: RuntimeState };

export function NetworkPanel({ runtime }: Props) {
  const [busy, setBusy] = useState(false);
  const [feedback, setFeedback] = useState('');
  const net = runtime.legacy?.network;

  const run = async (label: string, fn: () => Promise<unknown>) => {
    setBusy(true);
    setFeedback('');
    try {
      await fn();
      setFeedback(`${label} OK`);
    } catch (err) {
      setFeedback(
        `${label} failed: ${err instanceof Error ? err.message : err}`,
      );
    } finally {
      setBusy(false);
    }
  };

  return (
    <div className="network-panel">
      <h2>Network</h2>

      <section className="card">
        <h3>WiFi</h3>
        <div className="kv">
          <span>State</span>
          <span className="badge">{net?.state ?? '?'}</span>
        </div>
        <div className="kv">
          <span>IP</span>
          <span className="mono">{net?.ip ?? '?'}</span>
        </div>
        <div className="actions">
          <button
            type="button"
            disabled={busy}
            onClick={() => run('Reconnect', networkReconnect)}
          >
            Reconnect WiFi
          </button>
        </div>
      </section>

      <section className="card">
        <h3>ESP-NOW</h3>
        <div className="actions">
          <button
            type="button"
            disabled={busy}
            onClick={() => run('ESP-NOW On', espnowOn)}
          >
            Enable
          </button>
          <button
            type="button"
            disabled={busy}
            onClick={() => run('ESP-NOW Off', espnowOff)}
          >
            Disable
          </button>
        </div>
      </section>

      {feedback && <p className="feedback">{feedback}</p>}
    </div>
  );
}
