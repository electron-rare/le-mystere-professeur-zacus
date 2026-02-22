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
  const canLaunch = capabilities.canSelectScenario && capabilities.canStart
  const playLabel = canLaunch ? 'Lancer' : 'Surveiller'
  const modeLabel = legacyMode ? 'Legacy' : 'Story V2'

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
      <Panel className="space-y-3">
        <SectionHeader
          title="Sélection des scénarios"
          subtitle="Choisis un scénario et lance-le sur la cible."
          actions={
            <Button type="button" variant="outline" onClick={onRetry} size="sm">
              Rafraichir
            </Button>
          }
        />

        <div className="mt-4 grid gap-3 md:grid-cols-[minmax(0,1fr)_220px]">
          <Field label="Recherche" htmlFor="scenario-search">
            <div className="relative">
              <input
                id="scenario-search"
                value={query}
                onChange={(event) => setQuery(event.target.value)}
                placeholder="ID scénario ou description"
                className="story-input mt-2 pr-10"
              />
              {query ? (
                <button
                  type="button"
                  className="focus-ring absolute right-2 top-1/2 mt-1 -translate-y-1/2 rounded-full border border-white/60 bg-white/70 px-2 text-xs text-[var(--ink-600)]"
                  onClick={() => setQuery('')}
                >
                  ✕
                </button>
              ) : null}
            </div>
          </Field>

          <Field label="Tri" htmlFor="scenario-sort">
            <select
              id="scenario-sort"
              value={sortKey}
              onChange={(event) => setSortKey(event.target.value as SortKey)}
              className="story-input mt-2"
            >
              <option value="name">Nom</option>
              <option value="duration">Durée</option>
            </select>
          </Field>
        </div>

        <div className="mt-4 flex flex-wrap gap-2">
          <Badge tone="info">Mode: {modeLabel}</Badge>
          <Badge tone="neutral">Scénarios: {scenarios.length}</Badge>
          <Badge tone="neutral">Résultats: {filteredScenarios.length}</Badge>
          <Badge tone={legacyMode ? 'warning' : 'success'}>{legacyMode ? 'Mode Legacy' : 'Mode Story V2'}</Badge>
        </div>

        {error ? <InlineNotice tone="error">{error}</InlineNotice> : null}

        <div className="mt-4 grid gap-2 md:grid-cols-2 xl:grid-cols-4">
          <InlineNotice tone={canLaunch ? 'success' : 'warning'} className="md:col-span-2">
            <p>
              {canLaunch
                ? 'Contrôles complets disponibles: sélection, lancement et actions de run.'
                : 'Lancement direct indisponible: mode en lecture seule ou contrôles limités.'}
            </p>
          </InlineNotice>

          <InlineNotice tone="info">
            <p>Sortie: clic sur une carte pour lancer, ou sur "surveiller" pour ouvrir le monitor.</p>
          </InlineNotice>

          <InlineNotice tone="info">
            <p>Scénarios disponibles: {scenarios.length}</p>
          </InlineNotice>
        </div>
      </Panel>

      {loading ? (
        <div className="grid gap-4 sm:grid-cols-2 xl:grid-cols-3">
          {Array.from({ length: 3 }).map((_, index) => (
            <Panel key={`loading-${index}`} className="h-40 animate-pulse" />
          ))}
        </div>
      ) : null}

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
            <Panel
              key={scenario.id}
              as="article"
              className="group panel-stack flex h-full flex-col justify-between gap-3 border border-white/70 p-5 transition duration-200 hover:-translate-y-0.5 hover:shadow-[0_20px_45px_rgba(15,23,42,0.1)]"
            >
              <div className="space-y-3">
                <div className="flex flex-wrap items-start justify-between gap-2">
                  <div className="space-y-1">
                    <h3 className="text-lg font-semibold text-[var(--ink-900)]">{scenario.id}</h3>
                    <p className="text-[11px] uppercase tracking-[0.16em] text-[var(--ink-500)]">
                      identifiant
                    </p>
                  </div>
                  {scenario.is_current ? <Badge tone="success">Courant</Badge> : <Badge tone="neutral">Disponible</Badge>}
                </div>
                <p className="text-xs uppercase tracking-[0.18em] text-[var(--ink-500)]">{formatDuration(scenario)}</p>
                <p className="line-clamp-3 text-sm leading-relaxed text-[var(--ink-700)]">
                  {scenario.description ?? 'Aucune description fournie.'}
                </p>
                {scenario.current_step ? (
                  <p className="text-xs uppercase tracking-[0.16em] text-[var(--ink-500)]">
                    Étape courante: {scenario.current_step}
                  </p>
                ) : null}
              </div>

              <div className="grid gap-2 pt-1">
                <div className="flex items-center gap-2 text-xs text-[var(--ink-500)]">
                  <span>{scenario.is_current ? 'Actif' : 'Prêt'}</span>
                  <span>•</span>
                  <span>{formatDuration(scenario)}</span>
                </div>
                <Button
                  type="button"
                  variant="secondary"
                  onClick={() => {
                    void handlePlay(scenario.id)
                  }}
                  disabled={activeId === scenario.id || !canLaunch && !legacyMode}
                  fullWidth
                >
                  {activeId === scenario.id ? 'Connexion...' : playLabel}
                </Button>
                {scenario.is_current ? (
                  <p className="text-xs text-[var(--ink-500)]">
                    Scénario courant : <strong>{scenario.id}</strong>
                  </p>
                ) : null}
              </div>
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
