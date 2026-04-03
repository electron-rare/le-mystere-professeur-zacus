import { useGameStore } from '../store/gameStore.js';
import { PUZZLE_IDS } from '@zacus/shared';

export function SimpleDashboard() {
  const { elapsedMs, targetDuration, solvedPuzzles, connected, sendCommand } = useGameStore();

  const formatTime = (ms: number) => {
    const s = Math.floor(ms / 1000);
    return `${String(Math.floor(s / 60)).padStart(2, '0')}:${String(s % 60).padStart(2, '0')}`;
  };

  const progress = Math.min(1, solvedPuzzles.length / PUZZLE_IDS.length);

  return (
    <div className="flex flex-col items-center justify-center h-screen bg-[#1c1c1e] text-white gap-8">
      {/* Timer */}
      <div className="font-mono text-8xl font-bold tabular-nums">{formatTime(elapsedMs)}</div>

      {/* Progress */}
      <div className="w-96">
        <div className="flex justify-between text-sm text-white/60 mb-2">
          <span>Progression</span>
          <span>{solvedPuzzles.length} / {PUZZLE_IDS.length} puzzles</span>
        </div>
        <div className="h-3 bg-white/10 rounded-full overflow-hidden">
          <div className="h-full bg-[#0071e3] transition-all" style={{ width: `${progress * 100}%` }} />
        </div>
      </div>

      {/* Status */}
      <p className="text-white/60 text-lg">Le Professeur gère tout automatiquement</p>

      {/* Controls */}
      <div className="flex gap-4">
        <button
          onClick={() => sendCommand({ type: 'pause', data: {} })}
          disabled={!connected}
          className="px-8 py-4 rounded-2xl bg-[#2c2c2e] hover:bg-[#3a3a3c] disabled:opacity-40 text-lg font-medium"
        >
          Pause
        </button>
        <button
          onClick={() => sendCommand({ type: 'resume', data: {} })}
          disabled={!connected}
          className="px-8 py-4 rounded-2xl bg-[#0071e3] hover:bg-[#0077ed] disabled:opacity-40 text-lg font-medium"
        >
          Reprendre
        </button>
        <button
          onClick={() => sendCommand({ type: 'end_game', data: {} })}
          disabled={!connected}
          className="px-8 py-4 rounded-2xl bg-[#ff3b30] hover:bg-[#ff453a] disabled:opacity-40 text-lg font-medium"
        >
          Terminer
        </button>
      </div>

      {/* Emergency */}
      <a
        href="tel:+33XXXXXXXXX"
        className="text-sm text-white/40 hover:text-white/80 underline"
      >
        Appeler le support L'Electron Rare
      </a>

      {/* Connection indicator */}
      <div className={`fixed top-4 right-4 text-xs px-2 py-1 rounded-full ${connected ? 'bg-green-500/20 text-green-400' : 'bg-red-500/20 text-red-400'}`}>
        {connected ? '● Connecté à BOX-3' : '● Déconnecté'}
      </div>
    </div>
  );
}
