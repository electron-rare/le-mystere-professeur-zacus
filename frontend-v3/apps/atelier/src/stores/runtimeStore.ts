import { create } from 'zustand';

export interface RuntimeState {
  /** Last YAML committed via Run — currently running in the simulation. */
  currentIr: string | null;
  /** Latest YAML emitted by the editor (debounced); pending a Run. */
  pendingIr: string | null;
  /** True when pendingIr differs from currentIr. */
  isStale: boolean;
  setPendingIr: (yaml: string | null) => void;
  /** Promote pendingIr -> currentIr. Caller is responsible for kicking the engine. */
  commitPendingIr: () => void;
}

export const useRuntimeStore = create<RuntimeState>((set, get) => ({
  currentIr: null,
  pendingIr: null,
  isStale: false,
  setPendingIr: (pendingIr) =>
    set({ pendingIr, isStale: pendingIr !== null && pendingIr !== get().currentIr }),
  commitPendingIr: () => set((s) => ({ currentIr: s.pendingIr, isStale: false })),
}));
