import { useRuntimeStore, type SimulationMode } from '../stores/runtimeStore.js';
import { useValidationStore } from '../stores/validationStore.js';

const MODES: SimulationMode[] = ['sandbox', 'demo', 'test'];

export function ConsolePane() {
  const mode = useRuntimeStore((s) => s.mode);
  const setMode = useRuntimeStore((s) => s.setMode);
  const entries = useValidationStore((s) => s.entries);

  return (
    <div className="atelier-pane atelier-pane--console">
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
      <div style={{ fontSize: 12, color: '#6b7280' }}>
        {entries.length === 0
          ? 'Validation console — no issues'
          : `${entries.length} entr${entries.length === 1 ? 'y' : 'ies'}`}
      </div>
    </div>
  );
}
