// Puzzle IDs matching zacus_v3_complete.yaml hardware.puzzles[*].id
export const PUZZLE_IDS = ['P1_SON', 'P2_CIRCUIT', 'P3_QR', 'P4_RADIO', 'P5_MORSE', 'P6_SYMBOLES', 'P7_COFFRE'] as const;
export type PuzzleId = typeof PUZZLE_IDS[number];

export const NPC_MOODS = ['neutral', 'impressed', 'worried', 'amused'] as const;
export const PHASE_TYPES = ['INTRO', 'PROFILING', 'ADAPTIVE', 'CLIMAX', 'OUTRO'] as const;
export const DURATION_MODES = ['30', '45', '60', '90'] as const;

// NPC mood -> glow color (used in dashboard + simulation)
export const NPC_MOOD_COLORS: Record<string, string> = {
  neutral: '#0071e3',
  impressed: '#34c759',
  worried: '#ff9500',
  amused: '#af52de',
};

// Theme FER accent colors
export const FER_ACCENT = '#0071e3';
export const FER_SURFACE = '#1c1c1e';
export const FER_CARD = '#2c2c2e';

// BOX-3 mDNS default hostname
export const BOX3_MDNS_HOST = 'zacus-box3.local';
export const BOX3_WS_PORT = 81;
export const BOX3_WS_RECONNECT_MS = 3000;
