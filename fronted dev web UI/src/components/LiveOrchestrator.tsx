import { useCallback, useEffect, useRef, useState } from 'react'
import { useStoryStream } from '../hooks/useStoryStream'
import type { DeviceCapabilities } from '../lib/deviceApi'
import type { StreamMessage } from '../types/story'

type LiveOrchestratorProps = {
  scenarioId: string
  onSkip: () => Promise<void>
  onPause: () => Promise<void>
  onResume: () => Promise<void>
  onBack: () => void
  capabilities: DeviceCapabilities
}

type EventRecord = {
  id: string
  timestamp: string
  type: string
  payload: string
}

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

const LiveOrchestrator = ({ scenarioId, onSkip, onPause, onResume, onBack, capabilities }: LiveOrchestratorProps) => {
  const [currentStep, setCurrentStep] = useState('Awaiting stream...')
  const [progress, setProgress] = useState(0)
  const [runStatus, setRunStatus] = useState<'running' | 'paused' | 'done'>('running')
  const [events, setEvents] = useState<EventRecord[]>([])
  const [actionError, setActionError] = useState('')
  const [busyAction, setBusyAction] = useState<string | null>(null)
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
          if (step.toUpperCase().includes('DONE')) {
            setRunStatus('done')
          } else {
            setRunStatus('running')
          }
        }
        setProgress(Number.isFinite(pct) ? Math.min(Math.max(pct, 0), 100) : 0)
      }

      if (message.type === 'status') {
        const statusValue = typeof message.data?.status === 'string' ? message.data.status : ''
        if (statusValue === 'paused') {
          setRunStatus('paused')
        } else if (statusValue === 'running') {
          setRunStatus('running')
        } else if (statusValue === 'done' || statusValue === 'idle') {
          setRunStatus('done')
        }

        if (isRecord(message.data?.story)) {
          const liveStep = typeof message.data.story.step === 'string' ? message.data.story.step : undefined
          if (liveStep) {
            setCurrentStep(liveStep)
            if (liveStep.toUpperCase().includes('DONE')) {
              setRunStatus('done')
            }
          }
        }
      }

      if (message.type === 'transition' || message.type === 'audit_log' || message.type === 'error') {
        appendEvent(message)
      }
    },
    [appendEvent],
  )

  const { status, transport } = useStoryStream({ onMessage: handleMessage })

  const runAction = useCallback(async (name: string, action: () => Promise<void>, nextStatus?: 'running' | 'paused') => {
    setActionError('')
    setBusyAction(name)
    try {
      await action()
      if (nextStatus) {
        setRunStatus(nextStatus)
      }
    } catch (error) {
      setActionError(error instanceof Error ? error.message : 'Action failed')
    } finally {
      setBusyAction(null)
    }
  }, [])

  const canPause = runStatus !== 'paused' ? capabilities.canPause : capabilities.canResume

  useEffect(() => {
    if (logRef.current) {
      logRef.current.scrollTop = logRef.current.scrollHeight
    }
  }, [events])

  const statusBadge = {
    running: 'bg-[var(--teal-500)] text-white',
    paused: 'bg-[var(--accent-500)] text-white',
    done: 'bg-[var(--ink-500)] text-white',
  }[runStatus]

  return (
    <section className="space-y-6">
      <div className="flex flex-wrap items-center justify-between gap-3">
        <div>
          <h2 className="text-2xl font-semibold">Live Orchestrator</h2>
          <p className="text-sm text-[var(--ink-500)]">Scenario {scenarioId}</p>
        </div>
        <div className="flex items-center gap-2 text-xs uppercase tracking-[0.2em] text-[var(--ink-500)]">
          <span className={`rounded-full px-3 py-1 text-[10px] font-semibold ${statusBadge}`}>{runStatus}</span>
          <span className="rounded-full border border-[var(--ink-500)] px-3 py-1">
            {status === 'open' ? `Connected (${transport.toUpperCase()})` : 'Disconnected'}
          </span>
        </div>
      </div>

      <div className="grid gap-6 lg:grid-cols-[1fr_1.4fr_1fr]">
        <div className="glass-panel flex flex-col gap-4 rounded-3xl p-6">
          <div>
            <p className="text-xs uppercase tracking-[0.2em] text-[var(--ink-500)]">Current step</p>
            <h3 className="mt-2 text-2xl font-semibold">{currentStep}</h3>
          </div>
          <div>
            <div className="flex items-center justify-between text-xs text-[var(--ink-500)]">
              <span>Progress</span>
              <span>{Math.round(progress)}%</span>
            </div>
            <div className="mt-2 h-3 rounded-full bg-white/70">
              <div className="h-3 rounded-full bg-[var(--teal-500)]" style={{ width: `${Math.round(progress)}%` }} />
            </div>
          </div>
          {status !== 'open' && (
            <div className="rounded-2xl border border-[var(--accent-700)] bg-white/60 p-3 text-xs text-[var(--accent-700)]">
              Live stream disconnected. Retrying...
            </div>
          )}
        </div>

        <div className="glass-panel flex flex-col gap-4 rounded-3xl p-6">
          <p className="text-xs uppercase tracking-[0.2em] text-[var(--ink-500)]">Controls</p>
          <div className="flex flex-wrap gap-3">
            <button
              type="button"
              onClick={() =>
                runStatus === 'paused'
                  ? runAction('resume', onResume, 'running')
                  : runAction('pause', onPause, 'paused')
              }
              disabled={!canPause || busyAction !== null}
              className="focus-ring min-h-[44px] flex-1 rounded-full bg-[var(--ink-700)] px-4 text-sm font-semibold text-white disabled:cursor-not-allowed disabled:opacity-70"
            >
              {runStatus === 'paused' ? 'Resume' : 'Pause'}
            </button>
            <button
              type="button"
              onClick={() => runAction('skip', onSkip)}
              disabled={!capabilities.canSkip || busyAction !== null}
              className="focus-ring min-h-[44px] flex-1 rounded-full border border-[var(--ink-700)] px-4 text-sm font-semibold text-[var(--ink-700)] disabled:cursor-not-allowed disabled:opacity-70"
            >
              Skip
            </button>
            <button
              type="button"
              onClick={onBack}
              className="focus-ring min-h-[44px] flex-1 rounded-full border border-[var(--ink-500)] px-4 text-sm font-semibold text-[var(--ink-500)]"
            >
              Back
            </button>
          </div>
          {actionError && (
            <div className="rounded-2xl border border-[var(--accent-700)] bg-white/60 p-3 text-xs text-[var(--accent-700)]">
              {actionError}
            </div>
          )}
          <div className="rounded-2xl bg-white/60 p-3 text-xs text-[var(--ink-500)]">
            Some controls can be disabled depending on the API mode detected on the device.
          </div>
        </div>

        <div className="glass-panel flex flex-col rounded-3xl p-6">
          <p className="text-xs uppercase tracking-[0.2em] text-[var(--ink-500)]">Event log</p>
          <div ref={logRef} className="mt-4 max-h-[320px] space-y-3 overflow-y-auto pr-2 text-xs">
            {events.length === 0 && <p className="text-[var(--ink-500)]">Waiting for events...</p>}
            {events.map((event) => (
              <div key={event.id} className="rounded-2xl border border-white/60 bg-white/70 p-3">
                <div className="flex items-center justify-between text-[10px] uppercase tracking-[0.2em] text-[var(--ink-500)]">
                  <span>{event.timestamp}</span>
                  <span>{event.type}</span>
                </div>
                <pre className="mt-2 whitespace-pre-wrap text-[11px] text-[var(--ink-700)]">{event.payload}</pre>
              </div>
            ))}
          </div>
        </div>
      </div>
    </section>
  )
}

export default LiveOrchestrator
