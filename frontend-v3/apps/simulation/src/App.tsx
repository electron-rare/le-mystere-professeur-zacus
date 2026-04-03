import { useState, useEffect } from 'react';
import { RoomScene } from './scene/RoomScene.js';
import { useSimStore } from './store/simStore.js';
import { DemoMode } from './modes/DemoMode.js';
import { SandboxMode } from './modes/SandboxMode.js';
import { TestMode } from './modes/TestMode.js';
import type { SimMode } from './store/simStore.js';

export function App() {
  const { mode, setMode, loadScenario } = useSimStore();
  const [yamlLoaded, setYamlLoaded] = useState(false);

  // Load scenario: check URL param first, then fetch
  useEffect(() => {
    const params = new URLSearchParams(window.location.search);
    const encoded = params.get('yaml');
    if (encoded) {
      try {
        const yaml = decodeURIComponent(atob(encoded));
        loadScenario(yaml);
        setYamlLoaded(true);
        return;
      } catch {
        // fall through to fetch
      }
    }

    fetch('/scenarios/zacus_v3_complete.yaml')
      .then((r) => r.text())
      .then((yaml) => {
        loadScenario(yaml);
        setYamlLoaded(true);
      })
      .catch(() => {
        // In dev: scenario not served, use empty engine
        setYamlLoaded(true);
      });
  }, [loadScenario]);

  return (
    <div className="relative w-full h-screen bg-black">
      {/* Mode selector */}
      <div className="absolute top-4 left-1/2 -translate-x-1/2 z-10 flex gap-1 bg-black/60 rounded-full p-1 backdrop-blur-md">
        {(['demo', 'sandbox', 'test'] as SimMode[]).map((m) => (
          <button
            key={m}
            onClick={() => setMode(m)}
            className={`text-xs px-4 py-1.5 rounded-full capitalize transition-all ${mode === m ? 'bg-[#0071e3] text-white' : 'text-white/60 hover:text-white'}`}
          >
            {m}
          </button>
        ))}
      </div>

      {/* 3D Scene */}
      {yamlLoaded && <RoomScene />}

      {/* Mode-specific overlays */}
      {mode === 'demo' && <DemoMode />}
      {mode === 'sandbox' && <SandboxMode />}
      {mode === 'test' && <TestMode />}
    </div>
  );
}
