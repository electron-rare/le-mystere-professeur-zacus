import { describe, it, expect, beforeEach, vi } from 'vitest';

// Mock WebSocket before importing the store
const mockWs = {
  send: vi.fn(),
  close: vi.fn(),
  onopen: null as ((this: WebSocket, ev: Event) => void) | null,
  onmessage: null as ((this: WebSocket, ev: MessageEvent) => void) | null,
  onclose: null as ((this: WebSocket, ev: CloseEvent) => void) | null,
  onerror: null as ((this: WebSocket, ev: Event) => void) | null,
};
vi.stubGlobal('WebSocket', vi.fn(() => mockWs));

const { useGameStore } = await import('./gameStore.js');

describe('gameStore', () => {
  beforeEach(() => {
    useGameStore.setState({
      connected: false,
      ws: null,
      solvedPuzzles: [],
      skippedPuzzles: [],
      hintsGiven: {},
      events: [],
      sessionId: null,
      elapsedMs: 0,
      npcLastPhrase: null,
      groupProfile: null,
    });
    vi.clearAllMocks();
  });

  it('sets connected=true on WebSocket open', () => {
    useGameStore.getState().connect('ws://localhost:81');
    mockWs.onopen?.call(mockWs as unknown as WebSocket, new Event('open'));
    expect(useGameStore.getState().connected).toBe(true);
  });

  it('sends JSON command via WebSocket', () => {
    useGameStore.setState({ connected: true, ws: mockWs as unknown as WebSocket });
    useGameStore.getState().sendCommand({ type: 'force_hint', data: {} });
    expect(mockWs.send).toHaveBeenCalledWith(JSON.stringify({ type: 'force_hint', data: {} }));
  });

  it('updates solvedPuzzles on puzzle_solved event', () => {
    useGameStore.getState().connect('ws://localhost:81');
    const event = { type: 'puzzle_solved' as const, timestamp: Date.now(), data: { puzzle_id: 'P1_SON' } };
    useGameStore.getState().handleEvent(event);
    expect(useGameStore.getState().solvedPuzzles).toContain('P1_SON');
  });

  it('updates elapsedMs on timer_update event', () => {
    useGameStore.getState().handleEvent({ type: 'timer_update', timestamp: Date.now(), data: { elapsed_ms: 5000 } });
    expect(useGameStore.getState().elapsedMs).toBe(5000);
  });

  it('tracks hints per puzzle', () => {
    useGameStore.getState().handleEvent({ type: 'hint_given', timestamp: Date.now(), data: { puzzle_id: 'P2_CIRCUIT' } });
    useGameStore.getState().handleEvent({ type: 'hint_given', timestamp: Date.now(), data: { puzzle_id: 'P2_CIRCUIT' } });
    expect(useGameStore.getState().hintsGiven['P2_CIRCUIT']).toBe(2);
  });
});
