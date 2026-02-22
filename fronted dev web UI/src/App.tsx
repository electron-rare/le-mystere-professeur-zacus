import { Suspense, lazy, useCallback, useEffect, useMemo, useRef, useState } from 'react'
import LiveOrchestrator from './components/LiveOrchestrator'
import ScenarioSelector from './components/ScenarioSelector'
import { Badge, Button, InlineNotice, Panel } from './components/ui'
import {
  API_BASE,
  type ApiFlavor,
  type DeviceCapabilities,
  type FirmwareInfo,
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
import type { ScenarioMeta } from './types/story'

const StoryDesigner = lazy(() => import('./components/StoryDesigner'))

type ViewKey = 'selector' | 'orchestrator' | 'designer'

const VIEW_LABELS: Record<ViewKey, string> = {
  selector: 'Scenarios',
  orchestrator: 'Orchestrateur',
  designer: 'Designer',
}

const FLAVOR_LABELS: Record<ApiFlavor, string> = {
  story_v2: 'API Story V2',
  freenove_legacy: 'API Legacy',
  unknown: 'API inconnue',
}

const friendlyError = (err: unknown, fallback: string) => {
  if (err && typeof err === 'object' && 'status' in err) {
    const status = Number((err as { status?: number }).status)
    if (status === 404) {
      return 'Scenario introuvable. Verifie la liste disponible.'
    }
    if (status === 409) {
      return 'Le device est occupe. Reessaie dans quelques secondes.'
    }
    if (status === 507) {
      return "Stockage du device plein. Supprime d'anciens scenarios."
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
  const [firmwareInfo, setFirmwareInfo] = useState<FirmwareInfo | null>(null)
  const testRunTimerRef = useRef<number | null>(null)

  const capabilityBadges = useMemo(
    () => [
      { key: 'select', label: 'Select', enabled: capabilities.canSelectScenario },
      { key: 'start', label: 'Start', enabled: capabilities.canStart },
      { key: 'pause', label: 'Pause', enabled: capabilities.canPause && capabilities.canResume },
      { key: 'skip', label: 'Skip', enabled: capabilities.canSkip },
      { key: 'validate', label: 'Validate', enabled: capabilities.canValidate },
      { key: 'deploy', label: 'Deploy', enabled: capabilities.canDeploy },
    ],
    [
      capabilities.canDeploy,
      capabilities.canPause,
      capabilities.canResume,
      capabilities.canSelectScenario,
      capabilities.canSkip,
      capabilities.canStart,
      capabilities.canValidate,
    ],
  )

  const loadScenarios = useCallback(async () => {
    setLoading(true)
    setError('')

    try {
      const runtime = await getRuntimeInfo()
      setApiFlavor(runtime.flavor)
      setCapabilities(runtime.capabilities)
      setFirmwareInfo(runtime.firmwareInfo)
      setResolvedBase(runtime.base)

      const result = await listScenarios()
      setScenarios(result)
      setActiveScenario((previous) => previous || result[0]?.id || '')
    } catch (err) {
      setError(friendlyError(err, `Impossible de joindre le device sur ${resolvedBase}.`))
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
        throw new Error(friendlyError(err, "Impossible d'ouvrir le scenario."))
      }
    },
    [capabilities.canSelectScenario, capabilities.canStart],
  )

  const handleValidate = useCallback(async (yaml: string) => {
    try {
      return (await validateStory(yaml)) as { valid: boolean; errors?: string[] }
    } catch (err) {
      throw new Error(friendlyError(err, 'Validation en erreur.'))
    }
  }, [])

  const handleDeploy = useCallback(async (yaml: string) => {
    try {
      return (await deployStory(yaml)) as { deployed?: string; status: 'ok' | 'error'; message?: string }
    } catch (err) {
      throw new Error(friendlyError(err, 'Deploiement en erreur.'))
    }
  }, [])

  const handleTestRun = useCallback(
    async (yaml: string) => {
      try {
        if (!capabilities.canDeploy || !capabilities.canSelectScenario || !capabilities.canStart) {
          throw new Error('Le test run est disponible uniquement avec les APIs Story V2 deploy/select/start.')
        }

        const deployResult = (await deployStory(yaml)) as { deployed?: string; status: 'ok' | 'error' }
        if (deployResult.status !== 'ok' || !deployResult.deployed) {
          throw new Error('Le deploiement a echoue avant le test run.')
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
        throw new Error(friendlyError(err, 'Test run en erreur.'))
      }
    },
    [capabilities.canDeploy, capabilities.canSelectScenario, capabilities.canStart],
  )

  const handlePause = useCallback(async () => {
    try {
      await pauseStory()
    } catch (err) {
      throw new Error(friendlyError(err, 'Impossible de mettre en pause.'))
    }
  }, [])

  const handleResume = useCallback(async () => {
    try {
      await resumeStory()
    } catch (err) {
      throw new Error(friendlyError(err, 'Impossible de reprendre.'))
    }
  }, [])

  const handleSkip = useCallback(async () => {
    try {
      await skipStory()
    } catch (err) {
      throw new Error(friendlyError(err, "Impossible de passer l'etape."))
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
          scenarioId={activeScenario || 'INCONNU'}
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
    <div className="min-h-screen px-4 py-6 md:px-8 md:py-8">
      <div className="mx-auto flex max-w-7xl flex-col gap-4">
        <Panel as="header" className="sticky top-3 z-20 border-white/80 p-4 md:p-5">
          <div className="grid gap-3 lg:grid-cols-[1fr_auto] lg:items-center">
            <div className="space-y-1.5">
              <p className="text-[11px] uppercase tracking-[0.22em] text-[var(--ink-500)]">
                {FLAVOR_LABELS[apiFlavor]}
              </p>
              <h1 className="text-2xl font-semibold sm:text-3xl">Mission Control Zacus</h1>
              <p className="text-sm text-[var(--ink-500)]">Cible active : {resolvedBase}</p>
            </div>
            <nav
              className="panel-stack flex flex-wrap items-center gap-2 rounded-full px-2 py-2"
              aria-label="Navigation principale"
            >
              {(Object.keys(VIEW_LABELS) as ViewKey[]).map((key) => (
                <Button
                  key={key}
                  type="button"
                  onClick={() => setView(key)}
                  variant={view === key ? 'primary' : 'ghost'}
                  size="sm"
                  aria-current={view === key ? 'page' : undefined}
                  className="min-h-[34px] px-4"
                >
                  {VIEW_LABELS[key]}
                </Button>
              ))}
            </nav>
          </div>

          <div className="mt-4 grid gap-2 sm:grid-cols-2 lg:grid-cols-[1.2fr_auto]">
            <div className="grid gap-2 sm:grid-cols-2 lg:grid-cols-3">
              <Badge tone={error ? 'error' : loading ? 'warning' : 'success'}>
                {error ? 'Hors ligne' : loading ? 'Connexion...' : 'Connecté'}
              </Badge>
              <Badge tone={capabilities.streamKind === 'none' ? 'warning' : 'info'}>Flux : {capabilities.streamKind.toUpperCase()}</Badge>
              <Badge tone={capabilities.canNetworkControl ? 'warning' : 'neutral'}>
                {capabilities.canNetworkControl ? 'Mode réseau' : 'Mode classique'}
              </Badge>
              <Badge tone={capabilities.canDeploy ? 'success' : 'neutral'}>
                {capabilities.canDeploy ? 'Déploiement' : 'Pas de déploiement'}
              </Badge>
            </div>
            <div className="flex flex-wrap justify-start gap-1.5 lg:justify-end">
              {capabilityBadges.map((capability) => (
                <Badge key={capability.key} tone={capability.enabled ? 'success' : 'neutral'}>
                  {capability.label}
                </Badge>
              ))}
            </div>
          </div>
        </Panel>

        {firmwareInfo ? (
          <InlineNotice
            tone={firmwareInfo.warnings.length ? 'warning' : 'success'}
            className="border border-white/70"
          >
            <p className="font-semibold">
              Firmware{firmwareInfo.version ? ` ${firmwareInfo.version}` : ' non versionné'} · OTA:{' '}
              <strong>{firmwareInfo.canFirmwareUpdate ? 'actif' : 'non dispo'}</strong>, reboot:{' '}
              <strong>{firmwareInfo.canFirmwareReboot ? 'actif' : 'non dispo'}</strong>
            </p>
            {firmwareInfo.warnings.length ? (
              <ul className="mt-2 list-disc space-y-1 pl-5 text-sm">
                {firmwareInfo.warnings.map((warning) => (
                  <li key={warning}>{warning}</li>
                ))}
              </ul>
            ) : null}
          </InlineNotice>
        ) : null}

        <main>

        <Suspense
          fallback={
            <Panel className="p-6">
              <p className="text-sm text-[var(--ink-500)]">Chargement de l'interface...</p>
            </Panel>
          }
        >
          {pageContent}
        </Suspense>
        </main>
      </div>
    </div>
  )
}

export default App
