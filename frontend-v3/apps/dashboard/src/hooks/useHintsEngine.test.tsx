/**
 * @vitest-environment jsdom
 *
 * Tests for useHintsEngine — exercises the polling vs SSE transport
 * selection, the SSE error fallback path, and the heartbeat watchdog.
 *
 * We mount the hook into a tiny React tree via react-dom/client + React 19's
 * `act`. EventSource is replaced by an in-memory FakeEventSource so we can
 * deterministically drive `onopen`/`onmessage`/`onerror` callbacks.
 */
import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';
import { act, useEffect } from 'react';
import { createRoot, type Root } from 'react-dom/client';
import type { HintsSessionsResponse } from '@zacus/shared';
import { useHintsEngine, type UseHintsEngineResult } from './useHintsEngine.js';

// React 19 reads this flag to enable `act` semantics (silences the
// "current testing environment is not configured to support act(...)" warning).
// eslint-disable-next-line @typescript-eslint/no-explicit-any
(globalThis as any).IS_REACT_ACT_ENVIRONMENT = true;

// ---------------------------------------------------------------------------
// Fixtures + fakes
// ---------------------------------------------------------------------------

const FIXTURE: HintsSessionsResponse = {
  sessions: [],
  total_sessions: 0,
  config: {
    cooldown_s: 60,
    max_per_puzzle: 3,
    penalty_per_level: { '1': 50, '2': 100, '3': 200 },
    adaptive: {
      enabled: true,
      profiles: {},
      failed_attempts: { bump_every: 2, max_bump: 1 },
    },
  },
  now_ms: 1_700_000_000_000,
};

interface FakeES {
  url: string;
  readyState: number;
  onopen: ((ev: unknown) => void) | null;
  onmessage: ((ev: MessageEvent) => void) | null;
  onerror: ((ev: unknown) => void) | null;
  listeners: Map<string, Set<(ev: MessageEvent) => void>>;
  close: () => void;
  addEventListener: (type: string, fn: (ev: MessageEvent) => void) => void;
  removeEventListener: (type: string, fn: (ev: MessageEvent) => void) => void;
  // Test-only helpers:
  triggerOpen: () => void;
  triggerError: () => void;
  triggerEvent: (type: string, data?: unknown) => void;
}

const created: FakeES[] = [];

function installFakeEventSource(): void {
  class FakeEventSource implements FakeES {
    url: string;
    readyState = 0;
    onopen: ((ev: unknown) => void) | null = null;
    onmessage: ((ev: MessageEvent) => void) | null = null;
    onerror: ((ev: unknown) => void) | null = null;
    listeners = new Map<string, Set<(ev: MessageEvent) => void>>();

    constructor(url: string) {
      this.url = url;
      created.push(this);
    }

    close(): void {
      this.readyState = 2;
    }

    addEventListener(type: string, fn: (ev: MessageEvent) => void): void {
      let set = this.listeners.get(type);
      if (!set) {
        set = new Set();
        this.listeners.set(type, set);
      }
      set.add(fn);
    }

    removeEventListener(type: string, fn: (ev: MessageEvent) => void): void {
      this.listeners.get(type)?.delete(fn);
    }

    triggerOpen(): void {
      this.readyState = 1;
      this.onopen?.({});
    }

    triggerError(): void {
      this.onerror?.({});
    }

    triggerEvent(type: string, data: unknown = {}): void {
      const ev = { data: JSON.stringify(data) } as MessageEvent;
      if (type === 'message') {
        this.onmessage?.(ev);
      } else {
        const set = this.listeners.get(type);
        if (set) for (const fn of set) fn(ev);
      }
    }
  }
  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  (globalThis as any).EventSource = FakeEventSource;
}

function uninstallFakeEventSource(): void {
  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  delete (globalThis as any).EventSource;
}

function makeFetchMock(): ReturnType<typeof vi.fn> {
  return vi.fn(async () => ({
    ok: true,
    status: 200,
    json: async () => FIXTURE,
  }));
}

// Tiny capture-component: runs the hook and stashes the result on a ref so
// tests can assert on it without dragging in @testing-library.
function HookProbe({
  onResult,
  options,
}: {
  onResult: (r: UseHintsEngineResult) => void;
  options: Parameters<typeof useHintsEngine>[0];
}) {
  const result = useHintsEngine(options);
  useEffect(() => {
    onResult(result);
  });
  return null;
}

let container: HTMLDivElement | null = null;
let root: Root | null = null;

async function renderHook(
  options: Parameters<typeof useHintsEngine>[0],
): Promise<{ getResult: () => UseHintsEngineResult }> {
  container = document.createElement('div');
  document.body.appendChild(container);
  let last: UseHintsEngineResult | null = null;
  root = createRoot(container);
  await act(async () => {
    root!.render(
      <HookProbe options={options} onResult={(r) => (last = r)} />,
    );
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

describe('useHintsEngine — transport selection', () => {
  beforeEach(() => {
    created.length = 0;
    vi.useFakeTimers();
    vi.stubGlobal('fetch', makeFetchMock());
  });

  afterEach(async () => {
    await unmount();
    vi.useRealTimers();
    vi.unstubAllGlobals();
    uninstallFakeEventSource();
  });

  it('uses setInterval when mode="polling" and never opens an EventSource', async () => {
    installFakeEventSource();
    const setIntervalSpy = vi.spyOn(globalThis, 'setInterval');

    const probe = await renderHook({ mode: 'polling', pollMs: 5_000 });

    expect(created.length).toBe(0); // No EventSource constructed
    expect(setIntervalSpy).toHaveBeenCalled();
    expect(probe.getResult().transport).toBe('polling');
  });

  it('opens an EventSource when mode="sse" and reports transport="sse"', async () => {
    installFakeEventSource();

    const probe = await renderHook({ mode: 'sse' });

    expect(created.length).toBe(1);
    expect(created[0]!.url).toContain('/hints/events');
    expect(probe.getResult().transport).toBe('sse');
  });

  it('falls back to polling after 3 SSE errors in mode="auto"', async () => {
    installFakeEventSource();
    const setIntervalSpy = vi.spyOn(globalThis, 'setInterval');

    const probe = await renderHook({ mode: 'auto', pollMs: 5_000 });

    expect(created.length).toBe(1);
    const es = created[0]!;

    // Open succeeds first, then we get 3 transient errors.
    await act(async () => {
      es.triggerOpen();
    });
    expect(probe.getResult().transport).toBe('sse');

    await act(async () => {
      es.triggerError();
      es.triggerError();
      es.triggerError();
    });

    // After the third error, the hook tears down SSE and switches to polling.
    expect(probe.getResult().transport).toBe('polling');
    expect(setIntervalSpy).toHaveBeenCalled();
  });

  it('falls back to polling on connect timeout (mode="auto", no onopen within 2 s)', async () => {
    installFakeEventSource();
    const setIntervalSpy = vi.spyOn(globalThis, 'setInterval');

    const probe = await renderHook({ mode: 'auto', pollMs: 5_000 });

    expect(created.length).toBe(1);
    expect(probe.getResult().transport).toBe('sse');

    // Never trigger onopen — wait past the 2 s connect deadline.
    await act(async () => {
      vi.advanceTimersByTime(2_500);
    });

    expect(probe.getResult().transport).toBe('polling');
    expect(setIntervalSpy).toHaveBeenCalled();
  });

  it('falls back to polling when EventSource is undefined in mode="auto"', async () => {
    // Do NOT install FakeEventSource — global EventSource stays undefined.
    uninstallFakeEventSource();
    const setIntervalSpy = vi.spyOn(globalThis, 'setInterval');

    const probe = await renderHook({ mode: 'auto', pollMs: 5_000 });

    expect(probe.getResult().transport).toBe('polling');
    expect(setIntervalSpy).toHaveBeenCalled();
    expect(created.length).toBe(0);
  });
});
