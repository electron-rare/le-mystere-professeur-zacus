import { Component, Suspense, lazy, useState } from 'react';
import type { ErrorInfo, ReactNode } from 'react';
import { RuntimeControls } from './components/RuntimeControls';
import { Dashboard } from './components/Dashboard';
import { useRuntimeStore } from './lib/useRuntimeStore';
import { setApiBase, getApiBase } from './lib/api';
import './App.css';

// ─── Lazy-loaded heavy components ───

const LazyBlocklyDesigner = lazy(() =>
  import('./components/BlocklyDesigner').then((m) => ({ default: m.BlocklyDesigner })),
);
const LazyMonacoEditor = lazy(() => import('@monaco-editor/react'));
const LazyMediaManager = lazy(() =>
  import('./components/MediaManager').then((m) => ({ default: m.MediaManager })),
);
const LazyNetworkPanel = lazy(() =>
  import('./components/NetworkPanel').then((m) => ({ default: m.NetworkPanel })),
);

const LazyFallback = (
  <div style={{ padding: '2rem', textAlign: 'center' }}>Loading editor...</div>
);

// ─── ErrorBoundary ───

interface ErrorBoundaryProps {
  children: ReactNode;
  fallbackLabel?: string;
}

interface ErrorBoundaryState {
  hasError: boolean;
  error: Error | null;
}

class ErrorBoundary extends Component<ErrorBoundaryProps, ErrorBoundaryState> {
  constructor(props: ErrorBoundaryProps) {
    super(props);
    this.state = { hasError: false, error: null };
  }

  static getDerivedStateFromError(error: Error): ErrorBoundaryState {
    return { hasError: true, error };
  }

  componentDidCatch(error: Error, info: ErrorInfo) {
    console.error(`[ErrorBoundary${this.props.fallbackLabel ? ` ${this.props.fallbackLabel}` : ''}]`, error, info.componentStack);
  }

  render() {
    if (this.state.hasError) {
      return (
        <div className="error-boundary-fallback" role="alert">
          <h2>Something went wrong{this.props.fallbackLabel ? ` in ${this.props.fallbackLabel}` : ''}.</h2>
          <pre>{this.state.error?.message}</pre>
          <button type="button" onClick={() => this.setState({ hasError: false, error: null })}>
            Try again
          </button>
        </div>
      );
    }
    return this.props.children;
  }
}

export { ErrorBoundary };

type Tab = 'dashboard' | 'designer' | 'media' | 'network';

const TABS: { id: Tab; label: string }[] = [
  { id: 'dashboard', label: 'Dashboard' },
  { id: 'designer', label: 'Designer' },
  { id: 'media', label: 'Media' },
  { id: 'network', label: 'Network' },
];

function App() {
  const [tab, setTab] = useState<Tab>('dashboard');
  const [yaml, setYaml] = useState('');
  const [runtime3Json, setRuntime3Json] = useState('');
  const [apiUrl, setApiUrl] = useState(getApiBase());
  const runtime = useRuntimeStore();

  const handleApiChange = (url: string) => {
    setApiUrl(url);
    setApiBase(url);
  };

  return (
    <div className="app-shell">
      <header className="app-header">
        <h1>Zacus Studio V2</h1>
        <div className="header-status">
          <span
            className={`dot ${runtime.connected ? 'connected' : 'disconnected'}`}
          />
          <input
            type="text"
            className="api-url-input"
            value={apiUrl}
            onChange={(e) => handleApiChange(e.target.value.trim())}
            aria-label="ESP32 API URL"
            placeholder="http://esp32-ip:8080"
          />
          {runtime.story && (
            <span className="header-step mono">
              {runtime.story.current_step}
            </span>
          )}
        </div>
      </header>

      <nav className="tab-bar">
        {TABS.map((t) => (
          <button
            key={t.id}
            type="button"
            className={`tab ${tab === t.id ? 'active' : ''}`}
            onClick={() => setTab(t.id)}
          >
            {t.label}
          </button>
        ))}
      </nav>

      <main className="main-content">
        {tab === 'dashboard' && (
          <ErrorBoundary fallbackLabel="Dashboard">
            <Dashboard runtime={runtime} refresh={runtime.refresh} />
          </ErrorBoundary>
        )}

        {tab === 'designer' && (
          <ErrorBoundary fallbackLabel="Designer">
            <Suspense fallback={LazyFallback}>
              <div className="app-grid">
                <section className="panel">
                  <h2>Designer blocs</h2>
                  <LazyBlocklyDesigner
                    onDraftChange={(draft) => {
                      setYaml(draft.yaml);
                      setRuntime3Json(draft.runtime3Json);
                    }}
                  />
                </section>
                <section className="panel panel-stack">
                  <div className="editor-group">
                    <h2>YAML canonique</h2>
                    <LazyMonacoEditor
                      height="28vh"
                      defaultLanguage="yaml"
                      value={yaml}
                      options={{
                        minimap: { enabled: false },
                        scrollBeyondLastLine: false,
                        fontSize: 13,
                        readOnly: true,
                      }}
                    />
                  </div>
                  <div className="editor-group">
                    <h2>IR Runtime 3</h2>
                    <LazyMonacoEditor
                      height="28vh"
                      defaultLanguage="json"
                      value={runtime3Json}
                      options={{
                        minimap: { enabled: false },
                        scrollBeyondLastLine: false,
                        fontSize: 13,
                        readOnly: true,
                      }}
                    />
                  </div>
                  <RuntimeControls yaml={yaml} />
                </section>
              </div>
            </Suspense>
          </ErrorBoundary>
        )}

        {tab === 'media' && (
          <ErrorBoundary fallbackLabel="Media">
            <Suspense fallback={LazyFallback}>
              <LazyMediaManager runtime={runtime} />
            </Suspense>
          </ErrorBoundary>
        )}

        {tab === 'network' && (
          <ErrorBoundary fallbackLabel="Network">
            <Suspense fallback={LazyFallback}>
              <LazyNetworkPanel runtime={runtime} />
            </Suspense>
          </ErrorBoundary>
        )}
      </main>
    </div>
  );
}

export default App;
