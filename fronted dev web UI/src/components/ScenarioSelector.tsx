import { useState } from 'react'
import type { ScenarioMeta } from '../types/story'

const formatDuration = (scenario: ScenarioMeta) => {
  const seconds = scenario.estimated_duration_s ?? scenario.duration_s
  if (!seconds) {
    return 'Unknown'
  }
  const minutes = Math.round(seconds / 60)
  return minutes < 1 ? `${seconds}s` : `${minutes} min`
}

type ScenarioSelectorProps = {
  scenarios: ScenarioMeta[]
  loading: boolean
  error?: string
  onRetry: () => void
  onPlay: (scenarioId: string) => Promise<void>
}

const ScenarioSelector = ({ scenarios, loading, error, onRetry, onPlay }: ScenarioSelectorProps) => {
  const [activeId, setActiveId] = useState<string | null>(null)
  const [actionError, setActionError] = useState('')

  const handlePlay = async (scenarioId: string) => {
    setActionError('')
    setActiveId(scenarioId)
    try {
      await onPlay(scenarioId)
    } catch (err) {
      setActionError(err instanceof Error ? err.message : 'Unable to start scenario')
      setActiveId(null)
    }
  }

  return (
    <section className="space-y-6">
      <div className="flex flex-wrap items-center justify-between gap-3">
        <div>
          <h2 className="text-2xl font-semibold text-[var(--ink-900)]">Scenario Selector</h2>
          <p className="text-sm text-[var(--ink-500)]">Choose a story and launch it on the device.</p>
        </div>
        <button
          type="button"
          onClick={onRetry}
          className="focus-ring min-h-[44px] rounded-full border border-[var(--ink-500)] px-4 text-sm font-semibold text-[var(--ink-700)]"
        >
          Refresh
        </button>
      </div>

      {loading && (
        <div className="grid gap-4 sm:grid-cols-2 xl:grid-cols-4">
          {Array.from({ length: 4 }).map((_, index) => (
            <div key={`loading-${index}`} className="glass-panel h-40 animate-pulse rounded-3xl" />
          ))}
        </div>
      )}

      {error && (
        <div className="glass-panel rounded-2xl border border-[var(--accent-700)] p-4 text-sm text-[var(--accent-700)]">
          {error}
        </div>
      )}

      {actionError && (
        <div className="glass-panel rounded-2xl border border-[var(--accent-700)] p-4 text-sm text-[var(--accent-700)]">
          {actionError}
        </div>
      )}

      {!loading && !error && (
        <div className="grid gap-4 sm:grid-cols-2 xl:grid-cols-4">
          {scenarios.map((scenario) => (
            <article key={scenario.id} className="glass-panel flex h-full flex-col justify-between gap-4 rounded-3xl p-5">
              <div className="space-y-2">
                <h3 className="text-xl font-semibold">{scenario.id}</h3>
                <div className="text-xs uppercase tracking-[0.2em] text-[var(--ink-500)]">{formatDuration(scenario)}</div>
                <p className="text-sm text-[var(--ink-700)]">
                  {scenario.description ?? 'No description provided.'}
                </p>
              </div>
              <button
                type="button"
                onClick={() => handlePlay(scenario.id)}
                disabled={activeId === scenario.id}
                className="focus-ring min-h-[44px] rounded-full bg-[var(--accent-500)] px-4 text-sm font-semibold text-white transition hover:bg-[var(--accent-700)] disabled:cursor-not-allowed disabled:opacity-70"
              >
                {activeId === scenario.id ? 'Starting...' : 'Play'}
              </button>
            </article>
          ))}
        </div>
      )}
    </section>
  )
}

export default ScenarioSelector
