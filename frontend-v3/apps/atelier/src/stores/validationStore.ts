import { create } from 'zustand';

export type ValidationSeverity = 'error' | 'warning' | 'info';

export interface ValidationEntry {
  id: string;
  severity: ValidationSeverity;
  message: string;
  source: 'compile' | 'schema' | 'runtime' | 'test';
}

export interface ValidationState {
  entries: ValidationEntry[];
  push: (entry: ValidationEntry) => void;
  clear: (source?: ValidationEntry['source']) => void;
}

export const useValidationStore = create<ValidationState>((set) => ({
  entries: [],
  push: (entry) => set((s) => ({ entries: [...s.entries, entry] })),
  clear: (source) =>
    set((s) => ({
      entries: source ? s.entries.filter((e) => e.source !== source) : [],
    })),
}));
