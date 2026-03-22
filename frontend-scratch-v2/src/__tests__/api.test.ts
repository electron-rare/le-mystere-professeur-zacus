import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';

// We need to test the internal jsonFetch + the exported helpers.
// Since jsonFetch is not exported, we test it indirectly through storyList / storyStatus
// and also test setApiBase / getApiBase directly.

import {
  setApiBase,
  getApiBase,
  storyList,
  storyStatus,
  ApiError,
} from '../lib/api';

// ─── Mock globalThis.fetch ───

const mockFetch = vi.fn<(...args: unknown[]) => Promise<Response>>();

beforeEach(() => {
  vi.stubGlobal('fetch', mockFetch);
  setApiBase('http://test-esp:8080');
});

afterEach(() => {
  vi.restoreAllMocks();
});

// ─── Helpers ───

function jsonResponse(body: unknown, status = 200): Response {
  return new Response(JSON.stringify(body), {
    status,
    headers: { 'Content-Type': 'application/json' },
  });
}

// ─── Tests ───

describe('setApiBase / getApiBase', () => {
  it('stores and returns the base URL', () => {
    setApiBase('http://192.168.0.42:8080');
    expect(getApiBase()).toBe('http://192.168.0.42:8080');
  });

  it('strips trailing slashes', () => {
    setApiBase('http://host:9000///');
    expect(getApiBase()).toBe('http://host:9000');
  });
});

describe('jsonFetch (via storyList)', () => {
  it('returns parsed JSON on 200', async () => {
    const payload = { scenarios: [{ id: 'demo', version: 1, estimated_duration_s: 300 }] };
    mockFetch.mockResolvedValueOnce(jsonResponse(payload));

    const result = await storyList();
    expect(result).toEqual(payload.scenarios);
    expect(mockFetch).toHaveBeenCalledOnce();

    const [url] = mockFetch.mock.calls[0] as [string];
    expect(url).toBe('http://test-esp:8080/api/story/list');
  });

  it('throws ApiError on HTTP 500', async () => {
    mockFetch.mockResolvedValueOnce(
      jsonResponse({ error: 'internal failure' }, 500),
    );

    try {
      await storyList();
      expect.unreachable('should have thrown');
    } catch (err) {
      expect(err).toBeInstanceOf(ApiError);
      expect((err as ApiError).status).toBe(500);
      expect((err as ApiError).message).toBe('internal failure');
    }
  });

  it('throws on timeout (AbortError path)', async () => {
    // Simulate a request that never resolves — the AbortController will fire
    mockFetch.mockImplementation(
      (...args: unknown[]) =>
        new Promise((_resolve, reject) => {
          const init = args[1] as { signal?: AbortSignal } | undefined;
          if (init?.signal) {
            init.signal.addEventListener('abort', () => {
              const err = new DOMException('The operation was aborted.', 'AbortError');
              reject(err);
            });
          }
        }),
    );

    // storyStatus uses the default 5000ms timeout — too long for tests.
    // Instead we test indirectly: the AbortController fires, the promise rejects.
    // We reduce the wait by calling storyStatus which has default timeout.
    // For speed we rely on the abort signal being set up correctly.
    const promise = storyStatus();
    await expect(promise).rejects.toThrow(/timed out/);
  }, 10_000);
});

describe('storyList', () => {
  it('calls /api/story/list and unwraps scenarios', async () => {
    const scenarios = [
      { id: 'a', version: 2, estimated_duration_s: 600 },
      { id: 'b', version: 1, estimated_duration_s: 120 },
    ];
    mockFetch.mockResolvedValueOnce(jsonResponse({ scenarios }));

    const result = await storyList();
    expect(result).toHaveLength(2);
    expect(result[0].id).toBe('a');
  });
});

describe('storyStatus', () => {
  it('calls /api/story/status and returns status object', async () => {
    const status = {
      status: 'running',
      scenario_id: 'demo',
      current_step: 'STEP_1',
      progress_pct: 42,
      started_at_ms: 1700000000000,
      selected: 'demo',
      queue_depth: 0,
    };
    mockFetch.mockResolvedValueOnce(jsonResponse(status));

    const result = await storyStatus();
    expect(result.status).toBe('running');
    expect(result.current_step).toBe('STEP_1');

    const [url] = mockFetch.mock.calls[0] as [string];
    expect(url).toBe('http://test-esp:8080/api/story/status');
  });
});
