import { create } from 'zustand';

export interface EditorState {
  blocklyJson: string | null;
  setBlocklyJson: (json: string | null) => void;
}

export const useEditorStore = create<EditorState>((set) => ({
  blocklyJson: null,
  setBlocklyJson: (blocklyJson) => set({ blocklyJson }),
}));
