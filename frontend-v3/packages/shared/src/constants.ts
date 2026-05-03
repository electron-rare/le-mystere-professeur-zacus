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

// Hints engine REST defaults — overridden via VITE_HINTS_BASE_URL / VITE_HINTS_POLL_MS
export const HINTS_DEFAULT_BASE_URL = 'http://localhost:8311';
export const HINTS_DEFAULT_POLL_MS = 5000;
export const HINTS_GROUP_PROFILES = ['TECH', 'NON_TECH', 'MIXED', 'BOTH'] as const;

// Voice-bridge REST defaults — overridden via VITE_VOICE_BRIDGE_URL / VITE_VOICE_BRIDGE_POLL_MS
// 100.116.92.12:8200 = MacStudio Tailscale IP, where the F5-TTS bridge runs.
export const VOICE_BRIDGE_DEFAULT_BASE_URL = 'http://100.116.92.12:8200';
export const VOICE_BRIDGE_DEFAULT_POLL_MS = 2000;
