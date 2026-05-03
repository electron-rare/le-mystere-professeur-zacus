/**
 * useHintsEngine — REST polling hook against the FastAPI hints engine.
 *
 * Polls `GET /hints/sessions` every `VITE_HINTS_POLL_MS` (default 5000) and
 * exposes the current adaptive state per session/puzzle. Designed for the
 * game-master dashboard so hints auto-bumps surface live without WebSocket.
 *
 * Configuration (Vite env, all optional):
 *   VITE_HINTS_BASE_URL    base URL of the hints engine (default localhost:8311)
 *   VITE_HINTS_POLL_MS     poll interval in ms (default 5000)
 *   VITE_HINTS_ADMIN_KEY   X-Admin-Key sent on admin endpoints (default unset)
 */
import { useCallback, useEffect, useMemo, useRef, useState } from 'react';
import {
  HINTS_DEFAULT_BASE_URL,
  HINTS_DEFAULT_POLL_MS,
  type HintsSession,
  type HintsSessionsResponse,
  type HintsTrackerConfig,
} from '@zacus/shared';

export type HintsEngineError =
  | { kind: 'network'; message: string }
  | { kind: 'http'; status: number; message: string };

export interface UseHintsEngineOptions {
  /** Override base URL (otherwise read from VITE_HINTS_BASE_URL). */
  baseUrl?: string;
  /** Override poll interval in ms (otherwise read from VITE_HINTS_POLL_MS). */
  pollMs?: number;
  /** Override admin key (otherwise read from VITE_HINTS_ADMIN_KEY). */
  adminKey?: string | null;
  /** Disable polling (manual mode — use refetch()). */
  paused?: boolean;
}

export interface UseHintsEngineResult {
  sessions: HintsSession[];
  config: HintsTrackerConfig | null;
  nowMs: number;
  loading: boolean;
  error: HintsEngineError | null;
  refetch: () => Promise<void>;
  resetSession: (sessionId: string) => Promise<boolean>;
  baseUrl: string;
  pollMs: number;
}

interface ImportMetaEnvLike {
  VITE_HINTS_BASE_URL?: string;
  VITE_HINTS_POLL_MS?: string;
  VITE_HINTS_ADMIN_KEY?: string;
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

export function useHintsEngine(opts: UseHintsEngineOptions = {}): UseHintsEngineResult {
  const env = useMemo(() => readEnv(), []);
  const baseUrl = (opts.baseUrl ?? env.VITE_HINTS_BASE_URL ?? HINTS_DEFAULT_BASE_URL).replace(
    /\/+$/,
    '',
  );
  const pollMs = opts.pollMs ?? Number(env.VITE_HINTS_POLL_MS ?? HINTS_DEFAULT_POLL_MS);
  const adminKey = opts.adminKey ?? env.VITE_HINTS_ADMIN_KEY ?? null;

  const [sessions, setSessions] = useState<HintsSession[]>([]);
  const [config, setConfig] = useState<HintsTrackerConfig | null>(null);
  const [nowMs, setNowMs] = useState<number>(0);
  const [loading, setLoading] = useState<boolean>(false);
  const [error, setError] = useState<HintsEngineError | null>(null);

  const abortRef = useRef<AbortController | null>(null);

  const buildHeaders = useCallback((): Record<string, string> => {
    const h: Record<string, string> = { Accept: 'application/json' };
    if (adminKey) h['X-Admin-Key'] = adminKey;
    return h;
  }, [adminKey]);

  const refetch = useCallback(async () => {
    abortRef.current?.abort();
    const ctrl = new AbortController();
    abortRef.current = ctrl;
    setLoading(true);
    try {
      const resp = await fetch(`${baseUrl}/hints/sessions`, {
        headers: buildHeaders(),
        signal: ctrl.signal,
      });
      if (!resp.ok) {
        setError({
          kind: 'http',
          status: resp.status,
          message: `GET /hints/sessions → ${resp.status}`,
        });
        return;
      }
      const data = (await resp.json()) as HintsSessionsResponse;
      setSessions(Array.isArray(data.sessions) ? data.sessions : []);
      setConfig(data.config ?? null);
      setNowMs(typeof data.now_ms === 'number' ? data.now_ms : Date.now());
      setError(null);
    } catch (err) {
      // AbortError is expected when the effect tears down — swallow it.
      if (err instanceof DOMException && err.name === 'AbortError') return;
      const message = err instanceof Error ? err.message : String(err);
      setError({ kind: 'network', message });
    } finally {
      setLoading(false);
    }
  }, [baseUrl, buildHeaders]);

  const resetSession = useCallback(
    async (sessionId: string): Promise<boolean> => {
      try {
        const resp = await fetch(
          `${baseUrl}/hints/sessions/${encodeURIComponent(sessionId)}`,
          { method: 'DELETE', headers: buildHeaders() },
        );
        if (!resp.ok) {
          setError({
            kind: 'http',
            status: resp.status,
            message: `DELETE /hints/sessions/${sessionId} → ${resp.status}`,
          });
          return false;
        }
        await refetch();
        return true;
      } catch (err) {
        const message = err instanceof Error ? err.message : String(err);
        setError({ kind: 'network', message });
        return false;
      }
    },
    [baseUrl, buildHeaders, refetch],
  );

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
    sessions,
    config,
    nowMs,
    loading,
    error,
    refetch,
    resetSession,
    baseUrl,
    pollMs,
  };
}
