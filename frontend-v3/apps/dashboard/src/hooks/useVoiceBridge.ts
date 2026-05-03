/**
 * useVoiceBridge — REST polling hook against the voice-bridge FastAPI server
 * (tools/voice/bridge.py running on the MacStudio).
 *
 * Polls two endpoints in parallel on a fixed interval:
 *   - GET /health/ready       → F5-TTS warmup status (ready / loaded / warmup_ms)
 *   - GET /tts/cache/stats    → cache (count, size_mb, hits, misses, hit_rate)
 *
 * The dashboard cannot subscribe to the WebSocket /voice/ws because that
 * channel is reserved for firmware streaming. Polling cache stats is the
 * cheapest way to surface "voice TTS just synthesized" activity to the GM:
 * any change in (hits + misses) between two polls is treated as new TTS work.
 *
 * Configuration (Vite env, all optional):
 *   VITE_VOICE_BRIDGE_URL      base URL (default 100.116.92.12:8200)
 *   VITE_VOICE_BRIDGE_POLL_MS  poll interval in ms (default 2000)
 */
import { useCallback, useEffect, useMemo, useRef, useState } from 'react';
import {
  VOICE_BRIDGE_DEFAULT_BASE_URL,
  VOICE_BRIDGE_DEFAULT_POLL_MS,
  type VoiceBridgeReady,
  type VoiceBridgeCacheStats,
} from '@zacus/shared';

export type VoiceBridgeError =
  | { kind: 'network'; message: string }
  | { kind: 'http'; status: number; message: string };

export interface UseVoiceBridgeOptions {
  /** Override base URL (otherwise read from VITE_VOICE_BRIDGE_URL). */
  baseUrl?: string;
  /** Override poll interval in ms (otherwise read from VITE_VOICE_BRIDGE_POLL_MS). */
  pollMs?: number;
  /** Disable polling (manual mode — call refetch()). */
  paused?: boolean;
}

export interface UseVoiceBridgeResult {
  ready: boolean;
  f5_loaded: boolean;
  warmup_ms: number;
  cache: VoiceBridgeCacheStats;
  /** Round-trip latency for the last /health/ready call, in ms. */
  latency_ms_health: number;
  error: VoiceBridgeError | null;
  loading: boolean;
  refetch: () => Promise<void>;
  baseUrl: string;
  pollMs: number;
}

interface ImportMetaEnvLike {
  VITE_VOICE_BRIDGE_URL?: string;
  VITE_VOICE_BRIDGE_POLL_MS?: string;
}

function readEnv(): ImportMetaEnvLike {
  // import.meta.env is replaced at build time by Vite. In tests / Node it may
  // not exist — fall back to an empty object.
  try {
    // eslint-disable-next-line @typescript-eslint/no-explicit-any
    const env = (import.meta as any)?.env;
    return (env as ImportMetaEnvLike) ?? {};
  } catch {
    return {};
  }
}

const EMPTY_CACHE: VoiceBridgeCacheStats = {
  count: 0,
  size_mb: 0,
  hits: 0,
  misses: 0,
  hit_rate_since_boot: 0,
};

export function useVoiceBridge(opts: UseVoiceBridgeOptions = {}): UseVoiceBridgeResult {
  const env = useMemo(() => readEnv(), []);
  const baseUrl = (opts.baseUrl ?? env.VITE_VOICE_BRIDGE_URL ?? VOICE_BRIDGE_DEFAULT_BASE_URL).replace(
    /\/+$/,
    '',
  );
  const pollMs =
    opts.pollMs ?? Number(env.VITE_VOICE_BRIDGE_POLL_MS ?? VOICE_BRIDGE_DEFAULT_POLL_MS);

  const [ready, setReady] = useState<boolean>(false);
  const [f5Loaded, setF5Loaded] = useState<boolean>(false);
  const [warmupMs, setWarmupMs] = useState<number>(0);
  const [cache, setCache] = useState<VoiceBridgeCacheStats>(EMPTY_CACHE);
  const [latencyMsHealth, setLatencyMsHealth] = useState<number>(0);
  const [error, setError] = useState<VoiceBridgeError | null>(null);
  const [loading, setLoading] = useState<boolean>(false);

  const abortRef = useRef<AbortController | null>(null);

  const refetch = useCallback(async () => {
    abortRef.current?.abort();
    const ctrl = new AbortController();
    abortRef.current = ctrl;
    setLoading(true);

    const t0 = Date.now();
    try {
      // Fire both probes in parallel — they target the same FastAPI process,
      // so latency is dominated by network RTT. We treat /health/ready as the
      // primary error source (503 is expected during F5 warmup and is NOT an
      // error on its own — we still parse the body).
      const [readyResp, cacheResp] = await Promise.all([
        fetch(`${baseUrl}/health/ready`, { signal: ctrl.signal }),
        fetch(`${baseUrl}/tts/cache/stats`, { signal: ctrl.signal }),
      ]);

      setLatencyMsHealth(Date.now() - t0);

      // /health/ready returns 200 when ready, 503 during warmup. Both carry
      // the same JSON body — we always parse it.
      if (readyResp.status !== 200 && readyResp.status !== 503) {
        setError({
          kind: 'http',
          status: readyResp.status,
          message: `GET /health/ready → ${readyResp.status}`,
        });
        setReady(false);
      } else {
        const data = (await readyResp.json()) as VoiceBridgeReady;
        setReady(Boolean(data.ready));
        setF5Loaded(Boolean(data.f5_loaded));
        setWarmupMs(typeof data.warmup_ms === 'number' ? data.warmup_ms : 0);
        if (readyResp.status === 200 || readyResp.status === 503) setError(null);
      }

      if (!cacheResp.ok) {
        setError({
          kind: 'http',
          status: cacheResp.status,
          message: `GET /tts/cache/stats → ${cacheResp.status}`,
        });
      } else {
        const stats = (await cacheResp.json()) as VoiceBridgeCacheStats;
        setCache({
          count: typeof stats.count === 'number' ? stats.count : 0,
          size_mb: typeof stats.size_mb === 'number' ? stats.size_mb : 0,
          hits: typeof stats.hits === 'number' ? stats.hits : 0,
          misses: typeof stats.misses === 'number' ? stats.misses : 0,
          hit_rate_since_boot:
            typeof stats.hit_rate_since_boot === 'number' ? stats.hit_rate_since_boot : 0,
        });
      }
    } catch (err) {
      // AbortError is expected during teardown — swallow.
      if (err instanceof DOMException && err.name === 'AbortError') return;
      const message = err instanceof Error ? err.message : String(err);
      setError({ kind: 'network', message });
      setReady(false);
    } finally {
      setLoading(false);
    }
  }, [baseUrl]);

  // --------------------------------------------------------------------------
  // Polling effect — fires immediately, then every `pollMs`. Re-arms whenever
  // baseUrl, pollMs, or paused changes.
  // --------------------------------------------------------------------------
  useEffect(() => {
    if (opts.paused) return;
    void refetch();
    if (pollMs <= 0) return;
    const id = setInterval(() => {
      void refetch();
    }, pollMs);
    return () => {
      clearInterval(id);
      abortRef.current?.abort();
    };
  }, [opts.paused, pollMs, refetch]);

  return {
    ready,
    f5_loaded: f5Loaded,
    warmup_ms: warmupMs,
    cache,
    latency_ms_health: latencyMsHealth,
    error,
    loading,
    refetch,
    baseUrl,
    pollMs,
  };
}
