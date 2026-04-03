import { useGameStore } from '../store/gameStore.js';
import type { GameCommandType } from '@zacus/shared';

const CONTROLS: { label: string; cmd: GameCommandType }[] = [
  { label: 'Forcer indice', cmd: 'force_hint' },
  { label: 'Passer puzzle', cmd: 'skip_puzzle' },
  { label: 'Bonus +100', cmd: 'add_bonus' },
  { label: 'Sonner téléphone', cmd: 'ring_phone' },
  { label: 'Pause', cmd: 'pause' },
  { label: 'Reprendre', cmd: 'resume' },
  { label: 'Fin de partie', cmd: 'end_game' },
];

export function ControlPanel() {
  const { connected, sendCommand } = useGameStore();

  return (
    <section className="w-56 p-4 border-l border-white/10 flex flex-col gap-3">
      <h2 className="text-xs font-semibold text-white/60 uppercase tracking-wide">Contrôles</h2>
      {CONTROLS.map(({ label, cmd }) => (
        <button
          key={cmd}
          onClick={() => sendCommand({ type: cmd, data: {} })}
          disabled={!connected}
          className="w-full py-2 px-3 text-xs rounded-xl bg-[#2c2c2e] hover:bg-[#3a3a3c] disabled:opacity-40 text-left"
        >
          {label}
        </button>
      ))}

      <div className="mt-2">
        <label className="text-xs text-white/60">TTS manuel</label>
        <input
          type="text"
          placeholder="Texte à prononcer..."
          className="w-full mt-1 px-2 py-1.5 text-xs rounded-lg bg-[#2c2c2e] border border-white/10 focus:border-[#0071e3] outline-none"
          onKeyDown={(e) => {
            if (e.key === 'Enter') {
              sendCommand({ type: 'manual_tts', data: { text: (e.target as HTMLInputElement).value } });
              (e.target as HTMLInputElement).value = '';
            }
          }}
        />
      </div>
    </section>
  );
}
