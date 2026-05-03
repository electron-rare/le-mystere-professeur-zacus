/**
 * @vitest-environment jsdom
 *
 * Tests for useVoiceUsage — covers polling/cleanup against `/usage/stats`,
 * the reset round-trip (`POST /usage/reset` with `X-Admin-Key`), and the
 * bounded in-memory history ring.
 */
import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';
import { act, useEffect } from 'react';
import { createRoot, type Root } from 'react-dom/client';
import type { VoiceUsageStats } from '@zacus/shared';
import { useVoiceUsage, type UseVoiceUsageResult } from './useVoiceUsage.js';

// React 19 reads this flag to enable `act` semantics.
// eslint-disable-next-line @typescript-eslint/no-explicit-any
(globalThis as any).IS_REACT_ACT_ENVIRONMENT = true;

// ---------------------------------------------------------------------------
// Fixtures
// ---------------------------------------------------------------------------

const USAGE_FIXTURE: VoiceUsageStats = {
  since: '2026-05-04T12:00:00+00:00',
  uptime_s: 1234.5,
  npc_fast: { prompt_tokens: 100, completion_tokens: 50, total_tokens: 150, calls: 3 },
  hints_deep: { prompt_tokens: 200, completion_tokens: 75, total_tokens: 275, calls: 2 },
  tts: { f5_calls: 4, f5_seconds: 12.5, cache_hits: 6 },
  stt: { calls: 1, audio_seconds: 3.2 },
};

interface FetchCall {
  url: string;
  init?: RequestInit;
}

function makeFetchMock(): { mock: ReturnType<typeof vi.fn>; calls: FetchCall[] } {
  const calls: FetchCall[] = [];
  const mock = vi.fn(async (input: RequestInfo | URL, init?: RequestInit) => {
    const url = String(input);
    calls.push({ url, ...(init === undefined ? {} : { init }) });
    if (url.includes('/usage/stats')) {
      return {
        ok: true,
        status: 200,
        json: async () => USAGE_FIXTURE,
      };
    }
    if (url.includes('/usage/reset')) {
      return {
        ok: true,
        status: 200,
        json: async () => ({ reset: true, since: '2026-05-04T13:00:00+00:00' }),
      };
    }
    throw new Error(`unexpected fetch: ${url}`);
  });
  return { mock, calls };
}

// ---------------------------------------------------------------------------
// Tiny capture-component (mirrors useVoiceBridge.test.tsx)
// ---------------------------------------------------------------------------

function HookProbe({
  onResult,
  options,
}: {
  onResult: (r: UseVoiceUsageResult) => void;
  options: Parameters<typeof useVoiceUsage>[0];
}) {
  const result = useVoiceUsage(options);
  useEffect(() => {
    onResult(result);
  });
  return null;
}

let container: HTMLDivElement | null = null;
let root: Root | null = null;

async function renderHook(
  options: Parameters<typeof useVoiceUsage>[0],
): Promise<{ getResult: () => UseVoiceUsageResult }> {
  container = document.createElement('div');
  document.body.appendChild(container);
  let last: UseVoiceUsageResult | null = null;
  root = createRoot(container);
  await act(async () => {
    root!.render(<HookProbe options={options} onResult={(r) => (last = r)} />);
  });
  return {
    getResult: () => {
      if (!last) throw new Error('hook never produced a result');
      return last;
    },
  };
}

async function unmount(): Promise<void> {
  if (root) {
    await act(async () => {
      root!.unmount();
    });
    root = null;
  }
  if (container) {
    container.remove();
    container = null;
  }
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

describe('useVoiceUsage — polling, history ring, reset', () => {
  beforeEach(() => {
    vi.useFakeTimers();
  });

  afterEach(async () => {
    await unmount();
    vi.useRealTimers();
    vi.unstubAllGlobals();
  });

  it('polls /usage/stats on the configured interval and grows a bounded history ring', async () => {
    const { mock: fetchMock, calls } = makeFetchMock();
    vi.stubGlobal('fetch', fetchMock);

    const probe = await renderHook({
      baseUrl: 'http://test.local:8200',
      pollMs: 5_000,
      historySize: 3,
    });

    // Flush the initial refetch promise chain.
    await act(async () => {
      await Promise.resolve();
      await Promise.resolve();
    });

    const initialCalls = calls.length;
    expect(initialCalls).toBeGreaterThanOrEqual(1);
    expect(calls.every((c) => c.url.endsWith('/usage/stats'))).toBe(true);

    // Hook reflects the parsed payload.
    let r = probe.getResult();
    expect(r.usage?.npc_fast.total_tokens).toBe(150);
    expect(r.usage?.tts.f5_calls).toBe(4);
    expect(r.history.length).toBe(1);

    // Advance through three more polls — history should saturate at historySize=3.
    for (let i = 0; i < 4; i++) {
      await act(async () => {
        vi.advanceTimersByTime(5_000);
        await Promise.resolve();
        await Promise.resolve();
      });
    }

    r = probe.getResult();
    expect(calls.length).toBeGreaterThanOrEqual(initialCalls + 4);
    expect(r.history.length).toBe(3); // bounded ring
    // Newest snapshot is last in the ring.
    expect(r.history[r.history.length - 1]?.stats.since).toBe(USAGE_FIXTURE.since);
  });

  it('POST /usage/reset includes X-Admin-Key, clears history, and re-fetches', async () => {
    const { mock: fetchMock, calls } = makeFetchMock();
    vi.stubGlobal('fetch', fetchMock);

    const probe = await renderHook({
      baseUrl: 'http://test.local:8200',
      pollMs: 5_000,
      adminKey: 'super-secret',
    });

    // Flush the initial poll so `history` has 1 entry.
    await act(async () => {
      await Promise.resolve();
      await Promise.resolve();
    });
    expect(probe.getResult().history.length).toBe(1);

    // Trigger reset.
    await act(async () => {
      await probe.getResult().resetUsage();
      await Promise.resolve();
      await Promise.resolve();
    });

    // The reset call must have hit /usage/reset with POST + X-Admin-Key.
    const resetCall = calls.find((c) => c.url.endsWith('/usage/reset'));
    expect(resetCall).toBeDefined();
    expect(resetCall?.init?.method).toBe('POST');
    const headers = (resetCall?.init?.headers ?? {}) as Record<string, string>;
    expect(headers['X-Admin-Key']).toBe('super-secret');

    // After reset, history was cleared then re-populated by the immediate refetch.
    expect(probe.getResult().history.length).toBe(1);
  });

  it('stops polling on unmount and aborts the in-flight request', async () => {
    const { mock: fetchMock, calls } = makeFetchMock();
    vi.stubGlobal('fetch', fetchMock);
    const clearIntervalSpy = vi.spyOn(globalThis, 'clearInterval');

    await renderHook({ baseUrl: 'http://test.local:8200', pollMs: 5_000 });

    await act(async () => {
      await Promise.resolve();
      await Promise.resolve();
    });

    const beforeUnmount = calls.length;
    expect(beforeUnmount).toBeGreaterThanOrEqual(1);

    await unmount();
    expect(clearIntervalSpy).toHaveBeenCalled();

    await act(async () => {
      vi.advanceTimersByTime(20_000);
      await Promise.resolve();
    });

    expect(calls.length).toBe(beforeUnmount);
  });
});
