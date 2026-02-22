import { useEffect, useMemo, useState } from 'react'
import CodeMirror from '@uiw/react-codemirror'
import { yaml } from '@codemirror/lang-yaml'
import type { DeviceCapabilities } from '../lib/deviceApi'

type ValidationResult = {
  valid: boolean
  errors?: string[]
}

type DeployResult = {
  deployed?: string
  status: 'ok' | 'error'
  message?: string
}

type StoryDesignerProps = {
  onValidate: (yaml: string) => Promise<ValidationResult>
  onDeploy: (yaml: string) => Promise<DeployResult>
  onTestRun: (yaml: string) => Promise<void>
  capabilities: DeviceCapabilities
}

type NodalStyle = 'linear' | 'fork_merge' | 'hub'

type NodalTransition = {
  event: string
  target: string
}

type NodalStep = {
  id: string
  screen: string
  audio?: string
  transitions: NodalTransition[]
}

const TEMPLATE_LIBRARY: Record<string, string> = {
  DEFAULT: `id: DEFAULT
version: 2
initial_step_id: STEP_WAIT_UNLOCK
steps:
  - id: STEP_WAIT_UNLOCK
    screen_scene_id: SCENE_LOCKED
  - id: STEP_U_SON_PROTO
    screen_scene_id: SCENE_BROKEN
    audio_pack_id: PACK_BOOT_RADIO
`,
  EXAMPLE_UNLOCK_EXPRESS: `id: EXAMPLE_UNLOCK_EXPRESS
version: 2
initial_step_id: STEP_WAIT_UNLOCK
steps:
  - id: STEP_WAIT_UNLOCK
    screen_scene_id: SCENE_LOCKED
  - id: STEP_WIN
    screen_scene_id: SCENE_REWARD
    audio_pack_id: PACK_WIN
`,
  EXEMPLE_UNLOCK_EXPRESS_DONE: `id: EXEMPLE_UNLOCK_EXPRESS_DONE
version: 2
initial_step_id: STEP_WAIT_UNLOCK
steps:
  - id: STEP_WAIT_UNLOCK
    screen_scene_id: SCENE_LOCKED
  - id: STEP_WIN
    screen_scene_id: SCENE_REWARD
    audio_pack_id: PACK_WIN
`,
  SPECTRE_RADIO_LAB: `id: SPECTRE_RADIO_LAB
version: 2
initial_step_id: STEP_WAIT_UNLOCK
steps:
  - id: STEP_WAIT_UNLOCK
    screen_scene_id: SCENE_LOCKED
  - id: STEP_SONAR_SEARCH
    screen_scene_id: SCENE_SEARCH
  - id: STEP_MORSE_CLUE
    screen_scene_id: SCENE_BROKEN
`,
  ZACUS_V1_UNLOCK_ETAPE2: `id: ZACUS_V1_UNLOCK_ETAPE2
version: 2
initial_step_id: STEP_BOOT_WAIT
steps:
  - id: STEP_BOOT_WAIT
    screen_scene_id: SCENE_LOCKED
  - id: STEP_BOOT_USON
    screen_scene_id: SCENE_LOCKED
    audio_pack_id: PACK_BOOT_RADIO
`,
}

const NODAL_STYLE_LABELS: Record<NodalStyle, string> = {
  linear: 'Linear chain',
  fork_merge: 'Fork and merge',
  hub: 'Hub and spokes',
}

const SCENE_ROTATION = ['SCENE_LOCKED', 'SCENE_SEARCH', 'SCENE_BROKEN', 'SCENE_REWARD', 'SCENE_READY']
const AUDIO_ROTATION = ['PACK_BOOT_RADIO', 'PACK_WIN']

const clampNodeCount = (value: number) => Math.min(10, Math.max(3, Number.isFinite(value) ? value : 5))

const sanitizeScenarioId = (value: string) => {
  const normalized = value.trim().toUpperCase().replace(/[^A-Z0-9_]/g, '_')
  return normalized.length > 0 ? normalized : 'NODAL_STORY'
}

const renderNodalYaml = (scenarioId: string, initialStepId: string, steps: NodalStep[]) => {
  const lines: string[] = [`id: ${scenarioId}`, 'version: 2', `initial_step_id: ${initialStepId}`, 'steps:']

  for (const step of steps) {
    lines.push(`  - id: ${step.id}`)
    lines.push(`    screen_scene_id: ${step.screen}`)
    if (step.audio) {
      lines.push(`    audio_pack_id: ${step.audio}`)
    }
    if (step.transitions.length > 0) {
      lines.push('    transitions:')
      for (const transition of step.transitions) {
        lines.push(`      - event: ${transition.event}`)
        lines.push(`        target: ${transition.target}`)
      }
    }
  }

  return `${lines.join('\n')}\n`
}

const makeLinearGraph = (nodeCount: number) => {
  const steps: NodalStep[] = []
  for (let index = 0; index < nodeCount; index += 1) {
    const nodeId = index === 0 ? 'STEP_START' : `STEP_NODE_${index + 1}`
    const nextNode = index === nodeCount - 1 ? 'STEP_DONE' : `STEP_NODE_${index + 2}`
    const transitions: NodalTransition[] = []
    if (index === 0) {
      transitions.push({ event: 'UNLOCK', target: nextNode })
    }
    transitions.push({ event: 'BTN_NEXT', target: nextNode })
    transitions.push({ event: 'FORCE_DONE', target: 'STEP_DONE' })
    const audio = index % 2 === 1 ? AUDIO_ROTATION[index % AUDIO_ROTATION.length] : undefined

    steps.push({
      id: nodeId,
      screen: SCENE_ROTATION[index % SCENE_ROTATION.length],
      audio,
      transitions,
    })
  }

  steps.push({ id: 'STEP_DONE', screen: 'SCENE_READY', transitions: [] })
  return { initialStepId: 'STEP_START', steps }
}

const makeForkMergeGraph = (nodeCount: number) => {
  const branchDepth = Math.max(1, Math.min(3, Math.floor((nodeCount - 2) / 2)))
  const steps: NodalStep[] = [
    {
      id: 'STEP_START',
      screen: 'SCENE_LOCKED',
      transitions: [
        { event: 'UNLOCK', target: 'STEP_A_1' },
        { event: 'BTN_NEXT', target: 'STEP_B_1' },
      ],
    },
  ]

  for (let depth = 1; depth <= branchDepth; depth += 1) {
    const nextA = depth === branchDepth ? 'STEP_MERGE' : `STEP_A_${depth + 1}`
    const nextB = depth === branchDepth ? 'STEP_MERGE' : `STEP_B_${depth + 1}`
    steps.push({
      id: `STEP_A_${depth}`,
      screen: SCENE_ROTATION[(depth + 1) % SCENE_ROTATION.length],
      audio: depth === branchDepth ? AUDIO_ROTATION[0] : undefined,
      transitions: [{ event: 'BTN_NEXT', target: nextA }],
    })
    steps.push({
      id: `STEP_B_${depth}`,
      screen: SCENE_ROTATION[(depth + 2) % SCENE_ROTATION.length],
      audio: depth === branchDepth ? AUDIO_ROTATION[1] : undefined,
      transitions: [{ event: 'BTN_NEXT', target: nextB }],
    })
  }

  steps.push({
    id: 'STEP_MERGE',
    screen: 'SCENE_REWARD',
    transitions: [
      { event: 'AUDIO_DONE', target: 'STEP_DONE' },
      { event: 'BTN_NEXT', target: 'STEP_DONE' },
    ],
  })
  steps.push({ id: 'STEP_DONE', screen: 'SCENE_READY', transitions: [] })

  return { initialStepId: 'STEP_START', steps }
}

const makeHubGraph = (nodeCount: number) => {
  const branchCount = Math.max(3, Math.min(5, nodeCount - 1))
  const eventNames = ['UNLOCK', 'BTN_NEXT', 'FORCE_ETAPE2', 'PATH_4', 'PATH_5']
  const steps: NodalStep[] = []
  const startTransitions: NodalTransition[] = []

  for (let index = 0; index < branchCount; index += 1) {
    const branchId = `STEP_PATH_${index + 1}`
    startTransitions.push({
      event: eventNames[index] ?? `PATH_${index + 1}`,
      target: branchId,
    })
    steps.push({
      id: branchId,
      screen: SCENE_ROTATION[(index + 1) % SCENE_ROTATION.length],
      audio: index % 2 === 0 ? AUDIO_ROTATION[index % AUDIO_ROTATION.length] : undefined,
      transitions: [{ event: 'BTN_NEXT', target: 'STEP_HUB_MERGE' }],
    })
  }

  steps.unshift({ id: 'STEP_START', screen: 'SCENE_LOCKED', transitions: startTransitions })
  steps.push({
    id: 'STEP_HUB_MERGE',
    screen: 'SCENE_REWARD',
    transitions: [
      { event: 'AUDIO_DONE', target: 'STEP_DONE' },
      { event: 'BTN_NEXT', target: 'STEP_DONE' },
    ],
  })
  steps.push({ id: 'STEP_DONE', screen: 'SCENE_READY', transitions: [] })

  return { initialStepId: 'STEP_START', steps }
}

const buildNodalDraft = (scenarioId: string, style: NodalStyle, nodeCount: number) => {
  const safeId = sanitizeScenarioId(scenarioId)
  const safeNodeCount = clampNodeCount(nodeCount)
  const graph =
    style === 'linear'
      ? makeLinearGraph(safeNodeCount)
      : style === 'fork_merge'
        ? makeForkMergeGraph(safeNodeCount)
        : makeHubGraph(safeNodeCount)
  return renderNodalYaml(safeId, graph.initialStepId, graph.steps)
}

const StoryDesigner = ({ onValidate, onDeploy, onTestRun, capabilities }: StoryDesignerProps) => {
  const [draft, setDraft] = useState<string>(() => localStorage.getItem('story-draft') ?? TEMPLATE_LIBRARY.DEFAULT)
  const [status, setStatus] = useState('')
  const [errors, setErrors] = useState<string[]>([])
  const [busy, setBusy] = useState(false)
  const [selectedTemplate, setSelectedTemplate] = useState('')
  const [nodalScenarioId, setNodalScenarioId] = useState('NODAL_STORY')
  const [nodalStyle, setNodalStyle] = useState<NodalStyle>('fork_merge')
  const [nodalNodeCount, setNodalNodeCount] = useState(5)

  const validateEnabled = capabilities.canValidate
  const deployEnabled = capabilities.canDeploy
  const testRunEnabled = capabilities.canDeploy && capabilities.canSelectScenario && capabilities.canStart

  useEffect(() => {
    const timer = window.setTimeout(() => {
      localStorage.setItem('story-draft', draft)
    }, 500)

    return () => window.clearTimeout(timer)
  }, [draft])

  useEffect(() => {
    const handleBeforeUnload = (event: BeforeUnloadEvent) => {
      if (draft.trim().length === 0) {
        return
      }
      event.preventDefault()
      event.returnValue = ''
    }

    window.addEventListener('beforeunload', handleBeforeUnload)
    return () => window.removeEventListener('beforeunload', handleBeforeUnload)
  }, [draft])

  const handleTemplateChange = (value: string) => {
    setSelectedTemplate(value)
    if (value && TEMPLATE_LIBRARY[value]) {
      setDraft(TEMPLATE_LIBRARY[value])
      setStatus('Template loaded. Review and adjust resources before deploy.')
      setErrors([])
    }
  }

  const handleGenerateNodalDraft = () => {
    const generated = buildNodalDraft(nodalScenarioId, nodalStyle, nodalNodeCount)
    setDraft(generated)
    setStatus(`Nodal draft generated (${NODAL_STYLE_LABELS[nodalStyle]}, ${clampNodeCount(nodalNodeCount)} nodes).`)
    setErrors([])
  }

  const handleValidate = async () => {
    if (!validateEnabled) {
      setStatus('Validation is unavailable in legacy mode.')
      return
    }

    setBusy(true)
    setStatus('')
    setErrors([])
    try {
      const result = await onValidate(draft)
      if (result.valid) {
        setStatus('Validation passed.')
      } else {
        setStatus('Validation errors found')
        setErrors(result.errors ?? ['Invalid YAML'])
      }
    } catch (err) {
      setStatus(err instanceof Error ? err.message : 'Validation failed')
    } finally {
      setBusy(false)
    }
  }

  const handleDeploy = async () => {
    if (!deployEnabled) {
      setStatus('Deployment is unavailable in legacy mode.')
      return
    }

    setBusy(true)
    setStatus('')
    setErrors([])
    try {
      const result = await onDeploy(draft)
      if (result.status === 'ok') {
        setStatus(`Scenario deployed successfully ${result.deployed ? `(${result.deployed})` : ''}.`)
      } else {
        setStatus(result.message ?? 'Deployment failed')
      }
    } catch (err) {
      setStatus(err instanceof Error ? err.message : 'Deployment failed')
    } finally {
      setBusy(false)
    }
  }

  const handleTestRun = async () => {
    if (!testRunEnabled) {
      setStatus('Test run requires Story V2 select/start/deploy APIs.')
      return
    }

    setBusy(true)
    setStatus('')
    setErrors([])
    try {
      await onTestRun(draft)
      setStatus('Test run started. Returning to selector after 30 seconds.')
    } catch (err) {
      setStatus(err instanceof Error ? err.message : 'Test run failed')
    } finally {
      setBusy(false)
    }
  }

  const editorExtensions = useMemo(() => [yaml()], [])

  return (
    <section className="space-y-6">
      <div>
        <h2 className="text-2xl font-semibold">Story Designer</h2>
        <p className="text-sm text-[var(--ink-500)]">Draft YAML scenarios and deploy them to the device.</p>
      </div>

      {(!validateEnabled || !deployEnabled) && (
        <div className="glass-panel rounded-2xl border border-[var(--ink-500)] p-4 text-sm text-[var(--ink-700)]">
          Story Designer is in read/edit mode. Validate/deploy actions require Story V2 API support.
        </div>
      )}

      <div className="grid gap-6 lg:grid-cols-[1.1fr_0.9fr]">
        <div className="glass-panel rounded-3xl p-4">
          <CodeMirror
            value={draft}
            height="60vh"
            extensions={editorExtensions}
            onChange={setDraft}
            basicSetup={{ lineNumbers: true, foldGutter: false }}
          />
        </div>
        <div className="glass-panel flex flex-col gap-4 rounded-3xl p-6">
          <div className="space-y-2">
            <label className="text-xs uppercase tracking-[0.2em] text-[var(--ink-500)]" htmlFor="template">
              Load template
            </label>
            <select
              id="template"
              value={selectedTemplate}
              onChange={(event) => handleTemplateChange(event.target.value)}
              className="focus-ring min-h-[44px] rounded-xl border border-[var(--ink-500)] bg-white/70 px-3 text-sm"
            >
              <option value="">Select a template</option>
              {Object.keys(TEMPLATE_LIBRARY).map((key) => (
                <option key={key} value={key}>
                  {key}
                </option>
              ))}
            </select>
            <p className="text-xs text-[var(--ink-500)]">
              Templates use real scenario IDs and provide an editable starting point.
            </p>
          </div>

          <div className="space-y-3 border-t border-white/60 pt-4">
            <p className="text-xs uppercase tracking-[0.2em] text-[var(--ink-500)]">Nodal generator</p>
            <label className="block text-xs text-[var(--ink-500)]" htmlFor="nodal-scenario-id">
              Scenario ID
            </label>
            <input
              id="nodal-scenario-id"
              value={nodalScenarioId}
              onChange={(event) => setNodalScenarioId(event.target.value)}
              className="focus-ring min-h-[44px] w-full rounded-xl border border-[var(--ink-500)] bg-white/70 px-3 text-sm"
            />
            <label className="block text-xs text-[var(--ink-500)]" htmlFor="nodal-style">
              Graph style
            </label>
            <select
              id="nodal-style"
              value={nodalStyle}
              onChange={(event) => setNodalStyle(event.target.value as NodalStyle)}
              className="focus-ring min-h-[44px] w-full rounded-xl border border-[var(--ink-500)] bg-white/70 px-3 text-sm"
            >
              {(Object.keys(NODAL_STYLE_LABELS) as NodalStyle[]).map((style) => (
                <option key={style} value={style}>
                  {NODAL_STYLE_LABELS[style]}
                </option>
              ))}
            </select>
            <label className="block text-xs text-[var(--ink-500)]" htmlFor="nodal-node-count">
              Approx node count
            </label>
            <input
              id="nodal-node-count"
              type="number"
              min={3}
              max={10}
              value={nodalNodeCount}
              onChange={(event) => setNodalNodeCount(clampNodeCount(Number.parseInt(event.target.value, 10)))}
              className="focus-ring min-h-[44px] w-full rounded-xl border border-[var(--ink-500)] bg-white/70 px-3 text-sm"
            />
            <button
              type="button"
              onClick={handleGenerateNodalDraft}
              disabled={busy}
              className="focus-ring min-h-[44px] w-full rounded-full border border-[var(--ink-700)] px-4 text-sm font-semibold text-[var(--ink-700)] disabled:opacity-70"
            >
              Generate nodal draft
            </button>
            <p className="text-xs text-[var(--ink-500)]">
              Creates a node-and-transition skeleton directly in YAML so you can iterate visually by flow.
            </p>
          </div>

          <div className="grid gap-3 sm:grid-cols-2">
            <button
              type="button"
              onClick={handleValidate}
              disabled={busy || !validateEnabled}
              className="focus-ring min-h-[44px] rounded-full border border-[var(--ink-700)] px-4 text-sm font-semibold text-[var(--ink-700)] disabled:opacity-70"
            >
              Validate
            </button>
            <button
              type="button"
              onClick={handleDeploy}
              disabled={busy || !deployEnabled}
              className="focus-ring min-h-[44px] rounded-full bg-[var(--accent-500)] px-4 text-sm font-semibold text-white disabled:opacity-70"
            >
              Deploy
            </button>
            <button
              type="button"
              onClick={handleTestRun}
              disabled={busy || !testRunEnabled}
              className="focus-ring min-h-[44px] rounded-full border border-[var(--ink-500)] px-4 text-sm font-semibold text-[var(--ink-500)] disabled:opacity-70 sm:col-span-2"
            >
              Test Run (30 sec)
            </button>
          </div>

          {status && (
            <div className="rounded-2xl border border-white/60 bg-white/70 p-4 text-sm text-[var(--ink-700)]">
              {status}
            </div>
          )}

          {errors.length > 0 && (
            <div className="rounded-2xl border border-[var(--accent-700)] bg-white/70 p-4 text-xs text-[var(--accent-700)]">
              {errors.map((error) => (
                <p key={error}>{error}</p>
              ))}
            </div>
          )}
        </div>
      </div>
    </section>
  )
}

export default StoryDesigner
