import type { PuzzleId } from '@zacus/shared';

interface Props {
  id: PuzzleId;
  status: 'solved' | 'skipped' | 'pending';
  hints: number;
}

export function PuzzleCard({ id, status, hints }: Props) {
  return (
    <div
      className={`p-3 rounded-xl border ${
        status === 'solved' ? 'border-green-500/30 bg-green-500/10' :
        status === 'skipped' ? 'border-white/10 bg-white/5 opacity-50' :
        'border-white/10 bg-[#2c2c2e]'
      }`}
    >
      <div className="flex items-center justify-between">
        <span className="font-mono text-xs">{id}</span>
        <span>{status === 'solved' ? '✅' : status === 'skipped' ? '⏭' : '⬜'}</span>
      </div>
      {hints > 0 && <div className="text-xs text-orange-400 mt-1">{hints} indice(s)</div>}
    </div>
  );
}
