import { create } from 'zustand';
import type { GameEvent, GameCommand, NpcMood, Phase, GroupProfile } from '@zacus/shared';
import { BOX3_MDNS_HOST, BOX3_WS_PORT, BOX3_WS_RECONNECT_MS } from '@zacus/shared';

interface DashboardState {
  // Connection
  wsUrl: string;
  connected: boolean;
  ws: WebSocket | null;

  // Game state (mirrored from BOX-3 events)
  sessionId: string | null;
  elapsedMs: number;
  targetDuration: number;
  phase: Phase;
  groupProfile: GroupProfile | null;
  solvedPuzzles: string[];
  skippedPuzzles: string[];
  hintsGiven: Record<string, number>;
  npcMood: NpcMood;
  npcLastPhrase: string | null;
  codeAssembled: string;
  events: GameEvent[];

  // Actions
  connect: (url?: string) => void;
  disconnect: () => void;
  sendCommand: (cmd: GameCommand) => void;
  setTargetDuration: (minutes: number) => void;
  handleEvent: (event: GameEvent) => void;
}

export const useGameStore = create<DashboardState>((set, get) => ({
  wsUrl: `ws://${BOX3_MDNS_HOST}:${BOX3_WS_PORT}`,
  connected: false,
  ws: null,
  sessionId: null,
  elapsedMs: 0,
  targetDuration: 60,
  phase: 'INTRO',
  groupProfile: null,
  solvedPuzzles: [],
  skippedPuzzles: [],
  hintsGiven: {},
  npcMood: 'neutral',
  npcLastPhrase: null,
  codeAssembled: '',
  events: [],

  connect(url) {
    const wsUrl = url ?? get().wsUrl;
    const ws = new WebSocket(wsUrl);

    ws.onopen = () => set({ connected: true, ws });

    ws.onmessage = (msg) => {
      try {
        const event = JSON.parse(msg.data as string) as GameEvent;
        get().handleEvent(event);
      } catch {
        // Ignore malformed messages
      }
    };

    ws.onclose = () => {
      set({ connected: false, ws: null });
      // Auto-reconnect
      setTimeout(() => get().connect(wsUrl), BOX3_WS_RECONNECT_MS);
    };

    ws.onerror = () => ws.close();
  },

  disconnect() {
    get().ws?.close();
    set({ connected: false, ws: null });
  },

  sendCommand(cmd) {
    const { ws, connected } = get();
    if (!ws || !connected) return;
    ws.send(JSON.stringify(cmd));
  },

  setTargetDuration(minutes) {
    set({ targetDuration: minutes });
    get().sendCommand({ type: 'set_duration', data: { minutes } });
  },

  handleEvent(event: GameEvent) {
    set((state) => {
      const events = [...state.events, event].slice(-200); // Keep last 200

      switch (event.type) {
        case 'timer_update':
          return { events, elapsedMs: event.data['elapsed_ms'] as number };
        case 'puzzle_solved':
          return { events, solvedPuzzles: [...state.solvedPuzzles, event.data['puzzle_id'] as string] };
        case 'puzzle_skipped':
          return { events, skippedPuzzles: [...state.skippedPuzzles, event.data['puzzle_id'] as string] };
        case 'hint_given': {
          const pid = event.data['puzzle_id'] as string;
          return { events, hintsGiven: { ...state.hintsGiven, [pid]: (state.hintsGiven[pid] ?? 0) + 1 } };
        }
        case 'npc_spoke':
          return { events, npcLastPhrase: event.data['phrase'] as string };
        case 'phase_changed':
          return { events, phase: event.data['phase'] as Phase };
        case 'profile_detected':
          return { events, groupProfile: event.data['profile'] as GroupProfile };
        case 'game_start':
          return { events, sessionId: event.data['session_id'] as string };
        default:
          return { events };
      }
    });
  },
}));
