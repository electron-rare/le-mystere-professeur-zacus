import { create } from 'zustand';

export interface RuntimeState {
  currentIr: unknown | null;
  pendingIr: unknown | null;
  isStale: boolean;
  setPendingIr: (ir: unknown | null) => void;
  commitPendingIr: () => void;
}

export const useRuntimeStore = create<RuntimeState>((set, get) => ({
  currentIr: null,
  pendingIr: null,
  isStale: false,
  setPendingIr: (pendingIr) => set({ pendingIr, isStale: pendingIr !== get().currentIr }),
  commitPendingIr: () => set((s) => ({ currentIr: s.pendingIr, isStale: false })),
}));
