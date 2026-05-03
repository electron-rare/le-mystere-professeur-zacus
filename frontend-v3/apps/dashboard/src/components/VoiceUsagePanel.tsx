/**
 * VoiceUsagePanel — live cost-audit view of the voice-bridge `/usage/stats`
 * endpoint. Surfaces, in three sections to the GM:
 *
 *   1. LLM tokens (npc-fast + hints-deep) — total since boot/reset and the
 *      tokens-per-minute rate over the rolling history window kept by
 *      `useVoiceUsage` (default 5 min).
 *   2. F5 TTS — calls, seconds synthesised, cache hit rate.
 *   3. STT — calls, audio seconds transcribed.
 *
 * A tiny SVG sparkline tracks the cumulative token total over the window so
 * the GM can spot bursts at a glance. A "Reset" button POSTs to
 * `/usage/reset` (with `X-Admin-Key` if configured) after a confirm dialog.
 *
 * Status pill:
 *   🟢 normal       — combined tokens/min ≤ VOICE_USAGE_BURST_TPM
 *   🟡 burst        — combined tokens/min > VOICE_USAGE_BURST_TPM
 *   🔴 down         — `/usage/stats` unreachable
 */
import { useMemo } from 'react';
import { VOICE_USAGE_BURST_TPM, type VoiceUsageBucket } from '@zacus/shared';
import { useVoiceUsage } from '../hooks/useVoiceUsage.js';

interface Props {
  /** Override base URL (otherwise read from VITE_VOICE_BRIDGE_URL). */
  baseUrl?: string;
  /** Override poll interval in ms (default 5000 — `/usage/stats` is cheap but slow-moving). */
  pollMs?: number;
  /** Admin key for `POST /usage/reset` (otherwise VITE_VOICE_BRIDGE_ADMIN_KEY). */
  adminKey?: string;
}

type Status = 'normal' | 'burst' | 'down';

const STATUS_LABEL: Record<Status, string> = {
  normal: '\u{1F7E2} normal',
  burst: '\u{1F7E1} burst',
  down: '\u{1F534} down',
};

const STATUS_CLASS: Record<Status, string> = {
  normal: 'bg-green-500/20 text-green-300',
  burst: 'bg-yellow-500/20 text-yellow-300',
  down: 'bg-red-500/20 text-red-300',
};

const STATUS_TITLE: Record<Status, string> = {
  normal: `LLM < ${VOICE_USAGE_BURST_TPM} tokens/min sur la fenêtre`,
  burst: `LLM > ${VOICE_USAGE_BURST_TPM} tokens/min — surveillance coût`,
  down: 'Voice-bridge /usage/stats injoignable',
};

function formatUptime(s: number): string {
  if (!Number.isFinite(s) || s <= 0) return '—';
  const total = Math.floor(s);
  const h = Math.floor(total / 3600);
  const m = Math.floor((total % 3600) / 60);
  const sec = total % 60;
  if (h > 0) return `${h}h ${m}m`;
  if (m > 0) return `${m}m ${sec}s`;
  return `${sec}s`;
}

/**
 * Compute combined LLM tokens-per-minute over the rolling history window
 * by diffing the oldest and newest snapshots and dividing by the wall-clock
 * span between them. Returns 0 when fewer than two samples are available.
 */
function computeTokensPerMinute(history: VoiceUsageBucket[]): number {
  if (history.length < 2) return 0;
  const first = history[0];
  const last = history[history.length - 1];
  if (!first || !last) return 0;
  const spanMs = last.receivedAtMs - first.receivedAtMs;
  if (spanMs <= 0) return 0;
  const deltaTokens =
    last.stats.npc_fast.total_tokens +
    last.stats.hints_deep.total_tokens -
    (first.stats.npc_fast.total_tokens + first.stats.hints_deep.total_tokens);
  if (deltaTokens <= 0) return 0;
  return (deltaTokens * 60_000) / spanMs;
}

/**
 * Build a 60×16 SVG sparkline polyline string from the cumulative LLM token
 * series. Returns null when there aren't enough points to draw a line.
 */
function buildSparklinePoints(history: VoiceUsageBucket[]): string | null {
  if (history.length < 2) return null;
  const series = history.map(
    (b) => b.stats.npc_fast.total_tokens + b.stats.hints_deep.total_tokens,
  );
  const min = Math.min(...series);
  const max = Math.max(...series);
  const span = max - min || 1;
  const w = 60;
  const h = 16;
  const stepX = w / Math.max(1, series.length - 1);
  return series
    .map((v, i) => {
      const x = i * stepX;
      const y = h - ((v - min) / span) * h;
      return `${x.toFixed(1)},${y.toFixed(1)}`;
    })
    .join(' ');
}

export function VoiceUsagePanel(props: Props) {
  const opts: Parameters<typeof useVoiceUsage>[0] = {};
  if (props.baseUrl !== undefined) opts.baseUrl = props.baseUrl;
  if (props.pollMs !== undefined) opts.pollMs = props.pollMs;
  if (props.adminKey !== undefined) opts.adminKey = props.adminKey;

  const { usage, history, loading, error, refetch, resetUsage, baseUrl, pollMs } =
    useVoiceUsage(opts);

  const tokensPerMin = useMemo(() => computeTokensPerMinute(history), [history]);
  const sparkline = useMemo(() => buildSparklinePoints(history), [history]);

  const status: Status =
    error !== null && usage === null
      ? 'down'
      : tokensPerMin > VOICE_USAGE_BURST_TPM
        ? 'burst'
        : 'normal';

  const llmTotal =
    (usage?.npc_fast.total_tokens ?? 0) + (usage?.hints_deep.total_tokens ?? 0);
  const llmCalls = (usage?.npc_fast.calls ?? 0) + (usage?.hints_deep.calls ?? 0);

  // Cache hit rate for F5: hits / (hits + f5_calls). f5_calls already counts
  // synthesis-misses (cache hits short-circuit before F5 runs), so the
  // denominator is the total request volume. Safe against div-by-zero.
  const ttsTotalReq = (usage?.tts.cache_hits ?? 0) + (usage?.tts.f5_calls ?? 0);
  const ttsHitRate = ttsTotalReq > 0 ? Math.round(((usage?.tts.cache_hits ?? 0) / ttsTotalReq) * 100) : 0;

  const onReset = async () => {
    // Confirm before nuking the accounting window — the GM rarely wants this.
    const ok = window.confirm(
      'Réinitialiser tous les compteurs /usage/stats du voice-bridge ?',
    );
    if (!ok) return;
    try {
      await resetUsage();
    } catch (err) {
      // resetUsage already pushed the error into the hook state — surface a
      // visible alert too, otherwise the reset failure stays silent for any
      // GM not already watching the panel.
      const message = err instanceof Error ? err.message : String(err);
      window.alert(`Échec du reset : ${message}`);
    }
  };

  return (
    <section className="w-72 p-4 border-l border-white/10 overflow-y-auto">
      <header className="flex items-center justify-between mb-3 gap-2">
        <h2 className="text-xs font-semibold text-white/60 uppercase tracking-wide">
          {'\u{1F4CA} Voice Usage'}
        </h2>
        <div className="flex items-center gap-1.5">
          <span
            className={`text-[9px] px-1.5 py-0.5 rounded-full uppercase tracking-wide font-mono ${STATUS_CLASS[status]}`}
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
          poll {Math.round(pollMs / 1000)}s · uptime {formatUptime(usage?.uptime_s ?? 0)}
        </div>
        <div className="truncate" title={baseUrl}>
          {baseUrl.replace(/^https?:\/\//, '')}
        </div>
      </div>

      {error !== null && (
        <div className="mb-3 p-2 rounded-lg bg-red-500/10 border border-red-500/30 text-[11px] text-red-400">
          /usage/stats HS : {error.kind === 'http' ? `HTTP ${error.status}` : 'réseau'} —{' '}
          {error.message}
        </div>
      )}

      {/* LLM tokens — full-width because of the sparkline. */}
      <div className="rounded-xl border border-white/10 bg-[#2c2c2e] p-2.5 mb-3">
        <div className="flex items-center justify-between mb-1.5">
          <span className="text-[10px] uppercase tracking-wide text-white/50">
            LLM tokens
          </span>
          {sparkline !== null && (
            <svg width="60" height="16" className="opacity-80">
              <polyline
                points={sparkline}
                fill="none"
                stroke={status === 'burst' ? '#fbbf24' : '#34d399'}
                strokeWidth="1.2"
              />
            </svg>
          )}
        </div>
        <div className="grid grid-cols-2 gap-x-2 gap-y-1 text-[11px] text-white/70">
          <span>
            Total :{' '}
            <span className="text-white/95 font-mono">{llmTotal.toLocaleString('fr-FR')}</span>
          </span>
          <span>
            Appels :{' '}
            <span className="text-white/95 font-mono">{llmCalls}</span>
          </span>
          <span className="col-span-2">
            Débit :{' '}
            <span
              className={
                tokensPerMin > VOICE_USAGE_BURST_TPM
                  ? 'text-yellow-300 font-mono'
                  : 'text-white/95 font-mono'
              }
            >
              {Math.round(tokensPerMin).toLocaleString('fr-FR')} tok/min
            </span>{' '}
            <span className="text-white/40">(fenêtre {history.length}×{Math.round(pollMs / 1000)}s)</span>
          </span>
        </div>
        <div className="mt-1.5 grid grid-cols-2 gap-x-2 text-[10px] text-white/50">
          <span>
            npc-fast :{' '}
            <span className="text-white/80 font-mono">
              {(usage?.npc_fast.total_tokens ?? 0).toLocaleString('fr-FR')}
            </span>
          </span>
          <span>
            hints-deep :{' '}
            <span className="text-white/80 font-mono">
              {(usage?.hints_deep.total_tokens ?? 0).toLocaleString('fr-FR')}
            </span>
          </span>
        </div>
      </div>

      {/* F5 TTS + STT — two-column grid. */}
      <div className="grid grid-cols-2 gap-2 mb-3">
        <div className="rounded-xl border border-white/10 bg-[#2c2c2e] p-2.5">
          <div className="text-[10px] uppercase tracking-wide text-white/50 mb-1">
            F5 TTS
          </div>
          <div className="flex flex-col gap-0.5 text-[11px] text-white/70">
            <span>
              Calls :{' '}
              <span className="text-white/95 font-mono">{usage?.tts.f5_calls ?? 0}</span>
            </span>
            <span>
              Secondes :{' '}
              <span className="text-white/95 font-mono">
                {(usage?.tts.f5_seconds ?? 0).toFixed(1)}
              </span>
            </span>
            <span>
              Cache hits :{' '}
              <span className="text-white/95 font-mono">{usage?.tts.cache_hits ?? 0}</span>
            </span>
            <span>
              Taux hit :{' '}
              <span
                className={
                  ttsHitRate >= 50
                    ? 'text-green-400 font-mono'
                    : 'text-orange-300 font-mono'
                }
              >
                {ttsHitRate}%
              </span>
            </span>
          </div>
        </div>
        <div className="rounded-xl border border-white/10 bg-[#2c2c2e] p-2.5">
          <div className="text-[10px] uppercase tracking-wide text-white/50 mb-1">
            STT
          </div>
          <div className="flex flex-col gap-0.5 text-[11px] text-white/70">
            <span>
              Calls :{' '}
              <span className="text-white/95 font-mono">{usage?.stt.calls ?? 0}</span>
            </span>
            <span>
              Audio :{' '}
              <span className="text-white/95 font-mono">
                {(usage?.stt.audio_seconds ?? 0).toFixed(1)}s
              </span>
            </span>
          </div>
        </div>
      </div>

      <button
        onClick={() => void onReset()}
        className="w-full text-[11px] px-2 py-1 rounded-lg bg-red-500/15 hover:bg-red-500/25 text-red-300 border border-red-500/30"
        title="POST /usage/reset (X-Admin-Key requis si configuré côté serveur)"
      >
        Reset compteurs
      </button>
    </section>
  );
}
