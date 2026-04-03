import React, { useState, useEffect } from 'react';

interface BatteryReading {
  deviceId: string;
  name: string;
  pct: number;
  pack: string;
  remainingHours: number;
}

interface BatteryStatusMessage {
  type: string;
  device: string;
  battery_pct: number;
  remaining_hours?: number;
}

function BatteryBar({ pct }: { pct: number }): React.JSX.Element {
  const color =
    pct >= 50 ? 'var(--success, #22c55e)'
    : pct >= 20 ? 'var(--warning, #f59e0b)'
    : 'var(--error, #ef4444)';
  return (
    <div className="battery-bar-wrapper">
      <div
        className="battery-bar"
        style={{ width: `${pct}%`, backgroundColor: color }}
      />
    </div>
  );
}

const INITIAL_READINGS: BatteryReading[] = [
  { deviceId: 'box3',    name: 'BOX-3',     pct: 82, pack: 'Anker #1', remainingHours: 4.0 },
  { deviceId: 'p1_son',  name: 'P1 Son',    pct: 61, pack: 'Pack A',   remainingHours: 3.1 },
  { deviceId: 'p5_mor',  name: 'P5 Morse',  pct: 94, pack: 'Pack A',   remainingHours: 5.2 },
  { deviceId: 'p6_nfc',  name: 'P6 NFC',    pct: 78, pack: 'Pack B',   remainingHours: 4.0 },
  { deviceId: 'p7_cof',  name: 'P7 Coffre', pct: 31, pack: 'Pack B',   remainingHours: 1.5 },
];

export function BatteryDashboard(): React.JSX.Element {
  const [readings, setReadings] = useState<BatteryReading[]>(INITIAL_READINGS);

  // Listen for live battery status updates via WebSocket
  useEffect(() => {
    window.zacus.wifi.onWsMessage(rawData => {
      try {
        const msg = JSON.parse(rawData) as BatteryStatusMessage;
        if (msg.type === 'battery_status') {
          setReadings(prev => prev.map(r =>
            r.deviceId === msg.device
              ? {
                  ...r,
                  pct: msg.battery_pct,
                  remainingHours: msg.remaining_hours ?? msg.battery_pct / 20,
                }
              : r
          ));
        }
      } catch { /* not battery data */ }
    });
  }, []);

  const lowest = readings.reduce(
    (min, r) => r.pct < min.pct ? r : min,
    readings[0]
  );

  const criticalDevices = readings.filter(r => r.pct < 20);
  const warningDevices  = readings.filter(r => r.pct >= 20 && r.pct < 25);

  return (
    <div className="battery-dashboard">
      <div className="panel-header">
        <h2>Battery Dashboard</h2>
        <span className="text-dim">{readings.length} devices monitored</span>
      </div>

      {criticalDevices.length > 0 && (
        <div className="alert error">
          ⚠️ Critical: {criticalDevices.map(d => `${d.name} (${d.pct}%)`).join(', ')} — replace immediately
        </div>
      )}
      {warningDevices.length > 0 && criticalDevices.length === 0 && (
        <div className="alert warning">
          ⚠️ Low: {warningDevices.map(d => `${d.name} (${d.pct}%)`).join(', ')} — replace soon
        </div>
      )}
      {lowest && lowest.pct < 25 && criticalDevices.length === 0 && warningDevices.length === 0 && (
        <div className="alert warning">
          ⚠️ {lowest.name}: {lowest.pct}% — lowest battery
        </div>
      )}

      <div className="battery-list">
        {readings.map(r => (
          <div key={r.deviceId} className="battery-row">
            <div className="battery-name">{r.name}</div>
            <div className="battery-bar-container">
              <BatteryBar pct={r.pct} />
              <span className={`battery-pct ${r.pct < 20 ? 'error' : r.pct < 50 ? 'warn' : ''}`}>
                {r.pct}%
              </span>
            </div>
            <div className="battery-meta">
              <span className="text-dim">{r.pack}</span>
              <span className={r.remainingHours < 2 ? 'warn' : 'text-dim'}>
                ~{r.remainingHours.toFixed(1)}h remaining
              </span>
              {r.pct < 20 && <span className="badge error">⚠️ CRITICAL</span>}
              {r.pct >= 20 && r.pct < 25 && <span className="badge warning">⚠️ LOW</span>}
            </div>
          </div>
        ))}
      </div>

      <div className="battery-summary">
        <span className="text-dim">
          Avg: {Math.round(readings.reduce((s, r) => s + r.pct, 0) / readings.length)}% |{' '}
          Min: {lowest?.pct}% ({lowest?.name}) |{' '}
          Est. runtime: ~{Math.min(...readings.map(r => r.remainingHours)).toFixed(1)}h
        </span>
      </div>
    </div>
  );
}
