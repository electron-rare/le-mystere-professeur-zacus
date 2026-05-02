import { useEffect } from 'react';
import { useSimStore } from '../stores/simStore.js';

const HUD_STYLE: React.CSSProperties = {
  position: 'absolute',
  bottom: 16,
  left: 16,
  background: 'rgba(0, 0, 0, 0.8)',
  color: '#fff',
  borderRadius: 16,
  padding: 16,
  fontSize: 12,
  fontFamily: 'ui-monospace, SFMono-Regular, Menlo, monospace',
  backdropFilter: 'blur(8px)',
  WebkitBackdropFilter: 'blur(8px)',
};

export function TestMode() {
  const { engineState, solvedPuzzles, tickEngine } = useSimStore();

  useEffect(() => {
    const interval = setInterval(() => {
      tickEngine(Date.now());
    }, 100);
    return () => clearInterval(interval);
  }, [tickEngine]);

  return (
    <div style={HUD_STYLE}>
      <div style={{ fontWeight: 700, marginBottom: 8, color: '#0071e3' }}>
        Test Mode (10x speed)
      </div>
      <div>Phase: {engineState?.phase ?? '—'}</div>
      <div>Profile: {engineState?.groupProfile ?? 'detecting...'}</div>
      <div>Solved: {solvedPuzzles.join(', ') || 'none'}</div>
      <div>Code: {engineState?.codeAssembled ?? '________'}</div>
      <div>Mood: {engineState?.npcMood ?? '—'}</div>
      <div>Elapsed: {engineState ? Math.round(engineState.elapsedMs / 1000) : 0}s</div>
      <div style={{ marginTop: 8, color: 'rgba(255,255,255,0.4)' }}>
        Click puzzles to solve them
      </div>
    </div>
  );
}
