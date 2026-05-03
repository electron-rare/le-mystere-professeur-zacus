/**
 * useGameConfig — REST hook for the slice 12 ESP32 master + the hints
 * adaptive engine.
 *
 * Reads:
 *   - GET  http://<esp32>/game/group_profile           → currentProfile
 *   - GET  http://<hints>/healthz                      → adaptiveEnabled
 *                                                        + availableProfiles
 *
 * Writes:
 *   - POST http://<esp32>/game/group_profile           body: { group_profile }
 *
 * Configuration (Vite env, all optional):
 *   VITE_ESP32_BASE_URL    base URL of the ESP32 master
 *                          (default: http://zacus-master.local)
 *   VITE_HINTS_BASE_URL    base URL of the hints engine
 *                          (default: http://localhost:8311)
 *
 * Both endpoints are queried in parallel; either one can fail without
 * tearing down the other (UI shows partial state with a per-side error
 * message). The hook is intentionally polling-only — group profile is
 * a sticky operator setting, no SSE channel is needed.
 */
import { useCallback, useEffect, useMemo, useRef, useState } from 'react';
import {
  ESP32_DEFAULT_BASE_URL,
  HINTS_DEFAULT_BASE_URL,
  type GameGroupProfileResponse,
  type GameProfile,
  type HintsAdaptiveProfile,
  type HintsHealthz,
} from '@zacus/shared';

export type GameConfigError =
  | { kind: 'network'; message: string }
  | { kind: 'http'; status: number; message: string };

export interface UseGameConfigOptions {
  /** Override ESP32 master base URL (otherwise read from VITE_ESP32_BASE_URL). */
  esp32BaseUrl?: string;
  /** Override hints engine base URL (otherwise read from VITE_HINTS_BASE_URL). */
  hintsBaseUrl?: string;
  /** Disable the on-mount fetch (manual mode — call refetch()). */
  paused?: boolean;
}

export interface UseGameConfigResult {
  /** Current profile reported by the ESP32 master (null while unknown). */
  currentProfile: GameProfile | null;
  /** Profile names exposed by the hints engine, e.g. ["TECH","NON_TECH",...]. */
  availableProfiles: string[];
  /** Whether the adaptive engine is enabled in /healthz. */
  adaptiveEnabled: boolean;
  /** Adaptive profile configs (if /healthz exposes them; healthz today only
   *  exposes the names — config payload remains optional for forward-compat). */
  adaptiveProfileConfigs: Record<string, HintsAdaptiveProfile> | null;
  /** True while a fetch is in flight (any side). */
  loading: boolean;
  /** Error from the ESP32 side (null if last request succeeded). */
  esp32Error: GameConfigError | null;
  /** Error from the hints engine side (null if last request succeeded). */
  hintsError: GameConfigError | null;
  /** POST a new profile to the ESP32, then refetch. Returns true on success. */
  setProfile: (profile: GameProfile) => Promise<boolean>;
  /** Re-pull both endpoints. */
  refetch: () => Promise<void>;
  /** Resolved base URLs (after env + opts merge). */
  esp32BaseUrl: string;
  hintsBaseUrl: string;
}

interface ImportMetaEnvLike {
  VITE_ESP32_BASE_URL?: string;
  VITE_HINTS_BASE_URL?: string;
}

function readEnv(): ImportMetaEnvLike {
  try {
    // eslint-disable-next-line @typescript-eslint/no-explicit-any
    const env = (import.meta as any)?.env;
    return (env as ImportMetaEnvLike) ?? {};
  } catch {
    return {};
  }
}

export function useGameConfig(opts: UseGameConfigOptions = {}): UseGameConfigResult {
  const env = useMemo(() => readEnv(), []);
  const esp32BaseUrl = (opts.esp32BaseUrl ?? env.VITE_ESP32_BASE_URL ?? ESP32_DEFAULT_BASE_URL)
    .replace(/\/+$/, '');
  const hintsBaseUrl = (opts.hintsBaseUrl ?? env.VITE_HINTS_BASE_URL ?? HINTS_DEFAULT_BASE_URL)
    .replace(/\/+$/, '');

  const [currentProfile, setCurrentProfile] = useState<GameProfile | null>(null);
  const [availableProfiles, setAvailableProfiles] = useState<string[]>([]);
  const [adaptiveEnabled, setAdaptiveEnabled] = useState<boolean>(false);
  const [adaptiveProfileConfigs, setAdaptiveProfileConfigs] = useState<
    Record<string, HintsAdaptiveProfile> | null
  >(null);
  const [loading, setLoading] = useState<boolean>(false);
  const [esp32Error, setEsp32Error] = useState<GameConfigError | null>(null);
  const [hintsError, setHintsError] = useState<GameConfigError | null>(null);

  const abortRef = useRef<AbortController | null>(null);

  const fetchEsp32 = useCallback(
    async (signal: AbortSignal): Promise<void> => {
      try {
        const resp = await fetch(`${esp32BaseUrl}/game/group_profile`, {
          headers: { Accept: 'application/json' },
          signal,
        });
        if (!resp.ok) {
          setEsp32Error({
            kind: 'http',
            status: resp.status,
            message: `GET /game/group_profile → ${resp.status}`,
          });
          return;
        }
        const data = (await resp.json()) as GameGroupProfileResponse;
        if (data && typeof data.group_profile === 'string') {
          setCurrentProfile(data.group_profile);
        }
        setEsp32Error(null);
      } catch (err) {
        if (err instanceof DOMException && err.name === 'AbortError') return;
        const message = err instanceof Error ? err.message : String(err);
        setEsp32Error({ kind: 'network', message });
      }
    },
    [esp32BaseUrl],
  );

  const fetchHealthz = useCallback(
    async (signal: AbortSignal): Promise<void> => {
      try {
        const resp = await fetch(`${hintsBaseUrl}/healthz`, {
          headers: { Accept: 'application/json' },
          signal,
        });
        if (!resp.ok) {
          setHintsError({
            kind: 'http',
            status: resp.status,
            message: `GET /healthz → ${resp.status}`,
          });
          return;
        }
        const data = (await resp.json()) as HintsHealthz & {
          adaptive_profile_configs?: Record<string, HintsAdaptiveProfile>;
        };
        setAdaptiveEnabled(Boolean(data.adaptive_enabled));
        setAvailableProfiles(Array.isArray(data.adaptive_profiles) ? data.adaptive_profiles : []);
        // Forward-compat: /healthz today only exposes profile *names*. If the
        // server later inlines the per-profile configs, surface them as-is.
        setAdaptiveProfileConfigs(data.adaptive_profile_configs ?? null);
        setHintsError(null);
      } catch (err) {
        if (err instanceof DOMException && err.name === 'AbortError') return;
        const message = err instanceof Error ? err.message : String(err);
        setHintsError({ kind: 'network', message });
      }
    },
    [hintsBaseUrl],
  );

  const refetch = useCallback(async () => {
    abortRef.current?.abort();
    const ctrl = new AbortController();
    abortRef.current = ctrl;
    setLoading(true);
    try {
      await Promise.all([fetchEsp32(ctrl.signal), fetchHealthz(ctrl.signal)]);
    } finally {
      setLoading(false);
    }
  }, [fetchEsp32, fetchHealthz]);

  const setProfile = useCallback(
    async (profile: GameProfile): Promise<boolean> => {
      try {
        const resp = await fetch(`${esp32BaseUrl}/game/group_profile`, {
          method: 'POST',
          headers: {
            'Content-Type': 'application/json',
            Accept: 'application/json',
          },
          body: JSON.stringify({ group_profile: profile }),
        });
        if (!resp.ok) {
          setEsp32Error({
            kind: 'http',
            status: resp.status,
            message: `POST /game/group_profile → ${resp.status}`,
          });
          return false;
        }
        // Optimistic + authoritative refresh.
        setCurrentProfile(profile);
        setEsp32Error(null);
        await refetch();
        return true;
      } catch (err) {
        const message = err instanceof Error ? err.message : String(err);
        setEsp32Error({ kind: 'network', message });
        return false;
      }
    },
    [esp32BaseUrl, refetch],
  );

  useEffect(() => {
    if (opts.paused) return;
    void refetch();
    return () => {
      abortRef.current?.abort();
    };
  }, [opts.paused, refetch]);

  return {
    currentProfile,
    availableProfiles,
    adaptiveEnabled,
    adaptiveProfileConfigs,
    loading,
    esp32Error,
    hintsError,
    setProfile,
    refetch,
    esp32BaseUrl,
    hintsBaseUrl,
  };
}
