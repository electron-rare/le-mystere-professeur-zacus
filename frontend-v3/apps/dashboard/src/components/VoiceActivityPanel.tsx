/**
 * VoiceActivityPanel — live status of the voice-bridge (F5-TTS) on MacStudio.
 *
 * Surfaces three things to the GM:
 *   1. Health badge (ready / warmup / down) driven by /health/ready.
 *   2. Compact stats: warmup time, cache hit rate, count, size on disk.
 *   3. Activity pulse: any change in (cache.hits + cache.misses) between two
 *      polls flashes a "voice TTS just synthesized" indicator for 1 s.
 *
 * The /voice/ws WebSocket is consumed by the firmware, not the dashboard, so
 * this panel is the only window the GM has into voice activity in real time.
 */
import { useEffect, useRef, useState } from 'react';
import { useVoiceBridge } from '../hooks/useVoiceBridge.js';

type Status = 'ready' | 'warmup' | 'down';

interface Props {
  /** Override base URL (otherwise read from VITE_VOICE_BRIDGE_URL). */
  baseUrl?: string;
  /** Override poll interval in ms (default 2000). */
  pollMs?: number;
}

function formatHostFromUrl(url: string): string {
  try {
    const u = new URL(url);
    return u.host;
  } catch {
    return url;
  }
}

function deriveStatus(args: {
  error: ReturnType<typeof useVoiceBridge>['error'];
  ready: boolean;
  f5_loaded: boolean;
}): Status {
  if (args.error !== null && !args.ready) return 'down';
  if (args.ready && args.f5_loaded) return 'ready';
  return 'warmup';
}

const STATUS_LABEL: Record<Status, string> = {
  ready: '\u{1F7E2} ready',
  warmup: '\u{1F7E1} warmup',
  down: '\u{1F534} down',
};

const STATUS_TITLE: Record<Status, string> = {
  ready: 'F5-TTS chargé, prêt à synthétiser',
  warmup: 'F5-TTS en cours de chargement (warmup)',
  down: 'Voice-bridge injoignable ou erreur HTTP',
};

const STATUS_CLASS: Record<Status, string> = {
  ready: 'bg-green-500/20 text-green-300',
  warmup: 'bg-yellow-500/20 text-yellow-300',
  down: 'bg-red-500/20 text-red-300',
};

export function VoiceActivityPanel(props: Props) {
  const opts: Parameters<typeof useVoiceBridge>[0] = {};
  if (props.baseUrl !== undefined) opts.baseUrl = props.baseUrl;
  if (props.pollMs !== undefined) opts.pollMs = props.pollMs;
  const {
    ready,
    f5_loaded,
    warmup_ms,
    cache,
    latency_ms_health,
    error,
    loading,
    refetch,
    baseUrl,
    pollMs,
  } = useVoiceBridge(opts);

  const status = deriveStatus({ error, ready, f5_loaded });

  // Activity detection — when (hits + misses) increases between two polls, we
  // know the bridge synthesized at least one new utterance. We pulse a badge
  // for 1 s. We track the previous total in a ref so re-renders triggered by
  // unrelated state (e.g. latency update) don't accidentally re-trigger.
  const lastActivityTotalRef = useRef<number>(cache.hits + cache.misses);
  const [pulsing, setPulsing] = useState<boolean>(false);
  useEffect(() => {
    const total = cache.hits + cache.misses;
    if (total > lastActivityTotalRef.current) {
      setPulsing(true);
      const id = setTimeout(() => setPulsing(false), 1000);
      lastActivityTotalRef.current = total;
      return () => clearTimeout(id);
    }
    lastActivityTotalRef.current = total;
  }, [cache.hits, cache.misses]);

  const hitRatePct = Math.round(cache.hit_rate_since_boot * 100);
  const warmupS = warmup_ms > 0 ? (warmup_ms / 1000).toFixed(1) : '—';

  // One-line monitoring command users can copy-paste from the panel.
  const monitorCmd = `watch -n 2 'curl -s ${baseUrl}/tts/cache/stats | jq'`;

  return (
    <section className="w-72 p-4 border-l border-white/10 overflow-y-auto">
      <header className="flex items-center justify-between mb-3 gap-2">
        <h2 className="text-xs font-semibold text-white/60 uppercase tracking-wide">
          {'\u{1F399}\u{FE0F} Voice Bridge'}
        </h2>
        <div className="flex items-center gap-1.5">
          <span
            className={`text-[9px] px-1.5 py-0.5 rounded-full uppercase tracking-wide font-mono ${
              STATUS_CLASS[status]
            } ${pulsing ? 'animate-pulse ring-2 ring-cyan-300/60' : ''}`}
            title={STATUS_TITLE[status]}
          >
            {STATUS_LABEL[status]}
          </span>
          <button
            onClick={() => void refetch()}
            className="text-[10px] px-2 py-0.5 rounded-full bg-[#2c2c2e] hover:bg-[#3a3a3c] text-white/70"
            title="Recharger maintenant"
          >
            {loading ? '…' : '↻'}
          </button>
        </div>
      </header>

      <div className="text-[10px] text-white/40 mb-3 leading-relaxed">
        <div>
          {formatHostFromUrl(baseUrl)} · poll {Math.round(pollMs / 1000)}s
          {latency_ms_health > 0 && <span> · RTT {latency_ms_health}ms</span>}
        </div>
        <div>
          F5 :{' '}
          <span className={f5_loaded ? 'text-green-400' : 'text-orange-400'}>
            {f5_loaded ? 'chargé' : 'pas chargé'}
          </span>
          {warmup_ms > 0 && <span> · warmup {warmupS}s</span>}
        </div>
      </div>

      {error !== null && (
        <div className="mb-3 p-2 rounded-lg bg-red-500/10 border border-red-500/30 text-[11px] text-red-400">
          Voice-bridge HS : {error.kind === 'http' ? `HTTP ${error.status}` : 'réseau'} —{' '}
          {error.message}
        </div>
      )}

      <div
        className={`rounded-xl border p-2.5 mb-3 transition-colors ${
          pulsing
            ? 'border-cyan-300/60 bg-cyan-300/10'
            : 'border-white/10 bg-[#2c2c2e]'
        }`}
        aria-live="polite"
      >
        <div className="text-[10px] uppercase tracking-wide text-white/50 mb-1.5">
          Cache TTS
        </div>
        <div className="grid grid-cols-2 gap-x-2 gap-y-1 text-[11px] text-white/70">
          <span>
            Entrées :{' '}
            <span className="text-white/95 font-mono">{cache.count}</span>
          </span>
          <span>
            Taille :{' '}
            <span className="text-white/95 font-mono">
              {cache.size_mb.toFixed(1)} Mo
            </span>
          </span>
          <span>
            Hits :{' '}
            <span className="text-white/95 font-mono">{cache.hits}</span>
          </span>
          <span>
            Misses :{' '}
            <span className="text-white/95 font-mono">{cache.misses}</span>
          </span>
          <span className="col-span-2">
            Taux hit :{' '}
            <span
              className={
                hitRatePct >= 50
                  ? 'text-green-400 font-mono'
                  : 'text-orange-300 font-mono'
              }
            >
              {hitRatePct}%
            </span>{' '}
            <span className="text-white/40">depuis boot</span>
          </span>
        </div>
        {pulsing && (
          <div className="mt-1.5 text-[10px] text-cyan-300 italic">
            voice TTS just synthesized
          </div>
        )}
      </div>

      <div className="text-[10px] text-white/40 leading-relaxed">
        <div className="uppercase tracking-wide mb-1">Monitoring</div>
        <code className="block p-1.5 rounded bg-black/40 text-white/70 font-mono text-[10px] break-all whitespace-pre-wrap">
          {monitorCmd}
        </code>
      </div>
    </section>
  );
}
