/**
 * useHintsEngine — REST + SSE-aware hook against the FastAPI hints engine.
 *
 * Two transports:
 *   - polling : `setInterval` over `GET /hints/sessions` (legacy, default 5 s)
 *   - sse     : `EventSource` on `GET /hints/events` — every event triggers
 *               an authoritative refetch of `/hints/sessions`. The SSE stream
 *               is only used as a *signal* ("something happened, refresh
 *               now"), not as the source of truth, because the envelope
 *               payload is intentionally minimal on the server side.
 *
 * The default mode is `auto`: try SSE first, fall back to polling if the
 * connection cannot be established within 2 s, or after 3 consecutive
 * `onerror` callbacks during the session, or if the browser has no
 * `EventSource` global at all (jsdom / Node test envs).
 *
 * A 30 s heartbeat watchdog forces an EventSource reconnect if no event
 * (including the server's 15 s `ping`) was received in that window — covers
 * dead connections that browsers would otherwise hold open silently.
 *
 * Configuration (Vite env, all optional):
 *   VITE_HINTS_BASE_URL    base URL of the hints engine (default localhost:8311)
 *   VITE_HINTS_POLL_MS     poll interval in ms (default 5000)
 *   VITE_HINTS_ADMIN_KEY   X-Admin-Key sent on admin endpoints (default unset)
 *   VITE_HINTS_MODE        "polling" | "sse" | "auto" (default "auto")
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

export type HintsTransportMode = 'polling' | 'sse' | 'auto';
export type HintsActiveTransport = 'polling' | 'sse';

export interface UseHintsEngineOptions {
  /** Override base URL (otherwise read from VITE_HINTS_BASE_URL). */
  baseUrl?: string;
  /** Override poll interval in ms (otherwise read from VITE_HINTS_POLL_MS). */
  pollMs?: number;
  /** Override admin key (otherwise read from VITE_HINTS_ADMIN_KEY). */
  adminKey?: string | null;
  /** Disable polling (manual mode — use refetch()). */
  paused?: boolean;
  /** Transport mode (default: env VITE_HINTS_MODE then "auto"). */
  mode?: HintsTransportMode;
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
  /** Resolved transport mode actually in use right now. */
  transport: HintsActiveTransport;
}

interface ImportMetaEnvLike {
  VITE_HINTS_BASE_URL?: string;
  VITE_HINTS_POLL_MS?: string;
  VITE_HINTS_ADMIN_KEY?: string;
  VITE_HINTS_MODE?: string;
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

function parseMode(raw: string | undefined): HintsTransportMode {
  if (raw === 'polling' || raw === 'sse' || raw === 'auto') return raw;
  return 'auto';
}

// SSE tuning — fixed constants on purpose, no need to expose to callers yet.
const SSE_CONNECT_TIMEOUT_MS = 2_000;
const SSE_MAX_ERRORS = 3;
const SSE_HEARTBEAT_GAP_MS = 30_000;

export function useHintsEngine(opts: UseHintsEngineOptions = {}): UseHintsEngineResult {
  const env = useMemo(() => readEnv(), []);
  const baseUrl = (opts.baseUrl ?? env.VITE_HINTS_BASE_URL ?? HINTS_DEFAULT_BASE_URL).replace(
    /\/+$/,
    '',
  );
  const pollMs = opts.pollMs ?? Number(env.VITE_HINTS_POLL_MS ?? HINTS_DEFAULT_POLL_MS);
  const adminKey = opts.adminKey ?? env.VITE_HINTS_ADMIN_KEY ?? null;
  const requestedMode = opts.mode ?? parseMode(env.VITE_HINTS_MODE);

  const [sessions, setSessions] = useState<HintsSession[]>([]);
  const [config, setConfig] = useState<HintsTrackerConfig | null>(null);
  const [nowMs, setNowMs] = useState<number>(0);
  const [loading, setLoading] = useState<boolean>(false);
  const [error, setError] = useState<HintsEngineError | null>(null);
  const [transport, setTransport] = useState<HintsActiveTransport>(
    requestedMode === 'sse' || requestedMode === 'auto' ? 'sse' : 'polling',
  );

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

  // --------------------------------------------------------------------------
  // Transport effect — drives either SSE (preferred) or polling, with a
  // graceful demotion path when SSE proves unavailable. Re-runs whenever the
  // requested mode, base URL, pause flag, or poll interval changes.
  // --------------------------------------------------------------------------
  useEffect(() => {
    if (opts.paused) return;

    let cancelled = false;
    let pollIntervalId: ReturnType<typeof setInterval> | null = null;
    let connectTimeoutId: ReturnType<typeof setTimeout> | null = null;
    let heartbeatTimeoutId: ReturnType<typeof setTimeout> | null = null;
    let eventSource: EventSource | null = null;
    let errorCount = 0;
    let connected = false;

    const startPolling = () => {
      if (cancelled) return;
      setTransport('polling');
      void refetch();
      if (pollMs <= 0) return;
      pollIntervalId = setInterval(() => {
        void refetch();
      }, pollMs);
    };

    const teardownSse = () => {
      if (connectTimeoutId !== null) {
        clearTimeout(connectTimeoutId);
        connectTimeoutId = null;
      }
      if (heartbeatTimeoutId !== null) {
        clearTimeout(heartbeatTimeoutId);
        heartbeatTimeoutId = null;
      }
      if (eventSource) {
        try {
          eventSource.close();
        } catch {
          // ignore — close is best-effort
        }
        eventSource = null;
      }
    };

    const fallbackToPolling = () => {
      teardownSse();
      if (cancelled) return;
      startPolling();
    };

    const armHeartbeat = () => {
      if (heartbeatTimeoutId !== null) clearTimeout(heartbeatTimeoutId);
      heartbeatTimeoutId = setTimeout(() => {
        // No event (not even the server's 15 s ping) for 30 s — assume the
        // connection is wedged behind a proxy and reconnect.
        if (cancelled) return;
        teardownSse();
        startSse();
      }, SSE_HEARTBEAT_GAP_MS);
    };

    const startSse = () => {
      if (cancelled) return;
      // No EventSource (Node tests, very old browsers) → polling.
      if (typeof EventSource === 'undefined') {
        if (requestedMode === 'sse') {
          // Caller explicitly asked for SSE only — surface a clear error
          // instead of silently degrading.
          setError({
            kind: 'network',
            message: 'EventSource API indisponible dans cet environnement.',
          });
          return;
        }
        startPolling();
        return;
      }

      setTransport('sse');
      // Always pull initial state immediately so the UI is not blank while
      // we wait for the first SSE event.
      void refetch();

      try {
        eventSource = new EventSource(`${baseUrl}/hints/events`);
      } catch (err) {
        const message = err instanceof Error ? err.message : String(err);
        setError({ kind: 'network', message });
        if (requestedMode === 'auto') fallbackToPolling();
        return;
      }

      // 2 s connect timeout — if `onopen` does not fire, demote to polling
      // (only meaningful in `auto` mode; in strict `sse` mode we keep
      // retrying via the native EventSource reconnect loop).
      connectTimeoutId = setTimeout(() => {
        if (cancelled || connected) return;
        if (requestedMode === 'auto') {
          fallbackToPolling();
        }
      }, SSE_CONNECT_TIMEOUT_MS);

      const handleAnyEvent = () => {
        // Authoritative state lives behind /hints/sessions — the SSE event
        // is treated as a "refresh now" signal regardless of its type.
        // (The envelope payload is intentionally minimal server-side.)
        armHeartbeat();
        void refetch();
      };

      eventSource.onopen = () => {
        connected = true;
        errorCount = 0;
        if (connectTimeoutId !== null) {
          clearTimeout(connectTimeoutId);
          connectTimeoutId = null;
        }
        armHeartbeat();
      };

      // Catch-all default channel (covers the unnamed `message` events
      // and the heartbeat ping that some servers emit without a name).
      eventSource.onmessage = () => {
        handleAnyEvent();
      };

      // Named events broadcast by tools/hints/server.py — we don't read the
      // payload, we just refetch on any of them. Listing them explicitly
      // ensures EventSource does not skip them when an `event:` field is set.
      const namedEvents = [
        'hint_served',
        'hint_refused',
        'puzzle_start',
        'attempt_failed',
        'session_reset',
        'ping',
        'test',
      ];
      for (const name of namedEvents) {
        eventSource.addEventListener(name, handleAnyEvent);
      }

      eventSource.onerror = () => {
        errorCount += 1;
        if (errorCount >= SSE_MAX_ERRORS) {
          if (requestedMode === 'auto') {
            fallbackToPolling();
          } else {
            // In strict SSE mode just close to stop the retry storm — the
            // user will see the error state and can retry manually.
            teardownSse();
            setError({
              kind: 'network',
              message: `SSE: ${SSE_MAX_ERRORS} erreurs consécutives.`,
            });
          }
        }
      };
    };

    if (requestedMode === 'polling') {
      startPolling();
    } else {
      startSse();
    }

    return () => {
      cancelled = true;
      if (pollIntervalId !== null) clearInterval(pollIntervalId);
      teardownSse();
      abortRef.current?.abort();
    };
  }, [opts.paused, pollMs, refetch, baseUrl, requestedMode]);

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
    transport,
  };
}
