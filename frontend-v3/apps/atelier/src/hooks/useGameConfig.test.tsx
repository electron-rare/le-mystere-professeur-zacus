/**
 * @vitest-environment jsdom
 *
 * Tests for useGameConfig — exercises the parallel ESP32 + hints engine
 * fetch and the setProfile flow (POST then refetch).
 *
 * No @testing-library — same minimal probe pattern used by useHintsEngine.
 */
import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';
import { act, useEffect } from 'react';
import { createRoot, type Root } from 'react-dom/client';
import { useGameConfig, type UseGameConfigResult } from './useGameConfig.js';

// React 19 act flag (silences "not configured to support act" warning).
// eslint-disable-next-line @typescript-eslint/no-explicit-any
(globalThis as any).IS_REACT_ACT_ENVIRONMENT = true;

const ESP32_URL = 'http://zacus-test.local';
const HINTS_URL = 'http://hints-test.local:8311';

interface FetchCall {
  url: string;
  method: string;
  body: unknown;
}

function makeFetchMock(opts: {
  initialProfile?: string;
  postOk?: boolean;
  esp32Fail?: boolean;
}): { fn: ReturnType<typeof vi.fn>; calls: FetchCall[] } {
  const calls: FetchCall[] = [];
  const initialProfile = opts.initialProfile ?? 'MIXED';
  let currentProfile = initialProfile;

  const fn = vi.fn(async (input: RequestInfo | URL, init?: RequestInit) => {
    const url = typeof input === 'string' ? input : input.toString();
    const method = (init?.method ?? 'GET').toUpperCase();
    const body = init?.body ? JSON.parse(init.body as string) : null;
    calls.push({ url, method, body });

    if (url.endsWith('/healthz')) {
      return {
        ok: true,
        status: 200,
        json: async () => ({
          status: 'ok',
          phrases_loaded: 7,
          puzzles_loaded: 7,
          phrases_path: '/phrases.yaml',
          safety_puzzles_loaded: 0,
          safety_path: '/safety.yaml',
          litellm_url: 'http://localhost:4000',
          llm_model: 'gpt-4o-mini',
          adaptive_enabled: true,
          adaptive_path: '/adaptive.yaml',
          adaptive_profiles: ['TECH', 'NON_TECH', 'MIXED', 'BOTH'],
        }),
      };
    }
    if (url.endsWith('/game/group_profile')) {
      if (method === 'GET') {
        if (opts.esp32Fail) {
          return { ok: false, status: 503, json: async () => ({}) };
        }
        return {
          ok: true,
          status: 200,
          json: async () => ({ group_profile: currentProfile }),
        };
      }
      if (method === 'POST') {
        if (opts.postOk === false) {
          return { ok: false, status: 500, json: async () => ({}) };
        }
        currentProfile = (body as { group_profile: string }).group_profile;
        return { ok: true, status: 200, json: async () => ({ group_profile: currentProfile }) };
      }
    }
    return { ok: false, status: 404, json: async () => ({}) };
  });

  return { fn, calls };
}

function HookProbe({
  onResult,
}: {
  onResult: (r: UseGameConfigResult) => void;
}) {
  const result = useGameConfig({ esp32BaseUrl: ESP32_URL, hintsBaseUrl: HINTS_URL });
  useEffect(() => {
    onResult(result);
  });
  return null;
}

let container: HTMLDivElement | null = null;
let root: Root | null = null;

async function renderHook(): Promise<{ getResult: () => UseGameConfigResult }> {
  container = document.createElement('div');
  document.body.appendChild(container);
  let last: UseGameConfigResult | null = null;
  root = createRoot(container);
  await act(async () => {
    root!.render(<HookProbe onResult={(r) => (last = r)} />);
  });
  // Allow the on-mount refetch promise chain to settle.
  await act(async () => {
    await Promise.resolve();
    await Promise.resolve();
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

describe('useGameConfig', () => {
  beforeEach(() => {
    vi.useRealTimers();
  });

  afterEach(async () => {
    await unmount();
    vi.unstubAllGlobals();
  });

  it('fetches both endpoints on mount and surfaces current profile + adaptive state', async () => {
    const { fn } = makeFetchMock({ initialProfile: 'TECH' });
    vi.stubGlobal('fetch', fn);

    const probe = await renderHook();

    const r = probe.getResult();
    expect(r.currentProfile).toBe('TECH');
    expect(r.adaptiveEnabled).toBe(true);
    expect(r.availableProfiles).toEqual(['TECH', 'NON_TECH', 'MIXED', 'BOTH']);
    expect(r.esp32Error).toBeNull();
    expect(r.hintsError).toBeNull();
    // Both endpoints must have been hit at least once.
    const urls = fn.mock.calls.map((c) => String(c[0]));
    expect(urls.some((u) => u.endsWith('/game/group_profile'))).toBe(true);
    expect(urls.some((u) => u.endsWith('/healthz'))).toBe(true);
  });

  it('setProfile POSTs JSON body and refreshes currentProfile', async () => {
    const { fn, calls } = makeFetchMock({ initialProfile: 'MIXED' });
    vi.stubGlobal('fetch', fn);

    const probe = await renderHook();
    expect(probe.getResult().currentProfile).toBe('MIXED');

    let ok = false;
    await act(async () => {
      ok = await probe.getResult().setProfile('NON_TECH');
    });

    expect(ok).toBe(true);
    const post = calls.find((c) => c.method === 'POST');
    expect(post).toBeDefined();
    expect(post!.url).toBe(`${ESP32_URL}/game/group_profile`);
    expect(post!.body).toEqual({ group_profile: 'NON_TECH' });
    expect(probe.getResult().currentProfile).toBe('NON_TECH');
    expect(probe.getResult().esp32Error).toBeNull();
  });

  it('records esp32Error when GET /game/group_profile fails but keeps hints state', async () => {
    const { fn } = makeFetchMock({ esp32Fail: true });
    vi.stubGlobal('fetch', fn);

    const probe = await renderHook();

    const r = probe.getResult();
    expect(r.currentProfile).toBeNull();
    expect(r.esp32Error).not.toBeNull();
    expect(r.esp32Error?.kind).toBe('http');
    // Hints engine still answered — adaptive flag must be live.
    expect(r.adaptiveEnabled).toBe(true);
    expect(r.availableProfiles.length).toBe(4);
  });

  it('returns false from setProfile on POST failure and surfaces the error', async () => {
    const { fn } = makeFetchMock({ initialProfile: 'MIXED', postOk: false });
    vi.stubGlobal('fetch', fn);

    const probe = await renderHook();

    let ok = true;
    await act(async () => {
      ok = await probe.getResult().setProfile('BOTH');
    });

    expect(ok).toBe(false);
    expect(probe.getResult().esp32Error?.kind).toBe('http');
  });
});
