import { create } from 'zustand';

export type SimulationMode = 'sandbox' | 'demo' | 'test';

export interface RuntimeState {
  currentIr: unknown | null;
  pendingIr: unknown | null;
  isStale: boolean;
  mode: SimulationMode;
  setPendingIr: (ir: unknown | null) => void;
  commitPendingIr: () => void;
  setMode: (mode: SimulationMode) => void;
}

export const useRuntimeStore = create<RuntimeState>((set, get) => ({
  currentIr: null,
  pendingIr: null,
  isStale: false,
  mode: 'sandbox',
  setPendingIr: (pendingIr) => set({ pendingIr, isStale: pendingIr !== get().currentIr }),
  commitPendingIr: () => set((s) => ({ currentIr: s.pendingIr, isStale: false })),
  setMode: (mode) => set({ mode }),
}));
