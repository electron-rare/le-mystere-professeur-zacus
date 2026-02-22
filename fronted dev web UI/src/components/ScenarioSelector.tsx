import { useMemo, useState } from 'react'
import type { ApiFlavor, DeviceCapabilities } from '../lib/deviceApi'
import type { ScenarioMeta } from '../types/story'
import { Badge, Button, Field, InlineNotice, Panel, SectionHeader } from './ui'

const formatDuration = (scenario: ScenarioMeta) => {
  const seconds = scenario.estimated_duration_s ?? scenario.duration_s
  if (!seconds) {
    return 'Inconnue'
  }

  const minutes = Math.round(seconds / 60)
  return minutes < 1 ? `${seconds}s` : `${minutes} min`
}

type SortKey = 'name' | 'duration'

type ScenarioSelectorProps = {
  scenarios: ScenarioMeta[]
  loading: boolean
  flavor: ApiFlavor
  capabilities: DeviceCapabilities
  error?: string
  onRetry: () => void
  onPlay: (scenarioId: string) => Promise<void>
}

const ScenarioSelector = ({ scenarios, loading, flavor, capabilities, error, onRetry, onPlay }: ScenarioSelectorProps) => {
  const [activeId, setActiveId] = useState<string | null>(null)
  const [actionError, setActionError] = useState('')
  const [query, setQuery] = useState('')
  const [sortKey, setSortKey] = useState<SortKey>('name')

  const legacyMode = flavor === 'freenove_legacy'
  const playLabel = capabilities.canSelectScenario || capabilities.canStart ? 'Lancer' : 'Ouvrir monitor'

  const filteredScenarios = useMemo(() => {
    const normalizedQuery = query.trim().toUpperCase()
    const filtered = scenarios.filter((scenario) => {
      if (!normalizedQuery) {
        return true
      }
      return (
        scenario.id.toUpperCase().includes(normalizedQuery) ||
        (scenario.description ?? '').toUpperCase().includes(normalizedQuery)
      )
    })

    const sorted = [...filtered]
    sorted.sort((left, right) => {
      if (sortKey === 'duration') {
        const leftDuration = left.estimated_duration_s ?? left.duration_s ?? Number.MAX_SAFE_INTEGER
        const rightDuration = right.estimated_duration_s ?? right.duration_s ?? Number.MAX_SAFE_INTEGER
        if (leftDuration !== rightDuration) {
          return leftDuration - rightDuration
        }
      }

      return left.id.localeCompare(right.id)
    })

    return sorted
  }, [query, scenarios, sortKey])

  const hasSingleCurrentScenario = scenarios.length === 1 && scenarios[0]?.is_current

  const handlePlay = async (scenarioId: string) => {
    setActionError('')
    setActiveId(scenarioId)

    try {
      await onPlay(scenarioId)
    } catch (err) {
      setActionError(err instanceof Error ? err.message : 'Impossible de lancer le scenario')
      setActiveId(null)
    }
  }

  return (
    <section className="space-y-6">
      <Panel>
        <SectionHeader
          title="Selection des scenarios"
          subtitle="Choisis un scenario puis lance-le sur le device connecte."
          actions={
            <Button type="button" variant="outline" onClick={onRetry}>
              Rafraichir
            </Button>
          }
        />

        <div className="mt-4 grid gap-3 md:grid-cols-[minmax(0,1fr)_220px]">
          <Field label="Recherche" htmlFor="scenario-search">
            <input
              id="scenario-search"
              value={query}
              onChange={(event) => setQuery(event.target.value)}
              placeholder="ID scenario ou texte"
              className="focus-ring mt-2 min-h-[42px] w-full rounded-xl border border-[var(--mist-400)] bg-white/80 px-3 text-sm"
            />
          </Field>

          <Field label="Tri" htmlFor="scenario-sort">
            <select
              id="scenario-sort"
              value={sortKey}
              onChange={(event) => setSortKey(event.target.value as SortKey)}
              className="focus-ring mt-2 min-h-[42px] w-full rounded-xl border border-[var(--mist-400)] bg-white/80 px-3 text-sm"
            >
              <option value="name">Nom</option>
              <option value="duration">Duree</option>
            </select>
          </Field>
        </div>

        <div className="mt-4 flex flex-wrap gap-2">
          <Badge tone="info">Flavour: {flavor}</Badge>
          <Badge tone="neutral">Scenarios: {scenarios.length}</Badge>
          {legacyMode ? <Badge tone="warning">Mode legacy</Badge> : <Badge tone="success">Mode Story V2</Badge>}
        </div>
      </Panel>

      {loading ? (
        <div className="grid gap-4 sm:grid-cols-2 xl:grid-cols-3">
          {Array.from({ length: 3 }).map((_, index) => (
            <Panel key={`loading-${index}`} className="h-40 animate-pulse" />
          ))}
        </div>
      ) : null}

      {error ? <InlineNotice tone="error">{error}</InlineNotice> : null}

      {actionError ? <InlineNotice tone="error">{actionError}</InlineNotice> : null}

      {legacyMode ? (
        <InlineNotice tone="warning">
          Mode legacy detecte: selection/start sont indisponibles. Tu peux surveiller le run en direct et utiliser les
          controles autorises.
        </InlineNotice>
      ) : null}

      {!loading && !error && scenarios.length === 0 ? (
        <InlineNotice tone="info">Aucun metadata scenario remonte par le device.</InlineNotice>
      ) : null}

      {!loading && !error && hasSingleCurrentScenario ? (
        <InlineNotice tone="info">
          Le firmware renvoie uniquement la story courante. Tu peux ouvrir le monitor pour piloter en live.
        </InlineNotice>
      ) : null}

      {!loading && !error && filteredScenarios.length > 0 ? (
        <div className="grid gap-4 sm:grid-cols-2 xl:grid-cols-3">
          {filteredScenarios.map((scenario) => (
            <Panel key={scenario.id} as="article" className="flex h-full flex-col justify-between gap-4 p-5">
              <div className="space-y-2">
                <div className="flex items-center justify-between gap-2">
                  <h3 className="text-xl font-semibold text-[var(--ink-900)]">{scenario.id}</h3>
                  {scenario.is_current ? <Badge tone="success">Courant</Badge> : null}
                </div>
                <p className="text-xs uppercase tracking-[0.2em] text-[var(--ink-500)]">{formatDuration(scenario)}</p>
                <p className="text-sm text-[var(--ink-700)]">{scenario.description ?? 'Aucune description fournie.'}</p>
                {scenario.current_step ? (
                  <p className="text-xs uppercase tracking-[0.2em] text-[var(--ink-500)]">Etape {scenario.current_step}</p>
                ) : null}
              </div>

              <Button
                type="button"
                variant="secondary"
                onClick={() => {
                  void handlePlay(scenario.id)
                }}
                disabled={activeId === scenario.id}
                fullWidth
              >
                {activeId === scenario.id ? 'Ouverture...' : playLabel}
              </Button>
            </Panel>
          ))}
        </div>
      ) : null}

      {!loading && !error && filteredScenarios.length === 0 && scenarios.length > 0 ? (
        <InlineNotice tone="info">Aucun scenario ne correspond a la recherche courante.</InlineNotice>
      ) : null}
    </section>
  )
}

export default ScenarioSelector
