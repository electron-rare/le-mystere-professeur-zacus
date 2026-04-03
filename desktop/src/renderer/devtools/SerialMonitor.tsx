import React, { useState, useEffect, useRef, useCallback } from 'react';

interface LogEntry {
  port: string;
  timestamp: number;
  text: string;
  level: 'info' | 'warn' | 'error' | 'debug';
}

const LEVEL_COLORS: Record<LogEntry['level'], string> = {
  error: '#ef4444',
  warn:  '#f59e0b',
  info:  '#94a3b8',
  debug: '#475569',
};

function classifyLine(text: string): LogEntry['level'] {
  if (/error|fail|crash/i.test(text)) return 'error';
  if (/warn|caution/i.test(text)) return 'warn';
  if (/debug|verbose/i.test(text)) return 'debug';
  return 'info';
}

export function SerialMonitor(): React.JSX.Element {
  const [ports, setPorts] = useState<string[]>([]);
  const [connectedPorts, setConnectedPorts] = useState<Set<string>>(new Set());
  const [logs, setLogs] = useState<LogEntry[]>([]);
  const [filterPort, setFilterPort] = useState<string>('all');
  const [filterText, setFilterText] = useState('');
  const [filterLevel, setFilterLevel] = useState<'all' | LogEntry['level']>('all');
  const [paused, setPaused] = useState(false);
  const [sendText, setSendText] = useState('');
  const logEndRef = useRef<HTMLDivElement>(null);

  // Load port list on mount
  useEffect(() => {
    window.zacus.serial.list().then(serialPorts => {
      setPorts(serialPorts.map(p => p.path));
    });
  }, []);

  // Listen for incoming serial data and plug/unplug events
  useEffect(() => {
    window.zacus.serial.onData((port, data) => {
      if (paused) return;
      const lines = data.split('\n').filter(Boolean);
      setLogs(prev => [
        ...prev.slice(-2000), // keep last 2000 lines
        ...lines.map(text => ({
          port,
          timestamp: Date.now(),
          text: text.trim(),
          level: classifyLine(text),
        })),
      ]);
    });

    window.zacus.serial.onPlugged(port => {
      setPorts(prev => [...new Set([...prev, port])]);
    });

    window.zacus.serial.onUnplugged(port => {
      setConnectedPorts(prev => {
        const s = new Set(prev);
        s.delete(port);
        return s;
      });
    });
  }, [paused]);

  // Auto-scroll to bottom when new logs arrive
  useEffect(() => {
    if (!paused) {
      logEndRef.current?.scrollIntoView({ behavior: 'smooth' });
    }
  }, [logs, paused]);

  const connect = useCallback(async (port: string) => {
    const ok = await window.zacus.serial.connect(port, 115200);
    if (ok) setConnectedPorts(prev => new Set([...prev, port]));
  }, []);

  const disconnect = useCallback(async (port: string) => {
    await window.zacus.serial.disconnect(port);
    setConnectedPorts(prev => {
      const s = new Set(prev);
      s.delete(port);
      return s;
    });
  }, []);

  const sendCommand = useCallback(async () => {
    if (!sendText.trim()) return;
    // Send to all connected ports or the filtered port
    const targets = filterPort !== 'all'
      ? [filterPort]
      : [...connectedPorts];
    for (const port of targets) {
      await window.zacus.serial.write(port, sendText + '\n');
    }
    setSendText('');
  }, [sendText, filterPort, connectedPorts]);

  const visibleLogs = logs.filter(e => {
    if (filterPort !== 'all' && e.port !== filterPort) return false;
    if (filterLevel !== 'all' && e.level !== filterLevel) return false;
    if (filterText && !e.text.toLowerCase().includes(filterText.toLowerCase())) return false;
    return true;
  });

  const exportLogs = useCallback(() => {
    const text = visibleLogs
      .map(e => `[${new Date(e.timestamp).toISOString()}] [${e.port}] ${e.text}`)
      .join('\n');
    window.zacus.file.save(text, 'serial_log.txt');
  }, [visibleLogs]);

  return (
    <div className="serial-monitor">
      <div className="serial-toolbar">
        <select
          value={filterPort}
          onChange={e => setFilterPort(e.target.value)}
          className="select"
        >
          <option value="all">All ports</option>
          {ports.map(p => (
            <option key={p} value={p}>{p.split('/').pop()}</option>
          ))}
        </select>

        <select
          value={filterLevel}
          onChange={e => setFilterLevel(e.target.value as 'all' | LogEntry['level'])}
          className="select"
        >
          <option value="all">All levels</option>
          <option value="error">Error</option>
          <option value="warn">Warn</option>
          <option value="info">Info</option>
          <option value="debug">Debug</option>
        </select>

        <input
          className="input"
          placeholder="Filter text…"
          value={filterText}
          onChange={e => setFilterText(e.target.value)}
        />

        <button className="btn" onClick={() => setPaused(p => !p)}>
          {paused ? '▶ Resume' : '⏸ Pause'}
        </button>

        <button className="btn" onClick={() => setLogs([])}>Clear</button>
        <button className="btn" onClick={exportLogs} disabled={visibleLogs.length === 0}>
          Export
        </button>
      </div>

      <div className="port-list">
        {ports.map(port => (
          <button
            key={port}
            className={`btn small ${connectedPorts.has(port) ? 'active' : ''}`}
            onClick={() => connectedPorts.has(port) ? disconnect(port) : connect(port)}
          >
            {connectedPorts.has(port) ? '● ' : '○ '}
            {port.split('/').pop()}
          </button>
        ))}
      </div>

      <div className="log-output">
        {visibleLogs.map((entry, i) => (
          <div
            key={i}
            className="log-line"
            style={{ color: LEVEL_COLORS[entry.level] }}
          >
            <span className="log-time">
              {new Date(entry.timestamp).toISOString().slice(11, 23)}
            </span>
            <span className="log-port">[{entry.port.split('/').pop()}]</span>
            <span className="log-text">{entry.text}</span>
          </div>
        ))}
        <div ref={logEndRef} />
      </div>

      <div className="serial-send-bar">
        <input
          className="input mono"
          placeholder="Send command…"
          value={sendText}
          onChange={e => setSendText(e.target.value)}
          onKeyDown={e => { if (e.key === 'Enter') sendCommand(); }}
          disabled={connectedPorts.size === 0}
        />
        <button
          className="btn primary"
          onClick={sendCommand}
          disabled={connectedPorts.size === 0 || !sendText.trim()}
        >
          Send
        </button>
      </div>
    </div>
  );
}
