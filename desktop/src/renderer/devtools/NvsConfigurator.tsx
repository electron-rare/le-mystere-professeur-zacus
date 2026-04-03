import React, { useState, useCallback } from 'react';

interface NVSSetting {
  key: string;
  type: 'string' | 'uint8' | 'float' | 'blob';
  value: string;
  puzzle: string;
  description: string;
}

const DEFAULT_SETTINGS: NVSSetting[] = [
  { key: 'wifi_ssid',   type: 'string', value: 'ZacusNet',    puzzle: 'all', description: 'WiFi network SSID' },
  { key: 'wifi_pass',   type: 'string', value: '',            puzzle: 'all', description: 'WiFi password' },
  { key: 'gm_ip',       type: 'string', value: '192.168.4.1', puzzle: 'all', description: 'Game Master IP address' },
  { key: 'code_digits', type: 'blob',   value: '1 4',         puzzle: 'all', description: 'Code digits (space-separated)' },
  { key: 'morse_msg',   type: 'string', value: 'ZACUS',       puzzle: 'p5',  description: 'Morse code message' },
  { key: 'rf_freq',     type: 'float',  value: '107.5',       puzzle: 'p4',  description: 'Target FM frequency (MHz)' },
  { key: 'nfc_uid_0',   type: 'string', value: '',            puzzle: 'p6',  description: 'NFC UID #1 (hex)' },
  { key: 'nfc_uid_1',   type: 'string', value: '',            puzzle: 'p6',  description: 'NFC UID #2 (hex)' },
];

const PUZZLE_FILTERS = ['all', 'p4', 'p5', 'p6'];

export function NvsConfigurator(): React.JSX.Element {
  const [settings, setSettings] = useState<NVSSetting[]>(DEFAULT_SETTINGS);
  const [targetPort, setTargetPort] = useState('');
  const [status, setStatus] = useState('');
  const [puzzleFilter, setPuzzleFilter] = useState('all');
  const [isWriting, setIsWriting] = useState(false);

  const updateSetting = useCallback((key: string, value: string) => {
    setSettings(prev => prev.map(s => s.key === key ? { ...s, value } : s));
  }, []);

  const writeAll = useCallback(async () => {
    if (!targetPort) {
      setStatus('Select a device port first');
      return;
    }

    setIsWriting(true);
    setStatus('Writing NVS settings…');
    let ok = 0;

    for (const setting of settings) {
      // Send "nvs set <key> <value>\n" via serial
      const cmd = `nvs set ${setting.key} ${setting.value}\n`;
      await window.zacus.serial.write(targetPort, cmd);
      await delay(200); // Wait for ESP32 to process
      ok++;
    }

    setIsWriting(false);
    setStatus(`Done: ${ok}/${settings.length} settings written`);
  }, [settings, targetPort]);

  const readAll = useCallback(async () => {
    if (!targetPort) {
      setStatus('Select a device port first');
      return;
    }
    setStatus('Reading NVS…');
    await window.zacus.serial.write(targetPort, 'nvs dump\n');
    setStatus('Sent nvs dump — check Serial Monitor for output');
  }, [targetPort]);

  const resetToDefaults = useCallback(() => {
    setSettings(DEFAULT_SETTINGS);
    setStatus('Reset to defaults');
  }, []);

  const filteredSettings = puzzleFilter === 'all'
    ? settings
    : settings.filter(s => s.puzzle === puzzleFilter || s.puzzle === 'all');

  return (
    <div className="nvs-configurator">
      <div className="panel-header">
        <h2>NVS Configurator</h2>
        <div className="toolbar-group">
          <input
            className="input mono"
            placeholder="/dev/cu.usbmodem…"
            value={targetPort}
            onChange={e => setTargetPort(e.target.value)}
          />
          <button className="btn" onClick={readAll} disabled={!targetPort}>
            Read
          </button>
          <button
            className="btn primary"
            onClick={writeAll}
            disabled={!targetPort || isWriting}
          >
            {isWriting ? 'Writing…' : 'Write All'}
          </button>
          <button className="btn" onClick={resetToDefaults}>
            Reset
          </button>
        </div>
      </div>

      {status && (
        <div className={`status-bar ${status.startsWith('Done') ? 'success' : ''}`}>
          {status}
        </div>
      )}

      <div className="puzzle-filter">
        {PUZZLE_FILTERS.map(f => (
          <button
            key={f}
            className={`btn small ${puzzleFilter === f ? 'active' : ''}`}
            onClick={() => setPuzzleFilter(f)}
          >
            {f === 'all' ? 'All puzzles' : f.toUpperCase()}
          </button>
        ))}
      </div>

      <table className="nvs-table">
        <thead>
          <tr>
            <th>Key</th>
            <th>Type</th>
            <th>Puzzle</th>
            <th>Value</th>
            <th>Description</th>
          </tr>
        </thead>
        <tbody>
          {filteredSettings.map(s => (
            <tr key={s.key}>
              <td className="mono">{s.key}</td>
              <td>
                <span className="badge">{s.type}</span>
              </td>
              <td>{s.puzzle}</td>
              <td>
                <input
                  className="input mono"
                  type={s.key.includes('pass') ? 'password' : 'text'}
                  value={s.value}
                  onChange={e => updateSetting(s.key, e.target.value)}
                />
              </td>
              <td className="text-dim">{s.description}</td>
            </tr>
          ))}
        </tbody>
      </table>
    </div>
  );
}

function delay(ms: number): Promise<void> {
  return new Promise(resolve => setTimeout(resolve, ms));
}
