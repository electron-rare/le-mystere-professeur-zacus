import { useCallback, useEffect, useMemo, useRef, useState, type MouseEvent as ReactMouseEvent } from 'react'
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

type EditorNode = {
  id: string
  stepId: string
  screenSceneId: string
  audioPackId: string
  x: number
  y: number
  isInitial: boolean
}

type EditorEdge = {
  id: string
  fromNodeId: string
  toNodeId: string
  eventName: string
}

type DragState = {
  nodeId: string
  offsetX: number
  offsetY: number
}

const NODE_WIDTH = 250
const NODE_HEIGHT = 260
const CANVAS_HEIGHT = 560

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

const SCENE_ROTATION = ['SCENE_LOCKED', 'SCENE_SEARCH', 'SCENE_BROKEN', 'SCENE_REWARD', 'SCENE_READY']

const makeIdFragment = () => Math.random().toString(36).slice(2, 8)

const normalizeToken = (value: string, fallback: string) => {
  const normalized = value.trim().toUpperCase().replace(/[^A-Z0-9_]/g, '_')
  return normalized.length > 0 ? normalized : fallback
}

const ensureInitialNode = (nodes: EditorNode[]) => {
  if (nodes.length === 0) {
    return nodes
  }
  if (nodes.some((node) => node.isInitial)) {
    return nodes
  }
  return nodes.map((node, index) => ({ ...node, isInitial: index === 0 }))
}

const createNode = (index: number): EditorNode => ({
  id: `node-${makeIdFragment()}`,
  stepId: `STEP_NODE_${index}`,
  screenSceneId: SCENE_ROTATION[(index - 1) % SCENE_ROTATION.length],
  audioPackId: '',
  x: 28 + ((index - 1) % 3) * 270,
  y: 36 + Math.floor((index - 1) / 3) * 250,
  isInitial: false,
})

const createDefaultGraph = () => {
  const start: EditorNode = {
    id: 'node-start',
    stepId: 'STEP_START',
    screenSceneId: 'SCENE_LOCKED',
    audioPackId: '',
    x: 32,
    y: 90,
    isInitial: true,
  }
  const middle: EditorNode = {
    id: 'node-investigate',
    stepId: 'STEP_INVESTIGATION',
    screenSceneId: 'SCENE_SEARCH',
    audioPackId: 'PACK_BOOT_RADIO',
    x: 350,
    y: 280,
    isInitial: false,
  }
  const done: EditorNode = {
    id: 'node-done',
    stepId: 'STEP_DONE',
    screenSceneId: 'SCENE_READY',
    audioPackId: '',
    x: 680,
    y: 90,
    isInitial: false,
  }

  const edges: EditorEdge[] = [
    { id: 'edge-start-mid', fromNodeId: start.id, toNodeId: middle.id, eventName: 'UNLOCK' },
    { id: 'edge-mid-done', fromNodeId: middle.id, toNodeId: done.id, eventName: 'BTN_NEXT' },
  ]

  return { nodes: [start, middle, done], edges }
}

const buildStoryYaml = (scenarioId: string, nodes: EditorNode[], edges: EditorEdge[]) => {
  const normalizedNodes = ensureInitialNode(nodes)
  if (normalizedNodes.length === 0) {
    return `id: ${normalizeToken(scenarioId, 'NODAL_STORY')}
version: 2
initial_step_id: STEP_START
steps: []
`
  }

  const usedStepIds = new Set<string>()
  const stepIdByNodeId = new Map<string, string>()
  normalizedNodes.forEach((node, index) => {
    const baseId = normalizeToken(node.stepId, `STEP_NODE_${index + 1}`)
    let candidate = baseId
    let suffix = 2
    while (usedStepIds.has(candidate)) {
      candidate = `${baseId}_${suffix}`
      suffix += 1
    }
    usedStepIds.add(candidate)
    stepIdByNodeId.set(node.id, candidate)
  })

  const initialNode = normalizedNodes.find((node) => node.isInitial) ?? normalizedNodes[0]
  const initialStepId = stepIdByNodeId.get(initialNode.id) ?? 'STEP_START'
  const lines: string[] = [
    `id: ${normalizeToken(scenarioId, 'NODAL_STORY')}`,
    'version: 2',
    `initial_step_id: ${initialStepId}`,
    'steps:',
  ]

  normalizedNodes.forEach((node, index) => {
    const stepId = stepIdByNodeId.get(node.id) ?? `STEP_NODE_${index + 1}`
    const screenSceneId = normalizeToken(node.screenSceneId, 'SCENE_LOCKED')
    const audioPackId = node.audioPackId.trim().length > 0 ? normalizeToken(node.audioPackId, '') : ''
    const outgoingEdges = edges.filter((edge) => edge.fromNodeId === node.id && stepIdByNodeId.has(edge.toNodeId))

    lines.push(`  - id: ${stepId}`)
    lines.push(`    screen_scene_id: ${screenSceneId}`)
    if (audioPackId) {
      lines.push(`    audio_pack_id: ${audioPackId}`)
    }
    if (outgoingEdges.length > 0) {
      lines.push('    transitions:')
      outgoingEdges.forEach((edge) => {
        const targetStepId = stepIdByNodeId.get(edge.toNodeId)
        if (!targetStepId) {
          return
        }
        lines.push(`      - event: ${normalizeToken(edge.eventName, 'BTN_NEXT')}`)
        lines.push(`        target: ${targetStepId}`)
      })
    }
  })

  return `${lines.join('\n')}\n`
}

const StoryDesigner = ({ onValidate, onDeploy, onTestRun, capabilities }: StoryDesignerProps) => {
  const [draft, setDraft] = useState<string>(() => localStorage.getItem('story-draft') ?? TEMPLATE_LIBRARY.DEFAULT)
  const [status, setStatus] = useState('')
  const [errors, setErrors] = useState<string[]>([])
  const [busy, setBusy] = useState(false)
  const [selectedTemplate, setSelectedTemplate] = useState('')
  const [graphScenarioId, setGraphScenarioId] = useState('NODAL_STORY')
  const [nodes, setNodes] = useState<EditorNode[]>(() => createDefaultGraph().nodes)
  const [edges, setEdges] = useState<EditorEdge[]>(() => createDefaultGraph().edges)
  const [linkSourceId, setLinkSourceId] = useState<string | null>(null)
  const [dragState, setDragState] = useState<DragState | null>(null)
  const canvasRef = useRef<HTMLDivElement | null>(null)

  const validateEnabled = capabilities.canValidate
  const deployEnabled = capabilities.canDeploy
  const testRunEnabled = capabilities.canDeploy && capabilities.canSelectScenario && capabilities.canStart

  const nodeMap = useMemo(() => new Map(nodes.map((node) => [node.id, node])), [nodes])

  const outgoingEdgeMap = useMemo(() => {
    const map = new Map<string, EditorEdge[]>()
    edges.forEach((edge) => {
      const current = map.get(edge.fromNodeId) ?? []
      current.push(edge)
      map.set(edge.fromNodeId, current)
    })
    return map
  }, [edges])

  const renderedEdges = useMemo(
    () =>
      edges
        .map((edge) => {
          const from = nodeMap.get(edge.fromNodeId)
          const to = nodeMap.get(edge.toNodeId)
          if (!from || !to) {
            return null
          }
          const x1 = from.x + NODE_WIDTH
          const y1 = from.y + 54
          const x2 = to.x
          const y2 = to.y + 54
          const controlGap = Math.max(Math.abs(x2 - x1) * 0.4, 60)
          const path = `M ${x1} ${y1} C ${x1 + controlGap} ${y1}, ${x2 - controlGap} ${y2}, ${x2} ${y2}`
          return {
            id: edge.id,
            path,
            labelX: (x1 + x2) / 2,
            labelY: (y1 + y2) / 2 - 6,
            eventName: edge.eventName,
          }
        })
        .filter((edge): edge is { id: string; path: string; labelX: number; labelY: number; eventName: string } => edge !== null),
    [edges, nodeMap],
  )

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

  useEffect(() => {
    const clearDrag = () => setDragState(null)
    window.addEventListener('mouseup', clearDrag)
    return () => window.removeEventListener('mouseup', clearDrag)
  }, [])

  const updateNode = useCallback((nodeId: string, patch: Partial<EditorNode>) => {
    setNodes((previous) =>
      previous.map((node) => {
        if (node.id !== nodeId) {
          return node
        }
        return { ...node, ...patch }
      }),
    )
  }, [])

  const setInitialNode = useCallback((nodeId: string) => {
    setNodes((previous) =>
      previous.map((node) => ({
        ...node,
        isInitial: node.id === nodeId,
      })),
    )
  }, [])

  const handleTemplateChange = (value: string) => {
    setSelectedTemplate(value)
    if (value && TEMPLATE_LIBRARY[value]) {
      setDraft(TEMPLATE_LIBRARY[value])
      setStatus('Template loaded. Review and adjust resources before deploy.')
      setErrors([])
    }
  }

  const handleAddNode = useCallback(() => {
    setNodes((previous) => {
      const nextNode = createNode(previous.length + 1)
      if (previous.length === 0) {
        nextNode.isInitial = true
      }
      return [...previous, nextNode]
    })
  }, [])

  const handleResetGraph = useCallback(() => {
    const graph = createDefaultGraph()
    setNodes(graph.nodes)
    setEdges(graph.edges)
    setLinkSourceId(null)
    setStatus('Node graph reset to default.')
    setErrors([])
  }, [])

  const handleRemoveNode = useCallback((nodeId: string) => {
    setNodes((previous) => ensureInitialNode(previous.filter((node) => node.id !== nodeId)))
    setEdges((previous) => previous.filter((edge) => edge.fromNodeId !== nodeId && edge.toNodeId !== nodeId))
    setLinkSourceId((previous) => (previous === nodeId ? null : previous))
  }, [])

  const handleStartLink = useCallback((nodeId: string) => {
    setLinkSourceId(nodeId)
    const nodeLabel = nodeMap.get(nodeId)?.stepId ?? nodeId
    setStatus(`Link mode: select target node for ${nodeLabel}.`)
  }, [nodeMap])

  const handleNodeClick = useCallback(
    (nodeId: string) => {
      if (!linkSourceId || linkSourceId === nodeId) {
        return
      }
      const eventName = window.prompt('Transition event name', 'BTN_NEXT')
      if (eventName === null) {
        setStatus('Link creation cancelled.')
        setLinkSourceId(null)
        return
      }
      const trimmedEvent = eventName.trim()
      if (!trimmedEvent) {
        setStatus('Link event cannot be empty.')
        return
      }

      setEdges((previous) => {
        const existing = previous.find(
          (edge) => edge.fromNodeId === linkSourceId && edge.toNodeId === nodeId,
        )
        if (existing) {
          return previous.map((edge) =>
            edge.id === existing.id
              ? {
                  ...edge,
                  eventName: trimmedEvent,
                }
              : edge,
          )
        }
        return [
          ...previous,
          {
            id: `edge-${makeIdFragment()}`,
            fromNodeId: linkSourceId,
            toNodeId: nodeId,
            eventName: trimmedEvent,
          },
        ]
      })
      setLinkSourceId(null)
      setStatus('Nodes linked. Edit event directly inside the source node if needed.')
      setErrors([])
    },
    [linkSourceId],
  )

  const handleRemoveEdge = useCallback((edgeId: string) => {
    setEdges((previous) => previous.filter((edge) => edge.id !== edgeId))
  }, [])

  const handleDragStart = useCallback(
    (event: ReactMouseEvent<HTMLDivElement>, nodeId: string) => {
      if (!canvasRef.current) {
        return
      }
      const node = nodeMap.get(nodeId)
      if (!node) {
        return
      }
      const bounds = canvasRef.current.getBoundingClientRect()
      setDragState({
        nodeId,
        offsetX: event.clientX - bounds.left - node.x,
        offsetY: event.clientY - bounds.top - node.y,
      })
      event.preventDefault()
      event.stopPropagation()
    },
    [nodeMap],
  )

  const handleCanvasMouseMove = useCallback(
    (event: ReactMouseEvent<HTMLDivElement>) => {
      if (!dragState || !canvasRef.current) {
        return
      }
      const bounds = canvasRef.current.getBoundingClientRect()
      const maxX = Math.max(20, bounds.width - NODE_WIDTH - 10)
      const maxY = Math.max(20, CANVAS_HEIGHT - NODE_HEIGHT - 10)
      const nextX = Math.min(maxX, Math.max(10, event.clientX - bounds.left - dragState.offsetX))
      const nextY = Math.min(maxY, Math.max(10, event.clientY - bounds.top - dragState.offsetY))
      updateNode(dragState.nodeId, { x: nextX, y: nextY })
    },
    [dragState, updateNode],
  )

  const handleGenerateFromNodes = useCallback(() => {
    const generated = buildStoryYaml(graphScenarioId, nodes, edges)
    setDraft(generated)
    setStatus('YAML generated from linked nodes.')
    setErrors([])
  }, [edges, graphScenarioId, nodes])

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
        <p className="text-sm text-[var(--ink-500)]">
          Build your story as linked nodes, set parameters in each node, then generate YAML.
        </p>
      </div>

      {(!validateEnabled || !deployEnabled) && (
        <div className="glass-panel rounded-2xl border border-[var(--ink-500)] p-4 text-sm text-[var(--ink-700)]">
          Story Designer is in read/edit mode. Validate/deploy actions require Story V2 API support.
        </div>
      )}

      <div className="glass-panel rounded-3xl p-5">
        <div className="grid gap-3 md:grid-cols-[minmax(0,1fr)_auto_auto_auto] md:items-end">
          <label className="text-xs uppercase tracking-[0.2em] text-[var(--ink-500)]" htmlFor="graph-scenario-id">
            Scenario ID
            <input
              id="graph-scenario-id"
              value={graphScenarioId}
              onChange={(event) => setGraphScenarioId(event.target.value)}
              className="focus-ring mt-2 min-h-[44px] w-full rounded-xl border border-[var(--ink-500)] bg-white/70 px-3 text-sm text-[var(--ink-900)]"
            />
          </label>
          <button
            type="button"
            onClick={handleAddNode}
            className="focus-ring min-h-[44px] rounded-full border border-[var(--ink-700)] px-4 text-sm font-semibold text-[var(--ink-700)]"
          >
            Add node
          </button>
          <button
            type="button"
            onClick={handleGenerateFromNodes}
            className="focus-ring min-h-[44px] rounded-full bg-[var(--ink-700)] px-4 text-sm font-semibold text-white"
          >
            Generate YAML
          </button>
          <button
            type="button"
            onClick={handleResetGraph}
            className="focus-ring min-h-[44px] rounded-full border border-[var(--ink-500)] px-4 text-sm font-semibold text-[var(--ink-500)]"
          >
            Reset graph
          </button>
        </div>
        <p className="mt-3 text-xs text-[var(--ink-500)]">
          Click <span className="font-semibold">Link</span> on a source node, then click a target node to connect them.
          Node parameters and transition events are edited directly inside each node.
        </p>
        {linkSourceId && (
          <p className="mt-2 rounded-xl border border-[var(--accent-700)] bg-white/70 px-3 py-2 text-xs text-[var(--accent-700)]">
            Link mode active from <span className="font-semibold">{nodeMap.get(linkSourceId)?.stepId ?? linkSourceId}</span>. Click a target node.
          </p>
        )}

        <div
          ref={canvasRef}
          className="relative mt-4 overflow-auto rounded-2xl border border-white/60 bg-white/40"
          style={{ height: `${CANVAS_HEIGHT}px` }}
          onMouseMove={handleCanvasMouseMove}
          onMouseUp={() => setDragState(null)}
          onMouseLeave={() => setDragState(null)}
        >
          <svg className="pointer-events-none absolute inset-0 h-full w-full">
            {renderedEdges.map((edge) => (
              <g key={edge.id}>
                <path d={edge.path} fill="none" stroke="rgba(31,42,68,0.55)" strokeWidth={2.5} />
                <text x={edge.labelX} y={edge.labelY} textAnchor="middle" fontSize={11} fill="#1f2a44">
                  {edge.eventName}
                </text>
              </g>
            ))}
          </svg>

          {nodes.map((node) => {
            const outgoing = outgoingEdgeMap.get(node.id) ?? []
            return (
              <div
                key={node.id}
                className={`absolute rounded-2xl border bg-white/90 p-3 shadow-lg ${
                  linkSourceId === node.id ? 'border-[var(--accent-700)]' : 'border-[var(--mist-400)]'
                }`}
                style={{ left: `${node.x}px`, top: `${node.y}px`, width: `${NODE_WIDTH}px`, minHeight: `${NODE_HEIGHT}px` }}
                onClick={() => handleNodeClick(node.id)}
              >
                <div
                  className="mb-2 flex cursor-move items-center justify-between rounded-xl bg-[var(--mist-200)] px-2 py-1 text-xs font-semibold text-[var(--ink-700)]"
                  onMouseDown={(event) => handleDragStart(event, node.id)}
                >
                  <span>{node.stepId || 'STEP_NODE'}</span>
                  <div className="flex gap-1">
                    <button
                      type="button"
                      onClick={(event) => {
                        event.stopPropagation()
                        handleStartLink(node.id)
                      }}
                      className="rounded-md border border-[var(--ink-500)] px-2 py-1 text-[10px]"
                    >
                      Link
                    </button>
                    <button
                      type="button"
                      onClick={(event) => {
                        event.stopPropagation()
                        handleRemoveNode(node.id)
                      }}
                      className="rounded-md border border-[var(--accent-700)] px-2 py-1 text-[10px] text-[var(--accent-700)]"
                    >
                      Del
                    </button>
                  </div>
                </div>

                <label className="block text-[10px] uppercase tracking-[0.15em] text-[var(--ink-500)]">
                  Step ID
                  <input
                    value={node.stepId}
                    onChange={(event) => updateNode(node.id, { stepId: event.target.value })}
                    onClick={(event) => event.stopPropagation()}
                    className="focus-ring mt-1 min-h-[32px] w-full rounded-lg border border-[var(--mist-400)] px-2 text-xs text-[var(--ink-900)]"
                  />
                </label>
                <label className="mt-2 block text-[10px] uppercase tracking-[0.15em] text-[var(--ink-500)]">
                  Screen
                  <input
                    value={node.screenSceneId}
                    onChange={(event) => updateNode(node.id, { screenSceneId: event.target.value })}
                    onClick={(event) => event.stopPropagation()}
                    className="focus-ring mt-1 min-h-[32px] w-full rounded-lg border border-[var(--mist-400)] px-2 text-xs text-[var(--ink-900)]"
                  />
                </label>
                <label className="mt-2 block text-[10px] uppercase tracking-[0.15em] text-[var(--ink-500)]">
                  Audio pack
                  <input
                    value={node.audioPackId}
                    onChange={(event) => updateNode(node.id, { audioPackId: event.target.value })}
                    onClick={(event) => event.stopPropagation()}
                    placeholder="PACK_BOOT_RADIO"
                    className="focus-ring mt-1 min-h-[32px] w-full rounded-lg border border-[var(--mist-400)] px-2 text-xs text-[var(--ink-900)]"
                  />
                </label>
                <button
                  type="button"
                  onClick={(event) => {
                    event.stopPropagation()
                    setInitialNode(node.id)
                  }}
                  className={`mt-2 min-h-[30px] w-full rounded-lg border px-2 text-[10px] uppercase tracking-[0.15em] ${
                    node.isInitial
                      ? 'border-[var(--teal-500)] bg-[var(--teal-500)] text-white'
                      : 'border-[var(--ink-500)] text-[var(--ink-700)]'
                  }`}
                >
                  {node.isInitial ? 'Initial node' : 'Set initial'}
                </button>

                {outgoing.length > 0 && (
                  <div className="mt-2 space-y-1 rounded-lg border border-[var(--mist-400)] p-2">
                    <p className="text-[10px] uppercase tracking-[0.15em] text-[var(--ink-500)]">Transitions</p>
                    {outgoing.map((edge) => (
                      <div key={edge.id} className="space-y-1">
                        <div className="text-[10px] text-[var(--ink-500)]">
                          to {nodeMap.get(edge.toNodeId)?.stepId ?? 'Unknown'}
                        </div>
                        <div className="flex gap-1">
                          <input
                            value={edge.eventName}
                            onClick={(event) => event.stopPropagation()}
                            onChange={(event) =>
                              setEdges((previous) =>
                                previous.map((candidate) =>
                                  candidate.id === edge.id
                                    ? {
                                        ...candidate,
                                        eventName: event.target.value,
                                      }
                                    : candidate,
                                ),
                              )
                            }
                            className="focus-ring min-h-[28px] flex-1 rounded-md border border-[var(--mist-400)] px-2 text-[10px] text-[var(--ink-900)]"
                          />
                          <button
                            type="button"
                            onClick={(event) => {
                              event.stopPropagation()
                              handleRemoveEdge(edge.id)
                            }}
                            className="rounded-md border border-[var(--accent-700)] px-2 text-[10px] text-[var(--accent-700)]"
                          >
                            x
                          </button>
                        </div>
                      </div>
                    ))}
                  </div>
                )}
              </div>
            )
          })}
        </div>
      </div>

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
              You can still switch to template mode and manually adjust YAML.
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

