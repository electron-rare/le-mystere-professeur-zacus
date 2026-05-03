import { useGameStore } from '../store/gameStore.js';
import { PUZZLE_IDS } from '@zacus/shared';
import type { PuzzleId } from '@zacus/shared';
import { PuzzleCard } from './PuzzleCard.js';
import { NpcPanel } from './NpcPanel.js';
import { Timeline } from './Timeline.js';
import { ControlPanel } from './ControlPanel.js';
import { HintsAdaptivePanel } from './HintsAdaptivePanel.js';

export function ExpertDashboard() {
  const {
    connected, sessionId, elapsedMs, targetDuration, phase,
    solvedPuzzles, skippedPuzzles, hintsGiven,
  } = useGameStore();

  const formatTime = (ms: number) => {
    const s = Math.floor(ms / 1000);
    return `${String(Math.floor(s / 60)).padStart(2, '0')}:${String(s % 60).padStart(2, '0')}`;
  };

  const progress = Math.min(1, elapsedMs / (targetDuration * 60_000));

  const puzzleStatus = (id: PuzzleId) => {
    if (solvedPuzzles.includes(id)) return 'solved';
    if (skippedPuzzles.includes(id)) return 'skipped';
    return 'pending';
  };

  return (
    <div className="flex flex-col h-screen bg-[#1c1c1e] text-white text-sm">
      {/* Header */}
      <header className="flex items-center gap-6 px-6 h-14 bg-[#2c2c2e] border-b border-white/10 shrink-0">
        <span className="font-mono text-xs text-white/40">SESSION {sessionId ?? '—'}</span>
        <span className="font-mono text-2xl font-bold">{formatTime(elapsedMs)}</span>
        <span className="text-white/40">/ {targetDuration} min</span>
        <div className="flex-1 mx-4 h-2 bg-white/10 rounded-full overflow-hidden">
          <div className="h-full bg-[#0071e3] transition-all" style={{ width: `${progress * 100}%` }} />
        </div>
        <span className={`text-xs px-2 py-0.5 rounded-full ${connected ? 'bg-green-500/20 text-green-400' : 'bg-red-500/20 text-red-400'}`}>
          {connected ? 'Connecté' : 'Déconnecté'}
        </span>
        <span className="text-xs text-white/40">Phase: {phase}</span>
      </header>

      <div className="flex flex-1 min-h-0 gap-0">
        {/* Puzzle Grid */}
        <section className="w-72 p-4 border-r border-white/10 overflow-y-auto">
          <h2 className="text-xs font-semibold text-white/60 mb-3 uppercase tracking-wide">Puzzles</h2>
          <div className="flex flex-col gap-2">
            {PUZZLE_IDS.map((id) => (
              <PuzzleCard
                key={id}
                id={id}
                status={puzzleStatus(id)}
                hints={hintsGiven[id] ?? 0}
              />
            ))}
          </div>
        </section>

        {/* NPC Panel */}
        <NpcPanel />

        {/* Timeline */}
        <Timeline />

        {/* Hints adaptive (P4) */}
        <HintsAdaptivePanel />

        {/* Controls */}
        <ControlPanel />
      </div>
    </div>
  );
}
