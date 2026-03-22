import { useEffect, useRef, useState } from 'react';
import {
  type ScenarioListItem,
  type VoiceStatus,
  type GameAnalytics,
  storyList,
  storySelect,
  storyStart,
  storyPause,
  storyResume,
  storySkip,
  askHint,
  voiceStatus,
  gameAnalytics,
} from '../lib/api';
import type { RuntimeState } from '../lib/useRuntimeStore';

// ---------------------------------------------------------------------------
// Types
// ---------------------------------------------------------------------------

interface ChatMessage {
  role: 'user' | 'zacus';
  text: string;
  timestamp: number;
}

type Props = { runtime: RuntimeState; refresh: () => void };

// ---------------------------------------------------------------------------
// Component
// ---------------------------------------------------------------------------

export function Dashboard({ runtime, refresh }: Props) {
  const [scenarios, setScenarios] = useState<ScenarioListItem[]>([]);
  const [busy, setBusy] = useState(false);
  const [feedback, setFeedback] = useState('');
  const [voiceInput, setVoiceInput] = useState('');
  const [voice, setVoice] = useState<VoiceStatus | null>(null);

  // Chat history
  const [chatHistory, setChatHistory] = useState<ChatMessage[]>([]);
  const [chatLoading, setChatLoading] = useState(false);
  const chatEndRef = useRef<HTMLDivElement>(null);

  // Hint level
  const [hintLevel, setHintLevel] = useState<number>(1);

  // Analytics
  const [analytics, setAnalytics] = useState<GameAnalytics | null>(null);
  const [analyticsError, setAnalyticsError] = useState('');

  // ─── Initial load ───
  useEffect(() => {
    storyList()
      .then(setScenarios)
      .catch(() => setScenarios([]));
    voiceStatus()
      .then(setVoice)
      .catch(() => setVoice(null));
  }, [runtime.connected]);

  // ─── Auto-poll voice status every 5s when connected ───
  useEffect(() => {
    if (!runtime.connected) return;
    const id = setInterval(() => {
      voiceStatus().then(setVoice).catch(() => {});
    }, 5000);
    return () => clearInterval(id);
  }, [runtime.connected]);

  // ─── Scroll chat to bottom ───
  useEffect(() => {
    chatEndRef.current?.scrollIntoView({ behavior: 'smooth' });
  }, [chatHistory, chatLoading]);

  // ─── Helpers ───
  const run = async (label: string, fn: () => Promise<unknown>) => {
    setBusy(true);
    setFeedback('');
    try {
      await fn();
      setFeedback(`${label} OK`);
      refresh();
    } catch (err) {
      setFeedback(
        `${label} failed: ${err instanceof Error ? err.message : err}`,
      );
    } finally {
      setBusy(false);
    }
  };

  const handleAskZacus = async () => {
    const text = voiceInput.trim();
    if (!text) return;

    const userMsg: ChatMessage = { role: 'user', text, timestamp: Date.now() };
    setChatHistory((prev) => [...prev, userMsg]);
    setVoiceInput('');
    setChatLoading(true);

    try {
      const puzzleId = runtime.story?.current_step || 'general';
      const res = await askHint(puzzleId, text, hintLevel);
      const zacusMsg: ChatMessage = {
        role: 'zacus',
        text: res.hint,
        timestamp: Date.now(),
      };
      setChatHistory((prev) => [...prev, zacusMsg]);
    } catch (err) {
      const errorMsg: ChatMessage = {
        role: 'zacus',
        text: `[Error] ${err instanceof Error ? err.message : String(err)}`,
        timestamp: Date.now(),
      };
      setChatHistory((prev) => [...prev, errorMsg]);
    } finally {
      setChatLoading(false);
    }
  };

  const refreshAnalytics = () => {
    setAnalyticsError('');
    gameAnalytics()
      .then(setAnalytics)
      .catch((err) =>
        setAnalyticsError(err instanceof Error ? err.message : String(err)),
      );
  };

  const formatDuration = (ms: number) => {
    const s = Math.floor(ms / 1000);
    const m = Math.floor(s / 60);
    const sec = s % 60;
    return `${m}m ${sec}s`;
  };

  const story = runtime.story;
  const legacy = runtime.legacy;
  const net = legacy?.network;
  const runtime3 = legacy?.runtime3;

  return (
    <div className="dashboard">
      <h2>Dashboard</h2>

      {/* Connection */}
      <section className="card">
        <h3>Connection</h3>
        <div className="kv">
          <span>Status</span>
          <span className={runtime.connected ? 'badge ok' : 'badge err'}>
            {runtime.connected ? 'Connected' : 'Disconnected'}
          </span>
        </div>
        {net && (
          <>
            <div className="kv">
              <span>Network</span>
              <span>{net.state ?? '?'}</span>
            </div>
            <div className="kv">
              <span>IP</span>
              <span>{net.ip ?? '?'}</span>
            </div>
          </>
        )}
      </section>

      {/* Story status */}
      <section className="card">
        <h3>Story runtime</h3>
        {story ? (
          <>
            <div className="kv">
              <span>Status</span>
              <span className="badge">{story.status}</span>
            </div>
            <div className="kv">
              <span>Scenario</span>
              <span>{story.scenario_id || '\u2014'}</span>
            </div>
            <div className="kv">
              <span>Step</span>
              <span className="mono">{story.current_step || '\u2014'}</span>
            </div>
            <div className="kv">
              <span>Progress</span>
              <div className="progress-bar">
                <div
                  className="progress-fill"
                  style={{ width: `${story.progress_pct}%` }}
                />
                <span>{story.progress_pct}%</span>
              </div>
            </div>
            <div className="kv">
              <span>Queue</span>
              <span>{story.queue_depth}</span>
            </div>
          </>
        ) : (
          <p className="muted">No Story V2 status available.</p>
        )}
      </section>

      <section className="card">
        <h3>Runtime 3 adapter</h3>
        {runtime3 ? (
          <div className="runtime3-grid">
            <div className="kv">
              <span>Contract</span>
              <span className="mono">
                {legacy?.story.runtime_contract ?? 'story_v2'}
              </span>
            </div>
            <div className="kv">
              <span>Status</span>
              <span
                className={`badge ${
                  runtime3.loaded ? 'ok' : runtime3.discovered ? 'active' : 'err'
                }`}
              >
                {runtime3.loaded
                  ? 'Loaded'
                  : runtime3.discovered
                    ? 'Discovered'
                    : 'Missing'}
              </span>
            </div>
            <div className="kv">
              <span>Scenario</span>
              <span className="mono">{runtime3.scenario_id || '\u2014'}</span>
            </div>
            <div className="kv">
              <span>Version</span>
              <span>{runtime3.scenario_version || 0}</span>
            </div>
            <div className="kv">
              <span>Entry step</span>
              <span className="mono">{runtime3.entry_step_id || '\u2014'}</span>
            </div>
            <div className="kv">
              <span>Migration</span>
              <span className="mono">{runtime3.migration_mode || '\u2014'}</span>
            </div>
            <div className="kv">
              <span>Steps</span>
              <span>{runtime3.step_count}</span>
            </div>
            <div className="kv">
              <span>Transitions</span>
              <span>{runtime3.transition_count}</span>
            </div>
            <div className="kv">
              <span>Source</span>
              <span className="mono">{runtime3.source_kind || '\u2014'}</span>
            </div>
            <div className="kv">
              <span>Schema</span>
              <span className="mono">{runtime3.schema_version || '\u2014'}</span>
            </div>
            <div className="kv runtime3-span">
              <span>Bundle</span>
              <span className="mono runtime3-path">{runtime3.path || '\u2014'}</span>
            </div>
            {runtime3.error && (
              <div className="notice warn runtime3-span">
                Runtime 3 error: {runtime3.error}
              </div>
            )}
          </div>
        ) : (
          <p className="muted">No Runtime 3 adapter metadata available.</p>
        )}
      </section>

      {/* Scenarios */}
      <section className="card">
        <h3>Scenarios</h3>
        {scenarios.length === 0 ? (
          <p className="muted">No scenarios found.</p>
        ) : (
          <ul className="scenario-list">
            {scenarios.map((s) => (
              <li key={s.id}>
                <span className="mono">{s.id}</span>
                <span className="muted">v{s.version}</span>
                <button
                  type="button"
                  disabled={busy || story?.selected === s.id}
                  onClick={() => run('Select', () => storySelect(s.id))}
                >
                  {story?.selected === s.id ? 'Selected' : 'Select'}
                </button>
              </li>
            ))}
          </ul>
        )}
      </section>

      {/* Controls */}
      <section className="card">
        <h3>Controls</h3>
        <div className="actions">
          <button
            type="button"
            disabled={busy || story?.status === 'running'}
            onClick={() => run('Start', storyStart)}
          >
            Start
          </button>
          <button
            type="button"
            disabled={busy || story?.status !== 'running'}
            onClick={() => run('Pause', storyPause)}
          >
            Pause
          </button>
          <button
            type="button"
            disabled={busy || story?.status !== 'paused'}
            onClick={() => run('Resume', storyResume)}
          >
            Resume
          </button>
          <button
            type="button"
            disabled={busy || story?.status !== 'running'}
            onClick={() => run('Skip', storySkip)}
          >
            Skip
          </button>
        </div>
        {feedback && <p className="feedback">{feedback}</p>}
      </section>

      {/* Voice — Ask Professor Zacus (enhanced chat) */}
      <section className="card">
        <h3>Ask Professor Zacus</h3>
        <div className="kv">
          <span>Voice Bridge</span>
          <span className={voice?.connected ? 'badge ok' : 'badge err'}>
            {voice?.connected ? 'Connected' : 'Disconnected'}
          </span>
        </div>

        {/* Hint level selector */}
        <div style={{ display: 'flex', gap: '1rem', alignItems: 'center', margin: '0.5rem 0' }}>
          <span style={{ fontSize: '0.85em', fontWeight: 500 }}>Hint level:</span>
          {[1, 2, 3].map((lvl) => (
            <label key={lvl} style={{ display: 'flex', alignItems: 'center', gap: '0.25rem', fontSize: '0.85em', cursor: 'pointer' }}>
              <input
                type="radio"
                name="hintLevel"
                value={lvl}
                checked={hintLevel === lvl}
                onChange={() => setHintLevel(lvl)}
              />
              {lvl === 1 ? 'Gentle' : lvl === 2 ? 'Medium' : 'Direct'}
            </label>
          ))}
        </div>

        {/* Chat history */}
        <div
          style={{
            maxHeight: '240px',
            overflowY: 'auto',
            border: '1px solid var(--border, #ddd)',
            borderRadius: '6px',
            padding: '0.5rem',
            marginBottom: '0.5rem',
            background: 'var(--bg-muted, #fafafa)',
          }}
        >
          {chatHistory.length === 0 && !chatLoading && (
            <p className="muted" style={{ textAlign: 'center', margin: '1rem 0', fontSize: '0.85em' }}>
              Ask Professor Zacus a question about the puzzle...
            </p>
          )}
          {chatHistory.map((msg, i) => (
            <div
              key={i}
              style={{
                display: 'flex',
                flexDirection: 'column',
                alignItems: msg.role === 'user' ? 'flex-end' : 'flex-start',
                marginBottom: '0.4rem',
              }}
            >
              <div
                style={{
                  maxWidth: '80%',
                  padding: '0.4rem 0.6rem',
                  borderRadius: '8px',
                  background: msg.role === 'user'
                    ? 'var(--accent, #6c5ce7)'
                    : 'var(--bg-card, #fff)',
                  color: msg.role === 'user' ? '#fff' : 'inherit',
                  border: msg.role === 'zacus' ? '1px solid var(--border, #ddd)' : 'none',
                  fontSize: '0.9em',
                  whiteSpace: 'pre-wrap',
                }}
              >
                {msg.role === 'zacus' && (
                  <strong style={{ fontSize: '0.8em', display: 'block', marginBottom: '0.15rem' }}>
                    Prof. Zacus
                  </strong>
                )}
                {msg.text}
              </div>
              <span style={{ fontSize: '0.7em', color: '#999', marginTop: '0.1rem' }}>
                {new Date(msg.timestamp).toLocaleTimeString()}
              </span>
            </div>
          ))}
          {chatLoading && (
            <div style={{ display: 'flex', alignItems: 'center', gap: '0.4rem', padding: '0.4rem' }}>
              <span
                className="spinner"
                style={{
                  display: 'inline-block',
                  width: '14px',
                  height: '14px',
                  border: '2px solid var(--border, #ddd)',
                  borderTopColor: 'var(--accent, #6c5ce7)',
                  borderRadius: '50%',
                  animation: 'spin 0.8s linear infinite',
                }}
              />
              <span style={{ fontSize: '0.85em', color: '#999' }}>Professor Zacus is thinking...</span>
            </div>
          )}
          <div ref={chatEndRef} />
        </div>

        {/* Hint count */}
        {analytics && (
          <div style={{ fontSize: '0.8em', color: '#888', marginBottom: '0.3rem' }}>
            Hints used this session: {analytics.total_hints}
          </div>
        )}

        {/* Input */}
        <div style={{ display: 'flex', gap: '0.5rem' }}>
          <input
            type="text"
            placeholder="Ask a question..."
            aria-label="Voice query"
            value={voiceInput}
            onChange={(e) => setVoiceInput(e.target.value)}
            onKeyDown={(e) => {
              if (e.key === 'Enter' && voiceInput.trim() && !chatLoading) {
                handleAskZacus();
              }
            }}
            disabled={chatLoading}
            style={{ flex: 1 }}
          />
          <button
            type="button"
            disabled={chatLoading || !voiceInput.trim()}
            onClick={handleAskZacus}
          >
            Ask
          </button>
        </div>
      </section>

      {/* Analytics */}
      <section className="card">
        <h3>
          Analytics{' '}
          <button
            type="button"
            onClick={refreshAnalytics}
            style={{ fontSize: '0.75em', marginLeft: '0.5rem', padding: '0.15rem 0.5rem' }}
          >
            Refresh
          </button>
        </h3>
        {analyticsError && <p className="error-banner">{analyticsError}</p>}
        {analytics ? (
          <div className="runtime3-grid">
            <div className="kv">
              <span>Session</span>
              <span className="mono">{analytics.session_id}</span>
            </div>
            <div className="kv">
              <span>Duration</span>
              <span>{formatDuration(analytics.duration_ms)}</span>
            </div>
            <div className="kv">
              <span>Puzzles solved</span>
              <span>
                {analytics.puzzles_solved} / {analytics.puzzles.length}
              </span>
            </div>
            <div className="kv">
              <span>Hints used</span>
              <span>{analytics.total_hints}</span>
            </div>
            <div className="kv">
              <span>Total attempts</span>
              <span>{analytics.total_attempts}</span>
            </div>
          </div>
        ) : (
          <p className="muted">No analytics loaded. Press Refresh to fetch.</p>
        )}
      </section>

      {/* Last WS event */}
      {runtime.lastEvent && (
        <section className="card">
          <h3>Last event</h3>
          <pre className="event-json">
            {JSON.stringify(runtime.lastEvent, null, 2)}
          </pre>
        </section>
      )}

      {runtime.lastError && (
        <p className="error-banner">{runtime.lastError}</p>
      )}

      {/* Spinner keyframe (injected once) */}
      <style>{`@keyframes spin { to { transform: rotate(360deg); } }`}</style>
    </div>
  );
}
