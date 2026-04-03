import { create } from 'zustand';
import type { NpcMood, EngineState } from '@zacus/shared';
import { ZacusScenarioEngine } from '@zacus/scenario-engine';

export type SimMode = 'demo' | 'sandbox' | 'test';

interface SimState {
  mode: SimMode;
  npcMood: NpcMood;
  npcLastPhrase: string | null;
  solvedPuzzles: string[];
  engineState: EngineState | null;
  engine: ZacusScenarioEngine | null;

  setMode: (mode: SimMode) => void;
  solvePuzzle: (id: string) => void;
  loadScenario: (yaml: string) => void;
  tickEngine: (nowMs: number) => void;
  speakNpc: (phrase: string) => void;
}

export const useSimStore = create<SimState>((set, get) => ({
  mode: 'sandbox',
  npcMood: 'neutral',
  npcLastPhrase: null,
  solvedPuzzles: [],
  engineState: null,
  engine: null,

  setMode(mode) { set({ mode }); },

  loadScenario(yaml) {
    const engine = new ZacusScenarioEngine();
    engine.load(yaml);
    engine.start({ targetDuration: 60, mode: '60' });
    set({ engine, engineState: engine.getState() });
  },

  tickEngine(nowMs) {
    const { engine } = get();
    if (!engine) return;
    const state = engine.tick(nowMs);
    set({ engineState: state, npcMood: state.npcMood });
  },

  solvePuzzle(id) {
    const { engine } = get();
    set((s) => ({ solvedPuzzles: [...s.solvedPuzzles, id] }));
    if (engine) {
      const decisions = engine.onEvent({
        type: 'puzzle_solved',
        timestamp: Date.now(),
        data: { puzzle_id: id },
      });
      for (const d of decisions) {
        if (d.action === 'speak') {
          const phrase = `Excellent ! Puzzle ${id} résolu !`;
          get().speakNpc(phrase);
        }
      }
    }
  },

  speakNpc(phrase) {
    set({ npcLastPhrase: phrase });
    // TTS via Web Speech API
    if ('speechSynthesis' in window) {
      const utt = new SpeechSynthesisUtterance(phrase);
      utt.lang = 'fr-FR';
      window.speechSynthesis.speak(utt);
    }
    // Clear bubble after 5s
    setTimeout(() => set({ npcLastPhrase: null }), 5000);
  },
}));
