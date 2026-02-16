import { useCallback, useEffect, useMemo, useState } from 'react'
import ScenarioSelector from './components/ScenarioSelector'
import LiveOrchestrator from './components/LiveOrchestrator'
import StoryDesigner from './components/StoryDesigner'
import type { ScenarioMeta } from './types/story'
import {
  API_BASE,
  deployStory,
  getStoryList,
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

  const loadScenarios = useCallback(async () => {
    setLoading(true)
    setError('')
    try {
      const result = await getStoryList()
      setScenarios((result as ScenarioMeta[]) ?? [])
    } catch (err) {
      setError(friendlyError(err, `Cannot reach device at ${API_BASE}. Check connection.`))
    } finally {
      setLoading(false)
    }
  }, [])

  useEffect(() => {
    loadScenarios()
  }, [loadScenarios])

  const handlePlay = useCallback(async (scenarioId: string) => {
    try {
      await selectStory(scenarioId)
      await startStory()
      setActiveScenario(scenarioId)
      setView('orchestrator')
    } catch (err) {
      throw new Error(friendlyError(err, 'Unable to start scenario.'))
    }
  }, [])

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
        const deployResult = (await deployStory(yaml)) as { deployed?: string; status: 'ok' | 'error' }
        if (deployResult.status !== 'ok' || !deployResult.deployed) {
          throw new Error('Deploy failed before test run.')
        }
        await selectStory(deployResult.deployed)
        await startStory()
        setActiveScenario(deployResult.deployed)
        setView('orchestrator')
        window.setTimeout(() => {
          setView('selector')
        }, 30000)
      } catch (err) {
        throw new Error(friendlyError(err, 'Test run failed.'))
      }
    },
    [],
  )

  const pageContent = useMemo(() => {
    if (view === 'selector') {
      return (
        <ScenarioSelector
          scenarios={scenarios}
          loading={loading}
          error={error}
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
          onPause={pauseStory}
          onResume={resumeStory}
          onSkip={skipStory}
        />
      )
    }

    return (
      <StoryDesigner onValidate={handleValidate} onDeploy={handleDeploy} onTestRun={handleTestRun} />
    )
  }, [view, scenarios, loading, error, loadScenarios, handlePlay, activeScenario, handleDeploy, handleTestRun, handleValidate])

  return (
    <div className="min-h-screen px-4 py-8 md:px-10">
      <header className="glass-panel mx-auto flex max-w-6xl flex-wrap items-center justify-between gap-4 rounded-3xl px-6 py-4">
        <div>
          <p className="text-xs uppercase tracking-[0.3em] text-[var(--ink-500)]">Story V2</p>
          <h1 className="text-3xl font-semibold">Mission Control</h1>
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
