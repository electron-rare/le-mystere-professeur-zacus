import { useGameStore } from '../store/gameStore.js';

export function Timeline() {
  const events = useGameStore((s) => s.events);

  return (
    <section className="flex-1 p-4 overflow-y-auto">
      <h2 className="text-xs font-semibold text-white/60 mb-3 uppercase tracking-wide">Événements</h2>
      <ul className="flex flex-col gap-1.5">
        {[...events].reverse().map((ev, i) => (
          <li key={i} className="flex items-start gap-2 text-xs">
            <span className="text-white/30 font-mono shrink-0">
              {new Date(ev.timestamp).toLocaleTimeString('fr-FR')}
            </span>
            <span className="text-white/80">{ev.type.replace(/_/g, ' ')}</span>
            {typeof ev.data['puzzle_id'] === 'string' && (
              <span className="text-[#0071e3]">{ev.data['puzzle_id']}</span>
            )}
          </li>
        ))}
      </ul>
    </section>
  );
}
