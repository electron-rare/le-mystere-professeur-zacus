import { useRuntimeStore, type SimulationMode } from '../stores/runtimeStore.js';
import { useValidationStore, type ValidationEntry } from '../stores/validationStore.js';

const MODES: SimulationMode[] = ['sandbox', 'demo', 'test'];

const SEVERITY_COLOR: Record<ValidationEntry['severity'], string> = {
  error: '#fca5a5',
  warning: '#fcd34d',
  info: '#9ca3af',
};

export function ConsolePane() {
  const mode = useRuntimeStore((s) => s.mode);
  const setMode = useRuntimeStore((s) => s.setMode);
  const entries = useValidationStore((s) => s.entries);

  const errorCount = entries.filter((e) => e.severity === 'error').length;
  const warningCount = entries.filter((e) => e.severity === 'warning').length;

  return (
    <div className="atelier-pane atelier-pane--console">
      <div style={{ display: 'flex', alignItems: 'center', width: '100%', gap: 12 }}>
        <div className="atelier-mode-tabs">
          {MODES.map((m) => (
            <button
              key={m}
              type="button"
              className={
                'atelier-mode-tab' + (m === mode ? ' atelier-mode-tab--active' : '')
              }
              onClick={() => setMode(m)}
            >
              {m}
            </button>
          ))}
        </div>
        <span style={{ marginLeft: 'auto', fontSize: 12, color: '#9a9a9d' }}>
          {entries.length === 0
            ? 'No issues'
            : `${errorCount} error${errorCount === 1 ? '' : 's'}, ${warningCount} warning${warningCount === 1 ? '' : 's'}`}
        </span>
      </div>
      <div
        style={{
          width: '100%',
          flex: 1,
          overflowY: 'auto',
          fontSize: 12,
          fontFamily: 'ui-monospace, SFMono-Regular, Menlo, Monaco, monospace',
        }}
      >
        {entries.map((entry) => (
          <div
            key={entry.id}
            style={{
              padding: '2px 0',
              color: SEVERITY_COLOR[entry.severity],
            }}
          >
            [{entry.severity}] {entry.message}
          </div>
        ))}
      </div>
    </div>
  );
}
