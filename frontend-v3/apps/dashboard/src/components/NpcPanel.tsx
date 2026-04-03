import { useGameStore } from '../store/gameStore.js';
import { NPC_MOOD_COLORS } from '@zacus/shared';

const moodEmoji: Record<string, string> = {
  neutral: '😐', impressed: '😄', worried: '😟', amused: '😂',
};

export function NpcPanel() {
  const { npcMood, groupProfile, npcLastPhrase } = useGameStore();

  return (
    <section className="w-56 p-4 border-r border-white/10 overflow-y-auto">
      <h2 className="text-xs font-semibold text-white/60 mb-3 uppercase tracking-wide">Professeur Zacus</h2>
      <div
        className="w-16 h-16 rounded-full mx-auto mb-3 flex items-center justify-center text-3xl"
        style={{
          backgroundColor: (NPC_MOOD_COLORS[npcMood] ?? '#0071e3') + '33',
          boxShadow: `0 0 20px ${(NPC_MOOD_COLORS[npcMood] ?? '#0071e3')}66`,
        }}
      >
        {moodEmoji[npcMood] ?? '😐'}
      </div>
      <div className="text-center text-xs text-white/60 mb-2">Humeur: {npcMood}</div>
      {groupProfile !== null && (
        <div className="text-center text-xs text-[#0071e3] mb-3">Profil détecté: {groupProfile}</div>
      )}
      {npcLastPhrase !== null && (
        <div className="bg-[#2c2c2e] rounded-xl p-3 text-xs italic text-white/80">
          "{npcLastPhrase}"
        </div>
      )}
    </section>
  );
}
