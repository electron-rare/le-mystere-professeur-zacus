import { useEffect } from 'react';
import { useSimStore } from '../store/simStore.js';

/**
 * TestMode: accelerated engine + diagnostic overlay.
 * Ticks the engine every 100ms (10x speed).
 */
export function TestMode() {
  const { engineState, solvedPuzzles, tickEngine } = useSimStore();

  // Accelerated tick (10x speed)
  useEffect(() => {
    const interval = setInterval(() => {
      tickEngine(Date.now());
    }, 100);
    return () => clearInterval(interval);
  }, [tickEngine]);

  return (
    <div className="absolute bottom-4 left-4 bg-black/80 text-white rounded-2xl p-4 text-xs font-mono backdrop-blur-md">
      <div className="font-bold mb-2 text-[#0071e3]">Test Mode (10x speed)</div>
      <div>Phase: {engineState?.phase ?? '—'}</div>
      <div>Profile: {engineState?.groupProfile ?? 'detecting...'}</div>
      <div>Solved: {solvedPuzzles.join(', ') || 'none'}</div>
      <div>Code: {engineState?.codeAssembled ?? '________'}</div>
      <div>Mood: {engineState?.npcMood ?? '—'}</div>
      <div>Elapsed: {engineState ? Math.round(engineState.elapsedMs / 1000) : 0}s</div>
      <div className="mt-2 text-white/40">Click puzzles to solve them</div>
    </div>
  );
}
