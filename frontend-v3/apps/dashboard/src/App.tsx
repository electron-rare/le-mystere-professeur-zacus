import { useState } from 'react';
import { useGameStore } from './store/gameStore.js';
import { ExpertDashboard } from './components/ExpertDashboard.js';
import { SimpleDashboard } from './components/SimpleDashboard.js';
import { ConnectionSetup } from './components/ConnectionSetup.js';

type ViewMode = 'expert' | 'simple';

export function App() {
  const { connect } = useGameStore();
  const [mode, setMode] = useState<ViewMode>(() => {
    const params = new URLSearchParams(window.location.search);
    return (params.get('mode') as ViewMode) ?? 'expert';
  });
  const [setupDone, setSetupDone] = useState(false);

  const handleConnect = (url: string) => {
    connect(url);
    setSetupDone(true);
  };

  if (!setupDone) {
    return <ConnectionSetup onConnect={handleConnect} />;
  }

  return (
    <div>
      {/* Mode toggle (top-center) */}
      <div className="fixed top-2 left-1/2 -translate-x-1/2 z-50 flex gap-1 bg-[#2c2c2e] rounded-full p-1">
        <button
          onClick={() => setMode('expert')}
          className={`text-xs px-4 py-1.5 rounded-full transition-all ${mode === 'expert' ? 'bg-[#0071e3] text-white' : 'text-white/40 hover:text-white'}`}
        >
          Expert
        </button>
        <button
          onClick={() => setMode('simple')}
          className={`text-xs px-4 py-1.5 rounded-full transition-all ${mode === 'simple' ? 'bg-[#0071e3] text-white' : 'text-white/40 hover:text-white'}`}
        >
          Simple
        </button>
      </div>
      {mode === 'expert' ? <ExpertDashboard /> : <SimpleDashboard />}
    </div>
  );
}
