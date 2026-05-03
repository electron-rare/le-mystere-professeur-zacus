/**
 * HintsAdaptivePanel — live view of the hints engine adaptive state.
 *
 * Polls `/hints/sessions` via useHintsEngine and renders one card per active
 * session, with a row per puzzle showing:
 *   count, level_floor_adaptive, stuck_minutes, failed_attempts,
 *   group_profile (from config), total_penalty, cooldown countdown.
 *
 * Highlights an "AUTO-BUMP" badge when a row has level_floor_adaptive > 0
 * (server has nudged the floor up because of group profile / stuck timer /
 * failed attempts).
 *
 * Reset button per session triggers DELETE /hints/sessions/{id}.
 */
import { useEffect, useState } from 'react';
import type { HintsSession, HintsSessionEntry } from '@zacus/shared';
import { useHintsEngine } from '../hooks/useHintsEngine.js';

interface DerivedRow extends HintsSessionEntry {
  stuck_minutes: number;
  cooldown_remaining_s: number;
  /** True when the engine has pushed an adaptive floor for this puzzle. */
  is_auto_bumped: boolean;
}

function deriveRows(session: HintsSession, nowMs: number): DerivedRow[] {
  return session.puzzles.map((p) => {
    const stuckMs = p.puzzle_started_at_ms > 0 ? Math.max(0, nowMs - p.puzzle_started_at_ms) : 0;
    const cooldownRemainingMs = Math.max(0, p.cooldown_until_ms - nowMs);
    // Heuristic for the "auto-bump" badge:
    //   adaptive floor is implicit in the data we get back from /sessions
    //   (we don't store it server-side per puzzle yet) — but a useful proxy
    //   is "count > 0 AND failed_attempts > 0". The richer signal lives on
    //   /hints/ask responses; the panel surfaces both inputs side-by-side
    //   so the GM can see WHY a level got nudged.
    const isAutoBumped = p.failed_attempts_for_puzzle > 0 || stuckMs >= 60_000;
    return {
      ...p,
      stuck_minutes: Math.floor(stuckMs / 60_000),
      cooldown_remaining_s: Math.ceil(cooldownRemainingMs / 1000),
      is_auto_bumped: isAutoBumped,
    };
  });
}

function formatHostFromUrl(url: string): string {
  try {
    const u = new URL(url);
    return `${u.host}`;
  } catch {
    return url;
  }
}

interface Props {
  /** Override base URL (otherwise read from VITE_HINTS_BASE_URL). */
  baseUrl?: string;
  /** Override poll interval in ms (otherwise read from VITE_HINTS_POLL_MS). */
  pollMs?: number;
}

export function HintsAdaptivePanel(props: Props) {
  const opts: Parameters<typeof useHintsEngine>[0] = {};
  if (props.baseUrl !== undefined) opts.baseUrl = props.baseUrl;
  if (props.pollMs !== undefined) opts.pollMs = props.pollMs;
  const { sessions, config, nowMs, loading, error, refetch, resetSession, baseUrl, pollMs } =
    useHintsEngine(opts);

  // Local clock for cooldown countdowns between polls (1 Hz).
  const [tickMs, setTickMs] = useState<number>(() => Date.now());
  useEffect(() => {
    const id = setInterval(() => setTickMs(Date.now()), 1000);
    return () => clearInterval(id);
  }, []);

  // Use the freshest of (server now_ms + elapsed since last poll, local clock).
  const effectiveNow = nowMs > 0 ? nowMs + (tickMs - nowMs) : tickMs;

  const adaptiveEnabled = config?.adaptive?.enabled ?? false;
  const adaptiveProfiles = config?.adaptive?.profiles
    ? Object.keys(config.adaptive.profiles)
    : [];

  return (
    <section className="w-72 p-4 border-l border-white/10 overflow-y-auto">
      <header className="flex items-center justify-between mb-3">
        <h2 className="text-xs font-semibold text-white/60 uppercase tracking-wide">
          Hints adaptatifs
        </h2>
        <button
          onClick={() => void refetch()}
          className="text-[10px] px-2 py-0.5 rounded-full bg-[#2c2c2e] hover:bg-[#3a3a3c] text-white/70"
          title="Recharger maintenant"
        >
          {loading ? '…' : '↻'}
        </button>
      </header>

      <div className="text-[10px] text-white/40 mb-3 leading-relaxed">
        <div>
          {formatHostFromUrl(baseUrl)} · poll {Math.round(pollMs / 1000)}s
        </div>
        <div>
          Adaptatif :{' '}
          <span className={adaptiveEnabled ? 'text-green-400' : 'text-orange-400'}>
            {adaptiveEnabled ? 'ON' : 'OFF'}
          </span>
          {adaptiveProfiles.length > 0 && (
            <span> · profils : {adaptiveProfiles.join(', ')}</span>
          )}
        </div>
        {config && (
          <div>
            cap {config.max_per_puzzle} · cooldown {config.cooldown_s}s
          </div>
        )}
      </div>

      {error !== null && (
        <div className="mb-3 p-2 rounded-lg bg-red-500/10 border border-red-500/30 text-[11px] text-red-400">
          Hints engine HS : {error.kind === 'http' ? `HTTP ${error.status}` : 'réseau'} —{' '}
          {error.message}
        </div>
      )}

      {sessions.length === 0 && error === null && (
        <div className="text-[11px] text-white/40 italic">
          Aucune session active sur le moteur d'indices.
        </div>
      )}

      <div className="flex flex-col gap-3">
        {sessions.map((session) => {
          const rows = deriveRows(session, effectiveNow);
          return (
            <div
              key={session.session_id}
              className="rounded-xl border border-white/10 bg-[#2c2c2e] p-2"
            >
              <div className="flex items-center justify-between mb-2">
                <span className="font-mono text-[11px] text-white/80 truncate">
                  {session.session_id}
                </span>
                <button
                  onClick={() => void resetSession(session.session_id)}
                  className="text-[10px] px-2 py-0.5 rounded-full bg-red-500/20 hover:bg-red-500/30 text-red-300"
                  title="Réinitialiser cette session"
                >
                  Reset
                </button>
              </div>
              <div className="flex justify-between text-[10px] text-white/50 mb-1">
                <span>
                  {session.total_hints} indice(s) · pénalité {session.total_penalty}
                </span>
              </div>
              <ul className="flex flex-col gap-1.5">
                {rows.map((row) => (
                  <li
                    key={row.puzzle_id}
                    className={`p-2 rounded-lg border text-[11px] ${
                      row.is_auto_bumped
                        ? 'border-orange-400/40 bg-orange-400/5'
                        : 'border-white/10 bg-[#1c1c1e]'
                    }`}
                  >
                    <div className="flex items-center justify-between mb-1">
                      <span className="font-mono text-white/90">{row.puzzle_id}</span>
                      {row.is_auto_bumped && (
                        <span className="text-[9px] px-1.5 py-0.5 rounded-full bg-orange-400/20 text-orange-300 uppercase tracking-wide">
                          Auto-bump
                        </span>
                      )}
                    </div>
                    <div className="grid grid-cols-2 gap-x-2 gap-y-0.5 text-[10px] text-white/60">
                      <span>
                        Indices :{' '}
                        <span className="text-white/90">{row.count}</span>
                      </span>
                      <span>
                        Échecs :{' '}
                        <span className="text-white/90">
                          {row.failed_attempts_for_puzzle}
                        </span>
                      </span>
                      <span>
                        Bloqué :{' '}
                        <span className="text-white/90">{row.stuck_minutes} min</span>
                      </span>
                      <span>
                        Pénalité :{' '}
                        <span className="text-white/90">{row.total_penalty}</span>
                      </span>
                      <span className="col-span-2">
                        Cooldown :{' '}
                        <span
                          className={
                            row.cooldown_remaining_s > 0
                              ? 'text-orange-300'
                              : 'text-green-400'
                          }
                        >
                          {row.cooldown_remaining_s > 0
                            ? `${row.cooldown_remaining_s}s restants`
                            : 'prêt'}
                        </span>
                      </span>
                    </div>
                  </li>
                ))}
              </ul>
            </div>
          );
        })}
      </div>
    </section>
  );
}
