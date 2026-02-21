import { useCallback, useEffect, useMemo, useRef, useState } from 'react'
import ScenarioSelector from './components/ScenarioSelector'
import LiveOrchestrator from './components/LiveOrchestrator'
import StoryDesigner from './components/StoryDesigner'
import type { ScenarioMeta } from './types/story'
import {
  API_BASE,
  type ApiFlavor,
  type DeviceCapabilities,
  deployStory,
  getCapabilities,
  getRuntimeInfo,
  listScenarios,
  pauseStory,
  resumeStory,
  selectStory,
  skipStory,
  startStory,
  validateStory,
} from './lib/api'

type ViewKey = 'selector' | 'orchestrator' | 'designer'

const VIEW_LABELS: Record<ViewKey, string> = {
  selector: 'Scenario Selector',
  orchestrator: 'Live Orchestrator',
  designer: 'Story Designer',
}

const FLAVOR_LABELS: Record<ApiFlavor, string> = {
  story_v2: 'Story V2 API',
  freenove_legacy: 'Freenove Legacy API',
  unknown: 'Unknown API',
}

const friendlyError = (err: unknown, fallback: string) => {
  if (err && typeof err === 'object' && 'status' in err) {
    const status = Number((err as { status?: number }).status)
    if (status === 404) {
      return 'Scenario not found. Browse available scenarios.'
    }
    if (status === 409) {
      return 'Device is busy. Try again shortly.'
    }
    if (status === 507) {
      return 'Device storage full. Delete old scenarios.'
    }
  }
  if (err instanceof Error && err.message) {
    return err.message
  }
  return fallback
}

const App = () => {
  const [view, setView] = useState<ViewKey>('selector')
  const [scenarios, setScenarios] = useState<ScenarioMeta[]>([])
  const [loading, setLoading] = useState(true)
  const [error, setError] = useState('')
  const [activeScenario, setActiveScenario] = useState<string>('')
  const [apiFlavor, setApiFlavor] = useState<ApiFlavor>('unknown')
  const [capabilities, setCapabilities] = useState<DeviceCapabilities>(() => getCapabilities('unknown'))
  const [resolvedBase, setResolvedBase] = useState(API_BASE)
  const testRunTimerRef = useRef<number | null>(null)

  const loadScenarios = useCallback(async () => {
    setLoading(true)
    setError('')
    try {
      const runtime = await getRuntimeInfo()
      setApiFlavor(runtime.flavor)
      setCapabilities(runtime.capabilities)
      setResolvedBase(runtime.base)

      const result = await listScenarios()
      setScenarios(result)
      setActiveScenario((previous) => previous || result[0]?.id || '')
    } catch (err) {
      setError(friendlyError(err, `Cannot reach device at ${resolvedBase}. Check connection.`))
    } finally {
      setLoading(false)
    }
  }, [resolvedBase])

  useEffect(() => {
    void loadScenarios()
  }, [loadScenarios])

  useEffect(
    () => () => {
      if (testRunTimerRef.current) {
        window.clearTimeout(testRunTimerRef.current)
      }
    },
    [],
  )

  const handlePlay = useCallback(
    async (scenarioId: string) => {
      try {
        if (capabilities.canSelectScenario) {
          await selectStory(scenarioId)
        }
        if (capabilities.canStart) {
          await startStory()
        }
        setActiveScenario(scenarioId)
        setView('orchestrator')
      } catch (err) {
        throw new Error(friendlyError(err, 'Unable to open scenario.'))
      }
    },
    [capabilities.canSelectScenario, capabilities.canStart],
  )

  const handleValidate = useCallback(async (yaml: string) => {
    try {
      return (await validateStory(yaml)) as { valid: boolean; errors?: string[] }
    } catch (err) {
      throw new Error(friendlyError(err, 'Validation failed.'))
    }
  }, [])

  const handleDeploy = useCallback(async (yaml: string) => {
    try {
      return (await deployStory(yaml)) as { deployed?: string; status: 'ok' | 'error'; message?: string }
    } catch (err) {
      throw new Error(friendlyError(err, 'Deployment failed.'))
    }
  }, [])

  const handleTestRun = useCallback(
    async (yaml: string) => {
      try {
        if (!capabilities.canDeploy || !capabilities.canSelectScenario || !capabilities.canStart) {
          throw new Error('Test run is only available when Story V2 deploy/start APIs are enabled.')
        }

        const deployResult = (await deployStory(yaml)) as { deployed?: string; status: 'ok' | 'error' }
        if (deployResult.status !== 'ok' || !deployResult.deployed) {
          throw new Error('Deploy failed before test run.')
        }
        await selectStory(deployResult.deployed)
        await startStory()
        setActiveScenario(deployResult.deployed)
        setView('orchestrator')
        if (testRunTimerRef.current) {
          window.clearTimeout(testRunTimerRef.current)
        }
        testRunTimerRef.current = window.setTimeout(() => {
          setView('selector')
          testRunTimerRef.current = null
        }, 30000)
      } catch (err) {
        throw new Error(friendlyError(err, 'Test run failed.'))
      }
    },
    [capabilities.canDeploy, capabilities.canSelectScenario, capabilities.canStart],
  )

  const handlePause = useCallback(async () => {
    try {
      await pauseStory()
    } catch (err) {
      throw new Error(friendlyError(err, 'Unable to pause scenario.'))
    }
  }, [])

  const handleResume = useCallback(async () => {
    try {
      await resumeStory()
    } catch (err) {
      throw new Error(friendlyError(err, 'Unable to resume scenario.'))
    }
  }, [])

  const handleSkip = useCallback(async () => {
    try {
      await skipStory()
    } catch (err) {
      throw new Error(friendlyError(err, 'Unable to skip step.'))
    }
  }, [])

  const pageContent = useMemo(() => {
    if (view === 'selector') {
      return (
        <ScenarioSelector
          scenarios={scenarios}
          loading={loading}
          error={error}
          flavor={apiFlavor}
          capabilities={capabilities}
          onRetry={loadScenarios}
          onPlay={handlePlay}
        />
      )
    }

    if (view === 'orchestrator') {
      return (
        <LiveOrchestrator
          scenarioId={activeScenario || 'Unknown'}
          onBack={() => setView('selector')}
          onPause={handlePause}
          onResume={handleResume}
          onSkip={handleSkip}
          capabilities={capabilities}
        />
      )
    }

    return (
      <StoryDesigner
        onValidate={handleValidate}
        onDeploy={handleDeploy}
        onTestRun={handleTestRun}
        capabilities={capabilities}
      />
    )
  }, [
    view,
    scenarios,
    loading,
    error,
    apiFlavor,
    capabilities,
    loadScenarios,
    handlePlay,
    activeScenario,
    handlePause,
    handleResume,
    handleSkip,
    handleDeploy,
    handleTestRun,
    handleValidate,
  ])

  return (
    <div className="min-h-screen px-4 py-8 md:px-10">
      <header className="glass-panel mx-auto flex max-w-6xl flex-wrap items-center justify-between gap-4 rounded-3xl px-6 py-4">
        <div>
          <p className="text-xs uppercase tracking-[0.3em] text-[var(--ink-500)]">
            {FLAVOR_LABELS[apiFlavor]}
          </p>
          <h1 className="text-3xl font-semibold">Mission Control</h1>
          <p className="text-xs text-[var(--ink-500)]">Target {resolvedBase}</p>
        </div>
        <nav className="flex flex-wrap gap-2">
          {(Object.keys(VIEW_LABELS) as ViewKey[]).map((key) => (
            <button
              key={key}
              type="button"
              onClick={() => setView(key)}
              className={`focus-ring min-h-[44px] rounded-full px-4 text-sm font-semibold transition ${
                view === key
                  ? 'bg-[var(--ink-700)] text-white'
                  : 'border border-[var(--ink-500)] text-[var(--ink-700)]'
              }`}
              aria-current={view === key ? 'page' : undefined}
            >
              {VIEW_LABELS[key]}
            </button>
          ))}
        </nav>
      </header>

      <main className="mx-auto mt-8 max-w-6xl">{pageContent}</main>
    </div>
  )
}

export default App
