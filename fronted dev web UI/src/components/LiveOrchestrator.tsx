import { useCallback, useEffect, useMemo, useRef, useState } from 'react'
import { useStoryStream } from '../hooks/useStoryStream'
import { setEspNowEnabled, wifiReconnect } from '../lib/deviceApi'
import type { DeviceCapabilities } from '../lib/deviceApi'
import type { StreamMessage } from '../types/story'
import { Badge, Button, Field, InlineNotice, Panel, SectionHeader } from './ui'

type LiveOrchestratorProps = {
  scenarioId: string
  onSkip: () => Promise<unknown>
  onPause: () => Promise<unknown>
  onResume: () => Promise<unknown>
  onBack: () => void
  capabilities: DeviceCapabilities
}

type EventRecord = {
  id: string
  timestamp: string
  type: string
  payload: string
}

type RunStatus = 'running' | 'paused' | 'done'

type FilterKey = 'all' | 'status' | 'transition' | 'error'

const isRecord = (value: unknown): value is Record<string, unknown> =>
  typeof value === 'object' && value !== null

const formatTimestamp = (ts?: string) => {
  const date = ts ? new Date(ts) : new Date()
  return date.toLocaleTimeString('fr-FR', { hour: '2-digit', minute: '2-digit', second: '2-digit' })
}

const stringifyPayload = (data?: Record<string, unknown>) => {
  try {
    return JSON.stringify(data ?? {}, null, 2)
  } catch {
    return '{"error":"payload_serialize_failed"}'
  }
}

const mapStatusFromPayload = (payload: unknown, fallback: RunStatus): RunStatus => {
  if (!isRecord(payload)) {
    return fallback
  }

  const statusValue = typeof payload.status === 'string' ? payload.status.toLowerCase() : ''
  if (statusValue === 'paused') {
    return 'paused'
  }
  if (statusValue === 'running') {
    return 'running'
  }
  if (statusValue === 'done' || statusValue === 'idle') {
    return 'done'
  }

  const currentStep = typeof payload.current_step === 'string' ? payload.current_step.toUpperCase() : ''
  if (currentStep.includes('DONE')) {
    return 'done'
  }

  return fallback
}

const getDisabledReason = (enabled: boolean, reason: string) => (enabled ? '' : reason)

const FILTER_OPTIONS: { value: FilterKey; label: string }[] = [
  { value: 'all', label: 'Tous' },
  { value: 'status', label: 'status' },
  { value: 'transition', label: 'transition' },
  { value: 'error', label: 'error' },
]

const LiveOrchestrator = ({ scenarioId, onSkip, onPause, onResume, onBack, capabilities }: LiveOrchestratorProps) => {
  const [currentStep, setCurrentStep] = useState('Attente du stream...')
  const [progress, setProgress] = useState(0)
  const [runStatus, setRunStatus] = useState<RunStatus>('running')
  const [events, setEvents] = useState<EventRecord[]>([])
  const [actionError, setActionError] = useState('')
  const [busyAction, setBusyAction] = useState<string | null>(null)
  const [eventFilter, setEventFilter] = useState<FilterKey>('all')
  const [autoScroll, setAutoScroll] = useState(true)
  const logRef = useRef<HTMLDivElement | null>(null)
  const eventCounterRef = useRef(0)

  const appendEvent = useCallback((message: StreamMessage) => {
    const entry: EventRecord = {
      id: `${message.type}-${eventCounterRef.current}`,
      timestamp: formatTimestamp(message.ts),
      type: message.type,
      payload: stringifyPayload(message.data),
    }

    eventCounterRef.current += 1
    setEvents((previous) => [...previous, entry].slice(-100))
  }, [])

  const handleMessage = useCallback(
    (message: StreamMessage) => {
      if (message.type === 'step_change') {
        const step = typeof message.data?.current_step === 'string' ? message.data.current_step : undefined
        const pct = Number(message.data?.progress_pct ?? 0)

        if (step) {
          setCurrentStep(step)
        }

        setRunStatus((current) => mapStatusFromPayload(message.data, current))
        setProgress(Number.isFinite(pct) ? Math.min(Math.max(pct, 0), 100) : 0)
      }

      if (message.type === 'status') {
        setRunStatus((current) => mapStatusFromPayload(message.data, current))

        if (isRecord(message.data?.story)) {
          const liveStep = typeof message.data.story.step === 'string' ? message.data.story.step : undefined
          if (liveStep) {
            setCurrentStep(liveStep)
          }
        }
      }

      appendEvent(message)
    },
    [appendEvent],
  )

  const { status, transport, retryCount, recoveryState } = useStoryStream({ onMessage: handleMessage })

  const runAction = useCallback(
    async (name: string, action: () => Promise<unknown>, fallbackError: string, expectedStatus?: RunStatus) => {
      setActionError('')
      setBusyAction(name)

      try {
        const response = await action()
        if (expectedStatus) {
          setRunStatus(mapStatusFromPayload(response, expectedStatus))
        }
      } catch (error) {
        setActionError(error instanceof Error ? error.message : fallbackError)
      } finally {
        setBusyAction(null)
      }
    },
    [],
  )

  const canPause = runStatus !== 'paused' ? capabilities.canPause : capabilities.canResume
  const pauseReason = runStatus !== 'paused' ? 'Pause indisponible sur ce mode API.' : 'Resume indisponible sur ce mode API.'
  const skipReason = 'Skip indisponible sur ce mode API.'

  const visibleEvents = useMemo(() => {
    if (eventFilter === 'all') {
      return events
    }
    return events.filter((event) => event.type === eventFilter)
  }, [eventFilter, events])

  useEffect(() => {
    if (!autoScroll || !logRef.current) {
      return
    }
    logRef.current.scrollTop = logRef.current.scrollHeight
  }, [autoScroll, visibleEvents])

  const statusBadgeClass = {
    running: 'bg-[var(--teal-500)] text-white',
    paused: 'bg-[var(--accent-500)] text-white',
    done: 'bg-[var(--ink-500)] text-white',
  }[runStatus]

  const streamTone = status === 'open' ? 'success' : status === 'connecting' ? 'warning' : 'error'

  return (
    <section className="space-y-6">
      <Panel>
        <SectionHeader
          title="Orchestrateur live"
          subtitle={`Scenario ${scenarioId} - supervision temps reel`}
          actions={
            <Button type="button" variant="outline" onClick={onBack} size="sm">
              Retour
            </Button>
          }
        />

        <div className="mt-4 flex flex-wrap gap-2">
          <Badge className={statusBadgeClass}>{runStatus}</Badge>
          <Badge tone={streamTone}>Stream: {status === 'open' ? `${transport.toUpperCase()} connecte` : status}</Badge>
          <Badge tone="neutral">Recovery: {recoveryState}</Badge>
          <Badge tone="neutral">Retries: {retryCount}</Badge>
        </div>
      </Panel>

      <div className="grid gap-6 xl:grid-cols-[1fr_1fr_1.2fr]">
        <Panel className="space-y-4">
          <p className="text-xs uppercase tracking-[0.18em] text-[var(--ink-500)]">Runtime</p>
          <div>
            <p className="text-xs uppercase tracking-[0.18em] text-[var(--ink-500)]">Etape courante</p>
            <h3 className="mt-2 text-2xl font-semibold text-[var(--ink-900)]">{currentStep}</h3>
          </div>

          <div>
            <div className="flex items-center justify-between text-xs text-[var(--ink-500)]">
              <span>Progression</span>
              <span>{Math.round(progress)}%</span>
            </div>
            <div className="mt-2 h-3 rounded-full bg-white/70">
              <div className="h-3 rounded-full bg-[var(--teal-500)]" style={{ width: `${Math.round(progress)}%` }} />
            </div>
          </div>

          {status !== 'open' ? (
            <InlineNotice tone="warning">Stream non connecte. Reconnexion automatique active.</InlineNotice>
          ) : null}
        </Panel>

        <Panel className="space-y-4">
          <p className="text-xs uppercase tracking-[0.18em] text-[var(--ink-500)]">Controles story</p>

          <div className="grid gap-3 sm:grid-cols-2">
            <Button
              type="button"
              variant="primary"
              onClick={() => {
                void (runStatus === 'paused'
                  ? runAction('resume', onResume, 'Reprise impossible.', 'running')
                  : runAction('pause', onPause, 'Pause impossible.', 'paused'))
              }}
              disabled={!canPause || busyAction !== null}
              title={getDisabledReason(canPause, pauseReason)}
            >
              {runStatus === 'paused' ? 'Resume' : 'Pause'}
            </Button>

            <Button
              type="button"
              variant="outline"
              onClick={() => {
                void runAction('skip', onSkip, 'Skip impossible.')
              }}
              disabled={!capabilities.canSkip || busyAction !== null}
              title={getDisabledReason(capabilities.canSkip, skipReason)}
            >
              Skip
            </Button>
          </div>

          {!capabilities.canPause || !capabilities.canResume || !capabilities.canSkip ? (
            <InlineNotice tone="info">Certaines actions sont desactivees selon les capabilities de la cible.</InlineNotice>
          ) : null}

          {busyAction ? <InlineNotice tone="info">Action en cours: {busyAction}</InlineNotice> : null}

          {capabilities.canNetworkControl ? (
            <div className="space-y-3 rounded-2xl border border-[var(--mist-400)] bg-white/60 p-3">
              <p className="text-xs uppercase tracking-[0.16em] text-[var(--ink-500)]">Controles reseau (legacy)</p>
              <div className="grid gap-2 sm:grid-cols-3">
                <Button
                  type="button"
                  variant="outline"
                  disabled={busyAction !== null}
                  onClick={() => {
                    void runAction('wifi', wifiReconnect, 'WiFi reconnect impossible.')
                  }}
                >
                  WiFi reconnect
                </Button>
                <Button
                  type="button"
                  variant="outline"
                  disabled={busyAction !== null}
                  onClick={() => {
                    void runAction('espnow_off', () => setEspNowEnabled(false), 'ESP-NOW OFF impossible.')
                  }}
                >
                  ESPNOW OFF
                </Button>
                <Button
                  type="button"
                  variant="outline"
                  disabled={busyAction !== null}
                  onClick={() => {
                    void runAction('espnow_on', () => setEspNowEnabled(true), 'ESP-NOW ON impossible.')
                  }}
                >
                  ESPNOW ON
                </Button>
              </div>
            </div>
          ) : null}

          {actionError ? <InlineNotice tone="error">{actionError}</InlineNotice> : null}
        </Panel>

        <Panel className="space-y-3">
          <SectionHeader
            title="Journal d'evenements"
            subtitle="Historique borne a 100 lignes avec filtres."
            actions={
              <div className="flex items-center gap-2">
                <Badge tone="neutral">{visibleEvents.length} affiches</Badge>
                <label className="inline-flex items-center gap-2 text-xs text-[var(--ink-500)]">
                  <input
                    type="checkbox"
                    className="h-4 w-4 accent-[var(--teal-500)]"
                    checked={autoScroll}
                    onChange={(event) => setAutoScroll(event.target.checked)}
                  />
                  Auto-scroll
                </label>
                <Button
                  variant="ghost"
                  size="sm"
                  onClick={() => {
                    setEvents([])
                  }}
                >
                  Vider
                </Button>
              </div>
            }
          />

          <Field label="Filtre" htmlFor="event-filter">
            <select
              id="event-filter"
              value={eventFilter}
              onChange={(event) => setEventFilter(event.target.value as FilterKey)}
              className="story-input mt-2 min-h-[38px]"
            >
              {FILTER_OPTIONS.map((option) => (
                <option key={option.value} value={option.value}>
                  {option.label}
                </option>
              ))}
            </select>
          </Field>

          <div ref={logRef} className="soft-scrollbar max-h-[360px] space-y-3 overflow-y-auto pr-1 text-xs">
            {visibleEvents.length === 0 ? <p className="text-[var(--ink-500)]">Aucun evenement pour ce filtre.</p> : null}

            {visibleEvents.map((event) => (
              <div key={event.id} className="rounded-2xl border border-white/70 bg-white/75 p-3">
                <div className="flex items-center justify-between text-[10px] uppercase tracking-[0.2em] text-[var(--ink-500)]">
                  <span>{event.timestamp}</span>
                  <span>{event.type}</span>
                </div>
                <pre className="mt-2 whitespace-pre-wrap text-[11px] text-[var(--ink-700)]">{event.payload}</pre>
              </div>
            ))}
          </div>
        </Panel>
      </div>
    </section>
  )
}

export default LiveOrchestrator
