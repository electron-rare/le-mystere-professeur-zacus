import React, { useState, useEffect, useRef, useCallback } from 'react';

interface LogEvent {
  timestamp: number;
  source: 'serial' | 'ws' | 'npc' | 'puzzle' | 'system';
  device?: string;
  type: string;
  payload: unknown;
}

const SOURCE_COLORS: Record<LogEvent['source'], string> = {
  serial: '#60a5fa',
  ws:     '#34d399',
  npc:    '#a78bfa',
  puzzle: '#f59e0b',
  system: '#94a3b8',
};

export function LogRecorder(): React.JSX.Element {
  const [recording, setRecording] = useState(false);
  const [events, setEvents] = useState<LogEvent[]>([]);
  const [filterSource, setFilterSource] = useState<'all' | LogEvent['source']>('all');
  const sessionRef = useRef<LogEvent[]>([]);
  const logEndRef = useRef<HTMLDivElement>(null);

  const record = useCallback((event: LogEvent) => {
    sessionRef.current.push(event);
    setEvents(prev => [...prev, event]);
  }, []);

  useEffect(() => {
    if (!recording) return;

    window.zacus.serial.onData((port, data) => {
      record({
        timestamp: Date.now(),
        source: 'serial',
        device: port,
        type: 'serial_data',
        payload: data,
      });
    });

    window.zacus.wifi.onWsMessage(data => {
      try {
        record({
          timestamp: Date.now(),
          source: 'ws',
          type: 'ws_message',
          payload: JSON.parse(data),
        });
      } catch {
        record({
          timestamp: Date.now(),
          source: 'ws',
          type: 'ws_raw',
          payload: data,
        });
      }
    });
  }, [recording, record]);

  // Auto-scroll to bottom on new events
  useEffect(() => {
    logEndRef.current?.scrollIntoView({ behavior: 'smooth' });
  }, [events]);

  const toggleRecording = useCallback(() => {
    if (!recording) {
      // Starting a new recording — clear previous session
      sessionRef.current = [];
      setEvents([]);
    }
    setRecording(r => !r);
  }, [recording]);

  const exportJSON = useCallback(async () => {
    if (events.length === 0) return;
    const json = JSON.stringify(
      {
        session: {
          startedAt: events[0]?.timestamp,
          endedAt: events[events.length - 1]?.timestamp,
          eventCount: events.length,
          events,
        },
      },
      null,
      2
    );
    await window.zacus.file.save(json, `zacus_session_${Date.now()}.json`);
  }, [events]);

  const exportMarkdown = useCallback(async () => {
    if (events.length === 0) return;
    const lines = [
      '# Zacus Session Log',
      `Generated: ${new Date().toISOString()}`,
      `Events: ${events.length}`,
      '',
      '## Timeline',
      '',
      ...events.map(e => {
        const time = new Date(e.timestamp).toISOString().slice(11, 23);
        const dev  = e.device ? ` (${e.device.split('/').pop()})` : '';
        const pl   = JSON.stringify(e.payload).slice(0, 80);
        return `- **${time}** [${e.source}${dev}] \`${e.type}\`: \`${pl}\``;
      }),
    ];
    await window.zacus.file.save(lines.join('\n'), `zacus_session_${Date.now()}.md`);
  }, [events]);

  const visibleEvents = filterSource === 'all'
    ? events
    : events.filter(e => e.source === filterSource);

  const sources = ['serial', 'ws', 'npc', 'puzzle', 'system'] as const;

  return (
    <div className="log-recorder">
      <div className="panel-header">
        <h2>Log Recorder</h2>
        <div className="toolbar-group">
          <button
            className={`btn ${recording ? 'error' : 'primary'}`}
            onClick={toggleRecording}
          >
            {recording ? '⏹ Stop' : '● Record'}
          </button>
          <button
            className="btn"
            onClick={exportJSON}
            disabled={events.length === 0}
          >
            Export JSON
          </button>
          <button
            className="btn"
            onClick={exportMarkdown}
            disabled={events.length === 0}
          >
            Export Markdown
          </button>
          <button
            className="btn"
            onClick={() => { setEvents([]); sessionRef.current = []; }}
            disabled={events.length === 0}
          >
            Clear
          </button>
        </div>
      </div>

      {recording && (
        <div className="recording-indicator">
          <span className="rec-dot" /> Recording — {events.length} events captured
        </div>
      )}

      <div className="source-filter">
        <button
          className={`btn small ${filterSource === 'all' ? 'active' : ''}`}
          onClick={() => setFilterSource('all')}
        >
          All ({events.length})
        </button>
        {sources.map(src => {
          const count = events.filter(e => e.source === src).length;
          return (
            <button
              key={src}
              className={`btn small ${filterSource === src ? 'active' : ''}`}
              style={{ color: SOURCE_COLORS[src] }}
              onClick={() => setFilterSource(src)}
            >
              {src} ({count})
            </button>
          );
        })}
      </div>

      <div className="event-log">
        {visibleEvents.slice(-200).map((e, i) => (
          <div
            key={i}
            className={`event-row event-${e.source}`}
            style={{ borderLeftColor: SOURCE_COLORS[e.source] }}
          >
            <span className="event-time">
              {new Date(e.timestamp).toISOString().slice(11, 23)}
            </span>
            <span
              className="event-source"
              style={{ color: SOURCE_COLORS[e.source] }}
            >
              [{e.source}]
            </span>
            {e.device && (
              <span className="event-device">
                {e.device.split('/').pop()}
              </span>
            )}
            <span className="event-type">{e.type}</span>
            <span className="event-payload">
              {JSON.stringify(e.payload).slice(0, 120)}
            </span>
          </div>
        ))}
        <div ref={logEndRef} />
      </div>

      {events.length === 0 && !recording && (
        <div className="empty-state">
          Press Record to start capturing serial and WebSocket events.
        </div>
      )}
    </div>
  );
}
