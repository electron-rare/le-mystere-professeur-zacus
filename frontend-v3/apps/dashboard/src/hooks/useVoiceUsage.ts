/**
 * useVoiceUsage — REST polling hook against the voice-bridge `/usage/*`
 * cost-audit surface (tools/macstudio/voice-bridge/main.py:976).
 *
 * Polls `GET /usage/stats` every `pollMs` (default 5 s — much slower than
 * `useVoiceBridge`'s 2 s health probe, because the cost-audit dial only
 * needs minute-scale resolution and we don't want to burn extra round-trips
 * on the LAN).
 *
 * Keeps a bounded in-memory ring of the N most recent snapshots so the
 * `VoiceUsagePanel` can compute deltas (tokens/min over the last ~5 min)
 * and render a sparkline without holding any persistent client state.
 *
 * `resetUsage()` POSTs to `/usage/reset`. The voice-bridge protects this
 * endpoint with `X-Admin-Key` whenever `VOICE_BRIDGE_ADMIN_KEY` is set in
 * its environment — pass the matching key via `opts.adminKey` (read from
 * `VITE_VOICE_BRIDGE_ADMIN_KEY` if not provided).
 *
 * Configuration (Vite env, all optional):
 *   VITE_VOICE_BRIDGE_URL            base URL (default 100.116.92.12:8200)
 *   VITE_VOICE_BRIDGE_USAGE_POLL_MS  poll interval (default 5000 ms)
 *   VITE_VOICE_BRIDGE_ADMIN_KEY      admin key for `POST /usage/reset`
 */
import { useCallback, useEffect, useMemo, useRef, useState } from 'react';
import {
  VOICE_BRIDGE_DEFAULT_BASE_URL,
  VOICE_USAGE_DEFAULT_HISTORY,
  VOICE_USAGE_DEFAULT_POLL_MS,
  type VoiceUsageBucket,
  type VoiceUsageStats,
} from '@zacus/shared';

export type VoiceUsageError =
  | { kind: 'network'; message: string }
  | { kind: 'http'; status: number; message: string };

export interface UseVoiceUsageOptions {
  /** Override base URL (otherwise read from VITE_VOICE_BRIDGE_URL). */
  baseUrl?: string;
  /** Override poll interval in ms (otherwise VITE_VOICE_BRIDGE_USAGE_POLL_MS, default 5000). */
  pollMs?: number;
  /** Disable polling (manual mode — call refetch() / resetUsage() yourself). */
  paused?: boolean;
  /** Admin key sent as `X-Admin-Key` to `POST /usage/reset`. */
  adminKey?: string;
  /** Max number of snapshots kept in the in-memory ring (default 60 = 5 min × 1/5 s). */
  historySize?: number;
}

export interface UseVoiceUsageResult {
  /** Latest payload from `GET /usage/stats`, or null until the first successful poll. */
  usage: VoiceUsageStats | null;
  /** Bounded ring of (receivedAtMs, stats) snapshots — newest last. */
  history: VoiceUsageBucket[];
  loading: boolean;
  error: VoiceUsageError | null;
  /** Force a refetch outside the polling cadence. */
  refetch: () => Promise<void>;
  /**
   * POST `/usage/reset`. Resolves on 2xx, throws on network / 4xx / 5xx.
   * Triggers an immediate refetch on success so the UI reflects the new
   * `since` timestamp without waiting for the next poll tick.
   */
  resetUsage: () => Promise<void>;
  baseUrl: string;
  pollMs: number;
}

interface ImportMetaEnvLike {
  VITE_VOICE_BRIDGE_URL?: string;
  VITE_VOICE_BRIDGE_USAGE_POLL_MS?: string;
  VITE_VOICE_BRIDGE_ADMIN_KEY?: string;
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

/**
 * Best-effort coercion of an unknown JSON payload into `VoiceUsageStats`.
 * Defensive against partial / older voice-bridge builds: missing buckets
 * fall back to zeros so the dashboard never crashes on a stale server.
 */
function coerceStats(raw: unknown): VoiceUsageStats {
  const r = (raw ?? {}) as Record<string, unknown>;
  const num = (v: unknown): number => (typeof v === 'number' && Number.isFinite(v) ? v : 0);
  const llm = (v: unknown) => {
    const o = (v ?? {}) as Record<string, unknown>;
    return {
      prompt_tokens: num(o.prompt_tokens),
      completion_tokens: num(o.completion_tokens),
      total_tokens: num(o.total_tokens),
      calls: num(o.calls),
    };
  };
  const tts = (v: unknown) => {
    const o = (v ?? {}) as Record<string, unknown>;
    return {
      f5_calls: num(o.f5_calls),
      f5_seconds: num(o.f5_seconds),
      cache_hits: num(o.cache_hits),
    };
  };
  const stt = (v: unknown) => {
    const o = (v ?? {}) as Record<string, unknown>;
    return {
      calls: num(o.calls),
      audio_seconds: num(o.audio_seconds),
    };
  };
  return {
    since: typeof r.since === 'string' ? r.since : '',
    uptime_s: num(r.uptime_s),
    npc_fast: llm(r.npc_fast),
    hints_deep: llm(r.hints_deep),
    tts: tts(r.tts),
    stt: stt(r.stt),
  };
}

export function useVoiceUsage(opts: UseVoiceUsageOptions = {}): UseVoiceUsageResult {
  const env = useMemo(() => readEnv(), []);
  const baseUrl = (opts.baseUrl ?? env.VITE_VOICE_BRIDGE_URL ?? VOICE_BRIDGE_DEFAULT_BASE_URL).replace(
    /\/+$/,
    '',
  );
  const pollMs =
    opts.pollMs ?? Number(env.VITE_VOICE_BRIDGE_USAGE_POLL_MS ?? VOICE_USAGE_DEFAULT_POLL_MS);
  const adminKey = opts.adminKey ?? env.VITE_VOICE_BRIDGE_ADMIN_KEY ?? '';
  const historySize = opts.historySize ?? VOICE_USAGE_DEFAULT_HISTORY;

  const [usage, setUsage] = useState<VoiceUsageStats | null>(null);
  const [history, setHistory] = useState<VoiceUsageBucket[]>([]);
  const [error, setError] = useState<VoiceUsageError | null>(null);
  const [loading, setLoading] = useState<boolean>(false);

  const abortRef = useRef<AbortController | null>(null);

  const refetch = useCallback(async () => {
    abortRef.current?.abort();
    const ctrl = new AbortController();
    abortRef.current = ctrl;
    setLoading(true);
    try {
      const resp = await fetch(`${baseUrl}/usage/stats`, { signal: ctrl.signal });
      if (!resp.ok) {
        setError({
          kind: 'http',
          status: resp.status,
          message: `GET /usage/stats → ${resp.status}`,
        });
        return;
      }
      const stats = coerceStats(await resp.json());
      setUsage(stats);
      setError(null);
      setHistory((prev) => {
        const next = [...prev, { receivedAtMs: Date.now(), stats }];
        // Trim from the front so the most recent `historySize` entries remain.
        return next.length > historySize ? next.slice(next.length - historySize) : next;
      });
    } catch (err) {
      if (err instanceof DOMException && err.name === 'AbortError') return;
      const message = err instanceof Error ? err.message : String(err);
      setError({ kind: 'network', message });
    } finally {
      setLoading(false);
    }
  }, [baseUrl, historySize]);

  const resetUsage = useCallback(async () => {
    const headers: Record<string, string> = {};
    if (adminKey) headers['X-Admin-Key'] = adminKey;
    const resp = await fetch(`${baseUrl}/usage/reset`, {
      method: 'POST',
      headers,
    });
    if (!resp.ok) {
      const status = resp.status;
      const err: VoiceUsageError = {
        kind: 'http',
        status,
        message: `POST /usage/reset → ${status}`,
      };
      setError(err);
      throw new Error(err.message);
    }
    // On success, drop the rolling history (new accounting window starts now)
    // and immediately re-fetch so the UI reflects the fresh `since` stamp.
    setHistory([]);
    await refetch();
  }, [adminKey, baseUrl, refetch]);

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
    usage,
    history,
    loading,
    error,
    refetch,
    resetUsage,
    baseUrl,
    pollMs,
  };
}
