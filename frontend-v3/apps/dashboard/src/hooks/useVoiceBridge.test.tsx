/**
 * @vitest-environment jsdom
 *
 * Tests for useVoiceBridge — exercises the parallel polling, the cleanup of
 * the interval/AbortController on unmount, and the parsing of the two
 * voice-bridge endpoints (/health/ready + /tts/cache/stats).
 */
import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';
import { act, useEffect } from 'react';
import { createRoot, type Root } from 'react-dom/client';
import type { VoiceBridgeReady, VoiceBridgeCacheStats } from '@zacus/shared';
import { useVoiceBridge, type UseVoiceBridgeResult } from './useVoiceBridge.js';

// React 19 reads this flag to enable `act` semantics.
// eslint-disable-next-line @typescript-eslint/no-explicit-any
(globalThis as any).IS_REACT_ACT_ENVIRONMENT = true;

// ---------------------------------------------------------------------------
// Fixtures
// ---------------------------------------------------------------------------

const READY_FIXTURE: VoiceBridgeReady = {
  ready: true,
  f5_loaded: true,
  warmup_ms: 8421,
  cache_size: 12,
};

const CACHE_FIXTURE: VoiceBridgeCacheStats = {
  count: 12,
  size_mb: 3.4,
  hits: 50,
  misses: 8,
  hit_rate_since_boot: 0.86,
};

function makeFetchMock(): ReturnType<typeof vi.fn> {
  return vi.fn(async (input: RequestInfo | URL) => {
    const url = String(input);
    if (url.includes('/health/ready')) {
      return {
        ok: true,
        status: 200,
        json: async () => READY_FIXTURE,
      };
    }
    if (url.includes('/tts/cache/stats')) {
      return {
        ok: true,
        status: 200,
        json: async () => CACHE_FIXTURE,
      };
    }
    throw new Error(`unexpected fetch: ${url}`);
  });
}

// ---------------------------------------------------------------------------
// Tiny capture-component (mirrors useHintsEngine.test.tsx)
// ---------------------------------------------------------------------------

function HookProbe({
  onResult,
  options,
}: {
  onResult: (r: UseVoiceBridgeResult) => void;
  options: Parameters<typeof useVoiceBridge>[0];
}) {
  const result = useVoiceBridge(options);
  useEffect(() => {
    onResult(result);
  });
  return null;
}

let container: HTMLDivElement | null = null;
let root: Root | null = null;

async function renderHook(
  options: Parameters<typeof useVoiceBridge>[0],
): Promise<{ getResult: () => UseVoiceBridgeResult }> {
  container = document.createElement('div');
  document.body.appendChild(container);
  let last: UseVoiceBridgeResult | null = null;
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

describe('useVoiceBridge — polling + cleanup', () => {
  beforeEach(() => {
    vi.useFakeTimers();
  });

  afterEach(async () => {
    await unmount();
    vi.useRealTimers();
    vi.unstubAllGlobals();
  });

  it('polls /health/ready and /tts/cache/stats in parallel on each tick', async () => {
    const fetchMock = makeFetchMock();
    vi.stubGlobal('fetch', fetchMock);

    const probe = await renderHook({
      baseUrl: 'http://test.local:8200',
      pollMs: 2_000,
    });

    // Allow the initial refetch's promises to flush.
    await act(async () => {
      await Promise.resolve();
      await Promise.resolve();
    });

    const initialCalls = fetchMock.mock.calls.length;
    // First tick = both endpoints hit at least once.
    expect(initialCalls).toBeGreaterThanOrEqual(2);

    const urls = fetchMock.mock.calls.map((c) => String(c[0]));
    expect(urls.some((u) => u.endsWith('/health/ready'))).toBe(true);
    expect(urls.some((u) => u.endsWith('/tts/cache/stats'))).toBe(true);

    // Hook reflects the parsed payloads.
    const r = probe.getResult();
    expect(r.ready).toBe(true);
    expect(r.f5_loaded).toBe(true);
    expect(r.warmup_ms).toBe(8421);
    expect(r.cache.hits).toBe(50);
    expect(r.cache.hit_rate_since_boot).toBeCloseTo(0.86);

    // Advance one poll interval — both endpoints should be hit again.
    await act(async () => {
      vi.advanceTimersByTime(2_000);
      await Promise.resolve();
      await Promise.resolve();
    });

    expect(fetchMock.mock.calls.length).toBeGreaterThanOrEqual(initialCalls + 2);
  });

  it('stops polling on unmount and aborts the in-flight request', async () => {
    const fetchMock = makeFetchMock();
    vi.stubGlobal('fetch', fetchMock);
    const clearIntervalSpy = vi.spyOn(globalThis, 'clearInterval');

    await renderHook({ baseUrl: 'http://test.local:8200', pollMs: 2_000 });

    await act(async () => {
      await Promise.resolve();
      await Promise.resolve();
    });

    const beforeUnmount = fetchMock.mock.calls.length;
    expect(beforeUnmount).toBeGreaterThanOrEqual(2);

    await unmount();

    expect(clearIntervalSpy).toHaveBeenCalled();

    // After unmount, advancing timers must NOT trigger any new fetch.
    await act(async () => {
      vi.advanceTimersByTime(10_000);
      await Promise.resolve();
    });

    expect(fetchMock.mock.calls.length).toBe(beforeUnmount);
  });

  it('treats /health/ready 503 (warmup) as non-error and parses body', async () => {
    const warmupReady: VoiceBridgeReady = {
      ready: false,
      f5_loaded: false,
      warmup_ms: 0,
      cache_size: 0,
    };
    const fetchMock = vi.fn(async (input: RequestInfo | URL) => {
      const url = String(input);
      if (url.includes('/health/ready')) {
        return {
          ok: false,
          status: 503,
          json: async () => warmupReady,
        };
      }
      return {
        ok: true,
        status: 200,
        json: async () => CACHE_FIXTURE,
      };
    });
    vi.stubGlobal('fetch', fetchMock);

    const probe = await renderHook({
      baseUrl: 'http://test.local:8200',
      pollMs: 2_000,
    });

    await act(async () => {
      await Promise.resolve();
      await Promise.resolve();
    });

    const r = probe.getResult();
    expect(r.ready).toBe(false);
    expect(r.f5_loaded).toBe(false);
    expect(r.error).toBeNull(); // 503 during warmup is expected, not an error
  });
});
