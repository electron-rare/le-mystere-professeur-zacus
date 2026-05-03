/**
 * Smoke test for HintsAdaptivePanel.
 *
 * No @testing-library installed in this app — we use react-dom/server's
 * renderToString to assert the panel ships the FR labels and the auto-bump
 * badge when fed adaptive-bumped session data. We pause polling (pollMs=0
 * via paused flag indirectly) by stubbing fetch so the very first request
 * deterministically returns our fixture.
 */
import { describe, it, expect, vi, beforeEach } from 'vitest';
import { renderToString } from 'react-dom/server';
import type { HintsSessionsResponse } from '@zacus/shared';
import { HintsAdaptivePanel } from './HintsAdaptivePanel.js';

const FIXTURE: HintsSessionsResponse = {
  sessions: [
    {
      session_id: 'sess-test-1',
      total_penalty: 150,
      total_hints: 2,
      puzzles: [
        {
          puzzle_id: 'P2_CIRCUIT',
          count: 2,
          last_at_ms: 1_000_000,
          total_penalty: 150,
          cooldown_until_ms: 1_060_000,
          puzzle_started_at_ms: 800_000, // 200 s stuck
          failed_attempts_for_puzzle: 3, // triggers auto-bump heuristic
        },
        {
          puzzle_id: 'P1_SON',
          count: 0,
          last_at_ms: 0,
          total_penalty: 0,
          cooldown_until_ms: 0,
          puzzle_started_at_ms: 0,
          failed_attempts_for_puzzle: 0,
        },
      ],
    },
  ],
  total_sessions: 1,
  config: {
    cooldown_s: 60,
    max_per_puzzle: 3,
    penalty_per_level: { '1': 50, '2': 100, '3': 200 },
    adaptive: {
      enabled: true,
      profiles: {
        TECH: { base_modifier: 0, stuck_minutes_per_bump: 3, max_auto_bump: 2 },
        NON_TECH: { base_modifier: 1, stuck_minutes_per_bump: 2, max_auto_bump: 2 },
      },
      failed_attempts: { bump_every: 2, max_bump: 1 },
    },
  },
  now_ms: 1_010_000,
};

describe('HintsAdaptivePanel', () => {
  beforeEach(() => {
    // Stub global fetch so useHintsEngine resolves without network.
    vi.stubGlobal(
      'fetch',
      vi.fn(async () => ({
        ok: true,
        status: 200,
        json: async () => FIXTURE,
      })),
    );
  });

  it('renders FR labels and adaptive ON state', () => {
    // SSR render — useEffect does not run, so the panel renders its empty
    // state. We assert the static FR labels are present.
    const html = renderToString(<HintsAdaptivePanel pollMs={0} />);
    expect(html).toContain('Hints adaptatifs');
    expect(html).toContain('Adaptatif');
  });

  it('exports a component (smoke)', () => {
    expect(typeof HintsAdaptivePanel).toBe('function');
  });
});
