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
  actionsCsv: string
  appsCsv: string
  mp3GateOpen: boolean
  x: number
  y: number
  isInitial: boolean
}

type EditorEdge = {
  id: string
  fromNodeId: string
  toNodeId: string
  trigger: string
  eventType: string
  eventName: string
  afterMs: number
  priority: number
}

type GraphSnapshot = {
  nodes: EditorNode[]
  edges: EditorEdge[]
  linkSourceId: string | null
  linkEventName: string
  linkEventType: string
}

type DragState = {
  nodeId: string
  offsetX: number
  offsetY: number
}

const NODE_WIDTH = 250
const NODE_HEIGHT = 360
const CANVAS_HEIGHT = 560
const NODE_HORIZONTAL_GAP = 300
const NODE_VERTICAL_GAP = 220

const TEMPLATE_LIBRARY: Record<string, string> = {
  DEFAULT: `id: DEFAULT
version: 2
initial_step: STEP_WAIT_UNLOCK
app_bindings:
  - id: APP_LA
    app: LA_DETECTOR
    config:
      hold_ms: 3000
      unlock_event: UNLOCK
      require_listening: true
  - id: APP_AUDIO
    app: AUDIO_PACK
  - id: APP_SCREEN
    app: SCREEN_SCENE
  - id: APP_GATE
    app: MP3_GATE
steps:
  - step_id: STEP_WAIT_UNLOCK
    screen_scene_id: SCENE_LOCKED
    audio_pack_id: ""
    actions:
      - ACTION_TRACE_STEP
    apps:
      - APP_LA
      - APP_SCREEN
      - APP_GATE
    mp3_gate_open: false
    transitions:
      - trigger: on_event
        event_type: unlock
        event_name: UNLOCK
        target_step_id: STEP_U_SON_PROTO
        after_ms: 0
        priority: 100
  - step_id: STEP_U_SON_PROTO
    screen_scene_id: SCENE_BROKEN
    audio_pack_id: PACK_BOOT_RADIO
    actions:
      - ACTION_TRACE_STEP
    apps:
      - APP_AUDIO
      - APP_SCREEN
      - APP_GATE
    mp3_gate_open: false
    transitions: []
`,
  EXAMPLE_UNLOCK_EXPRESS: `id: EXAMPLE_UNLOCK_EXPRESS
version: 2
initial_step: STEP_WAIT_UNLOCK
app_bindings:
  - id: APP_LA
    app: LA_DETECTOR
    config:
      hold_ms: 3000
      unlock_event: UNLOCK
      require_listening: true
  - id: APP_AUDIO
    app: AUDIO_PACK
  - id: APP_SCREEN
    app: SCREEN_SCENE
  - id: APP_GATE
    app: MP3_GATE
steps:
  - step_id: STEP_WAIT_UNLOCK
    screen_scene_id: SCENE_LOCKED
    audio_pack_id: ""
    actions:
      - ACTION_TRACE_STEP
    apps:
      - APP_LA
      - APP_SCREEN
      - APP_GATE
    mp3_gate_open: false
    transitions:
      - trigger: on_event
        event_type: unlock
        event_name: UNLOCK
        target_step_id: STEP_WIN
        after_ms: 0
        priority: 100
  - step_id: STEP_WIN
    screen_scene_id: SCENE_REWARD
    audio_pack_id: PACK_WIN
    actions:
      - ACTION_TRACE_STEP
    apps:
      - APP_AUDIO
      - APP_SCREEN
      - APP_GATE
    mp3_gate_open: false
    transitions: []
`,
  EXEMPLE_UNLOCK_EXPRESS_DONE: `id: EXEMPLE_UNLOCK_EXPRESS_DONE
version: 2
initial_step: STEP_WAIT_UNLOCK
app_bindings:
  - id: APP_LA
    app: LA_DETECTOR
    config:
      hold_ms: 3000
      unlock_event: UNLOCK
      require_listening: true
  - id: APP_AUDIO
    app: AUDIO_PACK
  - id: APP_SCREEN
    app: SCREEN_SCENE
  - id: APP_GATE
    app: MP3_GATE
steps:
  - step_id: STEP_WAIT_UNLOCK
    screen_scene_id: SCENE_LOCKED
    audio_pack_id: ""
    actions:
      - ACTION_TRACE_STEP
    apps:
      - APP_LA
      - APP_SCREEN
      - APP_GATE
    mp3_gate_open: false
    transitions:
      - trigger: on_event
        event_type: unlock
        event_name: UNLOCK
        target_step_id: STEP_WIN
        after_ms: 0
        priority: 100
  - step_id: STEP_WIN
    screen_scene_id: SCENE_REWARD
    audio_pack_id: PACK_WIN
    actions:
      - ACTION_TRACE_STEP
    apps:
      - APP_AUDIO
      - APP_SCREEN
      - APP_GATE
    mp3_gate_open: false
    transitions: []
`,
  SPECTRE_RADIO_LAB: `id: SPECTRE_RADIO_LAB
version: 2
initial_step: STEP_WAIT_UNLOCK
app_bindings:
  - id: APP_LA
    app: LA_DETECTOR
    config:
      hold_ms: 3000
      unlock_event: UNLOCK
      require_listening: true
  - id: APP_AUDIO
    app: AUDIO_PACK
  - id: APP_SCREEN
    app: SCREEN_SCENE
  - id: APP_GATE
    app: MP3_GATE
steps:
  - step_id: STEP_WAIT_UNLOCK
    screen_scene_id: SCENE_LOCKED
    audio_pack_id: ""
    actions:
      - ACTION_TRACE_STEP
    apps:
      - APP_LA
      - APP_SCREEN
      - APP_GATE
    mp3_gate_open: false
    transitions:
      - trigger: on_event
        event_type: unlock
        event_name: UNLOCK
        target_step_id: STEP_SONAR_SEARCH
        after_ms: 0
        priority: 100
  - step_id: STEP_SONAR_SEARCH
    screen_scene_id: SCENE_SEARCH
    audio_pack_id: ""
    actions:
      - ACTION_TRACE_STEP
    apps:
      - APP_SCREEN
      - APP_GATE
    mp3_gate_open: false
    transitions:
      - trigger: on_event
        event_type: action
        event_name: BTN_NEXT
        target_step_id: STEP_MORSE_CLUE
        after_ms: 0
        priority: 100
  - step_id: STEP_MORSE_CLUE
    screen_scene_id: SCENE_BROKEN
    audio_pack_id: ""
    actions:
      - ACTION_TRACE_STEP
    apps:
      - APP_SCREEN
      - APP_GATE
    mp3_gate_open: false
    transitions: []
`,
  ZACUS_V1_UNLOCK_ETAPE2: `id: ZACUS_V1_UNLOCK_ETAPE2
version: 2
initial_step: STEP_BOOT_WAIT
app_bindings:
  - id: APP_LA
    app: LA_DETECTOR
    config:
      hold_ms: 3000
      unlock_event: UNLOCK
      require_listening: true
  - id: APP_AUDIO
    app: AUDIO_PACK
  - id: APP_SCREEN
    app: SCREEN_SCENE
  - id: APP_GATE
    app: MP3_GATE
steps:
  - step_id: STEP_BOOT_WAIT
    screen_scene_id: SCENE_LOCKED
    audio_pack_id: ""
    actions:
      - ACTION_TRACE_STEP
    apps:
      - APP_LA
      - APP_SCREEN
      - APP_GATE
    mp3_gate_open: false
    transitions:
      - trigger: on_event
        event_type: unlock
        event_name: UNLOCK
        target_step_id: STEP_BOOT_USON
        after_ms: 0
        priority: 100
  - step_id: STEP_BOOT_USON
    screen_scene_id: SCENE_LOCKED
    audio_pack_id: PACK_BOOT_RADIO
    actions:
      - ACTION_TRACE_STEP
    apps:
      - APP_AUDIO
      - APP_SCREEN
      - APP_GATE
    mp3_gate_open: false
    transitions: []
`,
}

const SCENE_ROTATION = ['SCENE_LOCKED', 'SCENE_SEARCH', 'SCENE_BROKEN', 'SCENE_REWARD', 'SCENE_READY']
const DEFAULT_ACTIONS_CSV = 'ACTION_TRACE_STEP'
const DEFAULT_APPS_CSV = 'APP_SCREEN,APP_GATE'
const DEFAULT_EDGE_TRIGGER = 'on_event'
const DEFAULT_EDGE_EVENT_TYPE = 'action'
const DEFAULT_EDGE_AFTER_MS = 0
const DEFAULT_EDGE_PRIORITY = 100

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

const tokenizeCsv = (value: string) =>
  Array.from(
    new Set(
      value
        .split(',')
        .map((token) => normalizeToken(token, ''))
        .filter((token) => token.length > 0),
    ),
  )

const normalizeTrigger = (value: string) => {
  const normalized = value.trim().toLowerCase()
  if (normalized === 'on_event' || normalized === 'after_ms' || normalized === 'immediate') {
    return normalized
  }
  return DEFAULT_EDGE_TRIGGER
}

const inferEventType = (eventName: string) => {
  const normalizedName = normalizeToken(eventName, 'ACTION')
  if (normalizedName === 'UNLOCK') {
    return 'unlock'
  }
  if (normalizedName === 'AUDIO_DONE') {
    return 'audio_done'
  }
  if (normalizedName.startsWith('TIMER')) {
    return 'timer'
  }
  if (normalizedName.startsWith('FORCE') || normalizedName === 'SKIP') {
    return 'serial'
  }
  return DEFAULT_EDGE_EVENT_TYPE
}

const normalizeEventType = (value: string, eventName: string) => {
  const normalized = value.trim().toLowerCase()
  if (normalized === 'none' || normalized === 'unlock' || normalized === 'audio_done' || normalized === 'timer' || normalized === 'serial' || normalized === 'action') {
    return normalized
  }
  return inferEventType(eventName)
}

const cloneNodes = (source: EditorNode[]) => source.map((node) => ({ ...node }))

const cloneEdges = (source: EditorEdge[]) => source.map((edge) => ({ ...edge }))

const createNode = (index: number): EditorNode => ({
  id: `node-${makeIdFragment()}`,
  stepId: `STEP_NODE_${index}`,
  screenSceneId: SCENE_ROTATION[(index - 1) % SCENE_ROTATION.length],
  audioPackId: '',
  actionsCsv: DEFAULT_ACTIONS_CSV,
  appsCsv: DEFAULT_APPS_CSV,
  mp3GateOpen: false,
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
    actionsCsv: DEFAULT_ACTIONS_CSV,
    appsCsv: 'APP_LA,APP_SCREEN,APP_GATE',
    mp3GateOpen: false,
    x: 32,
    y: 90,
    isInitial: true,
  }
  const middle: EditorNode = {
    id: 'node-investigate',
    stepId: 'STEP_INVESTIGATION',
    screenSceneId: 'SCENE_SEARCH',
    audioPackId: 'PACK_BOOT_RADIO',
    actionsCsv: DEFAULT_ACTIONS_CSV,
    appsCsv: 'APP_AUDIO,APP_SCREEN,APP_GATE',
    mp3GateOpen: false,
    x: 350,
    y: 280,
    isInitial: false,
  }
  const done: EditorNode = {
    id: 'node-done',
    stepId: 'STEP_DONE',
    screenSceneId: 'SCENE_READY',
    audioPackId: '',
    actionsCsv: `${DEFAULT_ACTIONS_CSV},ACTION_REFRESH_SD`,
    appsCsv: 'APP_SCREEN,APP_GATE',
    mp3GateOpen: true,
    x: 680,
    y: 90,
    isInitial: false,
  }

  const edges: EditorEdge[] = [
    {
      id: 'edge-start-mid',
      fromNodeId: start.id,
      toNodeId: middle.id,
      trigger: 'on_event',
      eventType: 'unlock',
      eventName: 'UNLOCK',
      afterMs: 0,
      priority: 100,
    },
    {
      id: 'edge-mid-done',
      fromNodeId: middle.id,
      toNodeId: done.id,
      trigger: 'on_event',
      eventType: 'action',
      eventName: 'BTN_NEXT',
      afterMs: 0,
      priority: 100,
    },
  ]

  return { nodes: [start, middle, done], edges }
}

const buildStoryYaml = (scenarioId: string, nodes: EditorNode[], edges: EditorEdge[]) => {
  const normalizedNodes = ensureInitialNode(nodes)
  if (normalizedNodes.length === 0) {
    return `id: ${normalizeToken(scenarioId, 'NODAL_STORY')}
version: 2
initial_step: STEP_START
app_bindings:
  - id: APP_LA
    app: LA_DETECTOR
    config:
      hold_ms: 3000
      unlock_event: UNLOCK
      require_listening: true
  - id: APP_AUDIO
    app: AUDIO_PACK
  - id: APP_SCREEN
    app: SCREEN_SCENE
  - id: APP_GATE
    app: MP3_GATE
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
    `initial_step: ${initialStepId}`,
    'app_bindings:',
    '  - id: APP_LA',
    '    app: LA_DETECTOR',
    '    config:',
    '      hold_ms: 3000',
    '      unlock_event: UNLOCK',
    '      require_listening: true',
    '  - id: APP_AUDIO',
    '    app: AUDIO_PACK',
    '  - id: APP_SCREEN',
    '    app: SCREEN_SCENE',
    '  - id: APP_GATE',
    '    app: MP3_GATE',
    'steps:',
  ]

  normalizedNodes.forEach((node, index) => {
    const stepId = stepIdByNodeId.get(node.id) ?? `STEP_NODE_${index + 1}`
    const screenSceneId = normalizeToken(node.screenSceneId, 'SCENE_LOCKED')
    const audioPackId = node.audioPackId.trim().length > 0 ? normalizeToken(node.audioPackId, '') : '""'
    const actions = tokenizeCsv(node.actionsCsv)
    const apps = tokenizeCsv(node.appsCsv)
    const outgoingEdges = edges.filter((edge) => edge.fromNodeId === node.id && stepIdByNodeId.has(edge.toNodeId))

    lines.push(`  - step_id: ${stepId}`)
    lines.push(`    screen_scene_id: ${screenSceneId}`)
    lines.push(`    audio_pack_id: ${audioPackId}`)
    lines.push('    actions:')
    ;(actions.length > 0 ? actions : [normalizeToken(DEFAULT_ACTIONS_CSV, 'ACTION_TRACE_STEP')]).forEach((action) =>
      lines.push(`      - ${action}`),
    )
    lines.push('    apps:')
    ;(apps.length > 0 ? apps : tokenizeCsv(DEFAULT_APPS_CSV)).forEach((appId) => lines.push(`      - ${appId}`))
    lines.push(`    mp3_gate_open: ${node.mp3GateOpen ? 'true' : 'false'}`)
    if (outgoingEdges.length > 0) {
      lines.push('    transitions:')
      outgoingEdges.forEach((edge) => {
        const targetStepId = stepIdByNodeId.get(edge.toNodeId)
        if (!targetStepId) {
          return
        }
        const eventName = normalizeToken(edge.eventName, 'BTN_NEXT')
        lines.push(`      - trigger: ${normalizeTrigger(edge.trigger)}`)
        lines.push(`        event_type: ${normalizeEventType(edge.eventType, eventName)}`)
        lines.push(`        event_name: ${eventName}`)
        lines.push(`        target_step_id: ${targetStepId}`)
        lines.push(`        after_ms: ${Number.isFinite(edge.afterMs) ? Math.max(0, Math.trunc(edge.afterMs)) : 0}`)
        lines.push(`        priority: ${Number.isFinite(edge.priority) ? Math.max(0, Math.min(255, Math.trunc(edge.priority))) : 100}`)
      })
    } else {
      lines.push('    transitions: []')
    }
  })

  return `${lines.join('\n')}\n`
}

const autoLayoutNodes = (nodes: EditorNode[], edges: EditorEdge[]) => {
  const normalizedNodes = ensureInitialNode(nodes)
  if (normalizedNodes.length === 0) {
    return normalizedNodes
  }

  const nodeById = new Map(normalizedNodes.map((node) => [node.id, node]))
  const adjacency = new Map<string, string[]>()
  normalizedNodes.forEach((node) => adjacency.set(node.id, []))
  edges.forEach((edge) => {
    if (!nodeById.has(edge.fromNodeId) || !nodeById.has(edge.toNodeId)) {
      return
    }
    const current = adjacency.get(edge.fromNodeId) ?? []
    current.push(edge.toNodeId)
    adjacency.set(edge.fromNodeId, current)
  })

  const startNode = normalizedNodes.find((node) => node.isInitial) ?? normalizedNodes[0]
  const levelByNode = new Map<string, number>()
  const queue: string[] = [startNode.id]
  levelByNode.set(startNode.id, 0)

  while (queue.length > 0) {
    const currentId = queue.shift()
    if (!currentId) {
      continue
    }
    const currentLevel = levelByNode.get(currentId) ?? 0
    ;(adjacency.get(currentId) ?? []).forEach((nextId) => {
      if (levelByNode.has(nextId)) {
        return
      }
      levelByNode.set(nextId, currentLevel + 1)
      queue.push(nextId)
    })
  }

  const assignedLevels = Array.from(levelByNode.values())
  let maxLevel = assignedLevels.length > 0 ? Math.max(...assignedLevels) : 0
  normalizedNodes.forEach((node) => {
    if (levelByNode.has(node.id)) {
      return
    }
    maxLevel += 1
    levelByNode.set(node.id, maxLevel)
  })

  const rowsByLevel = new Map<number, string[]>()
  normalizedNodes.forEach((node) => {
    const level = levelByNode.get(node.id) ?? 0
    const current = rowsByLevel.get(level) ?? []
    current.push(node.id)
    rowsByLevel.set(level, current)
  })

  return normalizedNodes.map((node) => {
    const level = levelByNode.get(node.id) ?? 0
    const levelRows = rowsByLevel.get(level) ?? [node.id]
    const rowIndex = Math.max(0, levelRows.indexOf(node.id))
    return {
      ...node,
      x: 32 + level * NODE_HORIZONTAL_GAP,
      y: 40 + rowIndex * NODE_VERTICAL_GAP,
    }
  })
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
  const [linkEventName, setLinkEventName] = useState('BTN_NEXT')
  const [linkEventType, setLinkEventType] = useState('action')
  const [historyPast, setHistoryPast] = useState<GraphSnapshot[]>([])
  const [historyFuture, setHistoryFuture] = useState<GraphSnapshot[]>([])
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
            eventLabel: `${normalizeEventType(edge.eventType, edge.eventName)}:${edge.eventName}`,
          }
        })
        .filter((edge): edge is { id: string; path: string; labelX: number; labelY: number; eventLabel: string } => edge !== null),
    [edges, nodeMap],
  )

  const graphBounds = useMemo(() => {
    const bounds = nodes.reduce(
      (acc, node) => ({
        width: Math.max(acc.width, node.x + NODE_WIDTH + 40),
        height: Math.max(acc.height, node.y + NODE_HEIGHT + 40),
      }),
      { width: 980, height: CANVAS_HEIGHT },
    )
    return {
      width: bounds.width,
      height: Math.max(bounds.height, CANVAS_HEIGHT),
    }
  }, [nodes])

  const graphHealth = useMemo(() => {
    const nodeIds = new Set(nodes.map((node) => node.id))
    const incomingByNode = new Map<string, number>()
    const outgoingByNode = new Map<string, number>()
    nodes.forEach((node) => {
      incomingByNode.set(node.id, 0)
      outgoingByNode.set(node.id, 0)
    })

    edges.forEach((edge) => {
      if (!nodeIds.has(edge.fromNodeId) || !nodeIds.has(edge.toNodeId)) {
        return
      }
      incomingByNode.set(edge.toNodeId, (incomingByNode.get(edge.toNodeId) ?? 0) + 1)
      outgoingByNode.set(edge.fromNodeId, (outgoingByNode.get(edge.fromNodeId) ?? 0) + 1)
    })

    const initialNodes = nodes.filter((node) => node.isInitial)
    const selectedInitial = initialNodes[0] ?? null
    const isolatedNodes = nodes.filter(
      (node) => (incomingByNode.get(node.id) ?? 0) === 0 && (outgoingByNode.get(node.id) ?? 0) === 0,
    )
    const danglingNodes = nodes.filter(
      (node) =>
        !node.isInitial &&
        (incomingByNode.get(node.id) ?? 0) === 0 &&
        (outgoingByNode.get(node.id) ?? 0) > 0,
    )
    const terminalNodes = nodes.filter((node) => (outgoingByNode.get(node.id) ?? 0) === 0)

    const warnings: string[] = []
    if (initialNodes.length === 0) {
      warnings.push('No initial node selected.')
    } else if (initialNodes.length > 1) {
      warnings.push('Multiple initial nodes detected; only one should remain active.')
    }
    if (isolatedNodes.length > 0) {
      warnings.push(`${isolatedNodes.length} isolated node(s) without any transition.`)
    }
    if (danglingNodes.length > 0) {
      warnings.push(`${danglingNodes.length} node(s) cannot be reached from another node.`)
    }
    if (terminalNodes.length === 0 && nodes.length > 0) {
      warnings.push('No terminal node detected (all nodes loop to another node).')
    }

    return {
      nodeCount: nodes.length,
      edgeCount: edges.length,
      terminalCount: terminalNodes.length,
      selectedInitial,
      warnings,
    }
  }, [edges, nodes])

  const snapshotGraph = useCallback(
    (): GraphSnapshot => ({
      nodes: cloneNodes(nodes),
      edges: cloneEdges(edges),
      linkSourceId,
      linkEventName,
      linkEventType,
    }),
    [edges, linkEventName, linkEventType, linkSourceId, nodes],
  )

  const applyGraphSnapshot = useCallback((snapshot: GraphSnapshot) => {
    setNodes(cloneNodes(snapshot.nodes))
    setEdges(cloneEdges(snapshot.edges))
    setLinkSourceId(snapshot.linkSourceId)
    setLinkEventName(snapshot.linkEventName)
    setLinkEventType(snapshot.linkEventType)
  }, [])

  const pushHistory = useCallback(() => {
    const snapshot = snapshotGraph()
    setHistoryPast((previous) => {
      const next = [...previous, snapshot]
      return next.length > 80 ? next.slice(next.length - 80) : next
    })
    setHistoryFuture([])
  }, [snapshotGraph])

  const canUndo = historyPast.length > 0
  const canRedo = historyFuture.length > 0

  const handleUndo = useCallback(() => {
    if (!canUndo) {
      return
    }
    const previous = historyPast[historyPast.length - 1]
    if (!previous) {
      return
    }
    const current = snapshotGraph()
    setHistoryPast((past) => past.slice(0, -1))
    setHistoryFuture((future) => {
      const next = [...future, current]
      return next.length > 80 ? next.slice(next.length - 80) : next
    })
    applyGraphSnapshot(previous)
    setStatus('Undo applied.')
  }, [applyGraphSnapshot, canUndo, historyPast, snapshotGraph])

  const handleRedo = useCallback(() => {
    if (!canRedo) {
      return
    }
    const next = historyFuture[historyFuture.length - 1]
    if (!next) {
      return
    }
    const current = snapshotGraph()
    setHistoryFuture((future) => future.slice(0, -1))
    setHistoryPast((past) => {
      const updated = [...past, current]
      return updated.length > 80 ? updated.slice(updated.length - 80) : updated
    })
    applyGraphSnapshot(next)
    setStatus('Redo applied.')
  }, [applyGraphSnapshot, canRedo, historyFuture, snapshotGraph])

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

  useEffect(() => {
    const onKeyDown = (event: KeyboardEvent) => {
      if (!(event.metaKey || event.ctrlKey) || event.key.toLowerCase() !== 'z') {
        return
      }
      const target = event.target as HTMLElement | null
      if (
        target &&
        (target.tagName === 'INPUT' ||
          target.tagName === 'TEXTAREA' ||
          target.tagName === 'SELECT' ||
          target.isContentEditable)
      ) {
        return
      }
      event.preventDefault()
      if (event.shiftKey) {
        handleRedo()
      } else {
        handleUndo()
      }
    }
    window.addEventListener('keydown', onKeyDown)
    return () => window.removeEventListener('keydown', onKeyDown)
  }, [handleRedo, handleUndo])

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
    pushHistory()
    setNodes((previous) =>
      previous.map((node) => ({
        ...node,
        isInitial: node.id === nodeId,
      })),
    )
  }, [pushHistory])

  const handleTemplateChange = (value: string) => {
    setSelectedTemplate(value)
    if (value && TEMPLATE_LIBRARY[value]) {
      setDraft(TEMPLATE_LIBRARY[value])
      setStatus('Template loaded. Review and adjust resources before deploy.')
      setErrors([])
    }
  }

  const createAndAddNode = useCallback(
    (options?: { x?: number; y?: number }) => {
      const nextNode = createNode(nodes.length + 1)
      if (typeof options?.x === 'number') {
        nextNode.x = Math.max(10, options.x)
      }
      if (typeof options?.y === 'number') {
        nextNode.y = Math.max(10, options.y)
      }
      if (nodes.length === 0) {
        nextNode.isInitial = true
      }
      setNodes((previous) => [...previous, nextNode])
      return nextNode
    },
    [nodes.length],
  )

  const handleAddNode = useCallback(() => {
    pushHistory()
    createAndAddNode()
    setStatus('Node added.')
    setErrors([])
  }, [createAndAddNode, pushHistory])

  const handleAddChildNode = useCallback(
    (sourceNodeId: string) => {
      const sourceNode = nodeMap.get(sourceNodeId)
      if (!sourceNode) {
        return
      }
      pushHistory()
      const newNode = createAndAddNode({
        x: sourceNode.x + NODE_HORIZONTAL_GAP,
        y: sourceNode.y,
      })
      if (!newNode) {
        return
      }
      setEdges((previous) => [
        ...previous,
        {
          id: `edge-${makeIdFragment()}`,
          fromNodeId: sourceNodeId,
          toNodeId: newNode.id,
          trigger: DEFAULT_EDGE_TRIGGER,
          eventType: normalizeEventType(linkEventType, linkEventName),
          eventName: linkEventName.trim() || 'BTN_NEXT',
          afterMs: DEFAULT_EDGE_AFTER_MS,
          priority: DEFAULT_EDGE_PRIORITY,
        },
      ])
      setStatus('Child node added and linked.')
      setErrors([])
    },
    [createAndAddNode, linkEventName, linkEventType, nodeMap, pushHistory],
  )

  const handleCanvasDoubleClick = useCallback(
    (event: ReactMouseEvent<HTMLDivElement>) => {
      if (!canvasRef.current) {
        return
      }
      const bounds = canvasRef.current.getBoundingClientRect()
      const { scrollLeft, scrollTop } = canvasRef.current
      const x = event.clientX - bounds.left + scrollLeft - NODE_WIDTH / 2
      const y = event.clientY - bounds.top + scrollTop - 24
      pushHistory()
      createAndAddNode({ x, y })
      setStatus('Node added on canvas.')
      setErrors([])
    },
    [createAndAddNode, pushHistory],
  )

  const handleResetGraph = useCallback(() => {
    pushHistory()
    const graph = createDefaultGraph()
    setNodes(graph.nodes)
    setEdges(graph.edges)
    setLinkSourceId(null)
    setLinkEventName('BTN_NEXT')
    setLinkEventType('action')
    setStatus('Node graph reset to default.')
    setErrors([])
  }, [pushHistory])

  const handleRemoveNode = useCallback((nodeId: string) => {
    pushHistory()
    setNodes((previous) => ensureInitialNode(previous.filter((node) => node.id !== nodeId)))
    setEdges((previous) => previous.filter((edge) => edge.fromNodeId !== nodeId && edge.toNodeId !== nodeId))
    setLinkSourceId((previous) => (previous === nodeId ? null : previous))
  }, [pushHistory])

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
      const trimmedEvent = linkEventName.trim()
      if (!trimmedEvent) {
        setStatus('Link event cannot be empty.')
        return
      }

      pushHistory()
      setEdges((previous) => {
        const existing = previous.find(
          (edge) => edge.fromNodeId === linkSourceId && edge.toNodeId === nodeId,
        )
        if (existing) {
          return previous.map((edge) =>
            edge.id === existing.id
              ? {
                  ...edge,
                  trigger: DEFAULT_EDGE_TRIGGER,
                  eventType: normalizeEventType(linkEventType, trimmedEvent),
                  eventName: trimmedEvent,
                  afterMs: DEFAULT_EDGE_AFTER_MS,
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
            trigger: DEFAULT_EDGE_TRIGGER,
            eventType: normalizeEventType(linkEventType, trimmedEvent),
            eventName: trimmedEvent,
            afterMs: DEFAULT_EDGE_AFTER_MS,
            priority: DEFAULT_EDGE_PRIORITY,
          },
        ]
      })
      setLinkSourceId(null)
      setStatus('Nodes linked. Edit event directly inside the source node if needed.')
      setErrors([])
    },
    [linkEventName, linkEventType, linkSourceId, pushHistory],
  )

  const handleRemoveEdge = useCallback((edgeId: string) => {
    pushHistory()
    setEdges((previous) => previous.filter((edge) => edge.id !== edgeId))
  }, [pushHistory])

  const handleDragStart = useCallback(
    (event: ReactMouseEvent<HTMLDivElement>, nodeId: string) => {
      if (!canvasRef.current) {
        return
      }
      const node = nodeMap.get(nodeId)
      if (!node) {
        return
      }
      pushHistory()
      const bounds = canvasRef.current.getBoundingClientRect()
      const { scrollLeft, scrollTop } = canvasRef.current
      setDragState({
        nodeId,
        offsetX: event.clientX - bounds.left + scrollLeft - node.x,
        offsetY: event.clientY - bounds.top + scrollTop - node.y,
      })
      event.preventDefault()
      event.stopPropagation()
    },
    [nodeMap, pushHistory],
  )

  const handleCanvasMouseMove = useCallback(
    (event: ReactMouseEvent<HTMLDivElement>) => {
      if (!dragState || !canvasRef.current) {
        return
      }
      const bounds = canvasRef.current.getBoundingClientRect()
      const { scrollLeft, scrollTop } = canvasRef.current
      const maxX = Math.max(20, graphBounds.width - NODE_WIDTH - 10)
      const maxY = Math.max(20, graphBounds.height - NODE_HEIGHT - 10)
      const nextX = Math.min(maxX, Math.max(10, event.clientX - bounds.left + scrollLeft - dragState.offsetX))
      const nextY = Math.min(maxY, Math.max(10, event.clientY - bounds.top + scrollTop - dragState.offsetY))
      updateNode(dragState.nodeId, { x: nextX, y: nextY })
    },
    [dragState, graphBounds.height, graphBounds.width, updateNode],
  )

  const handleGenerateFromNodes = useCallback(() => {
    const generated = buildStoryYaml(graphScenarioId, nodes, edges)
    setDraft(generated)
    setStatus('YAML generated from linked nodes.')
    setErrors([])
  }, [edges, graphScenarioId, nodes])

  const handleAutoLayout = useCallback(() => {
    pushHistory()
    setNodes((previous) => autoLayoutNodes(previous, edges))
    setStatus('Auto layout applied.')
  }, [edges, pushHistory])

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
        <div className="grid gap-3 md:grid-cols-[minmax(0,1fr)_auto_auto_auto_auto_auto_auto] md:items-end">
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
            onClick={handleAutoLayout}
            className="focus-ring min-h-[44px] rounded-full border border-[var(--ink-500)] px-4 text-sm font-semibold text-[var(--ink-700)]"
          >
            Auto layout
          </button>
          <button
            type="button"
            onClick={handleUndo}
            disabled={!canUndo}
            className="focus-ring min-h-[44px] rounded-full border border-[var(--ink-500)] px-4 text-sm font-semibold text-[var(--ink-700)] disabled:opacity-60"
            title="Cmd/Ctrl+Z"
          >
            Undo
          </button>
          <button
            type="button"
            onClick={handleRedo}
            disabled={!canRedo}
            className="focus-ring min-h-[44px] rounded-full border border-[var(--ink-500)] px-4 text-sm font-semibold text-[var(--ink-700)] disabled:opacity-60"
            title="Cmd/Ctrl+Shift+Z"
          >
            Redo
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
          Use <span className="font-semibold">Add node</span>, <span className="font-semibold">Add child</span>, or
          double-click the canvas to create nodes quickly.
        </p>
        {linkSourceId && (
          <div className="mt-2 space-y-2 rounded-xl border border-[var(--accent-700)] bg-white/70 px-3 py-2 text-xs text-[var(--accent-700)]">
            <p>
              Link mode active from <span className="font-semibold">{nodeMap.get(linkSourceId)?.stepId ?? linkSourceId}</span>.
              Click a target node.
            </p>
            <div className="flex flex-wrap items-end gap-2">
              <label className="text-[10px] uppercase tracking-[0.15em] text-[var(--accent-700)]" htmlFor="link-event-name">
                Event
                <input
                  id="link-event-name"
                  value={linkEventName}
                  onChange={(event) => {
                    const nextName = event.target.value
                    setLinkEventName(nextName)
                    setLinkEventType(inferEventType(nextName))
                  }}
                  className="focus-ring mt-1 min-h-[32px] rounded-lg border border-[var(--accent-700)] bg-white px-2 text-xs text-[var(--ink-900)]"
                />
              </label>
              <label className="text-[10px] uppercase tracking-[0.15em] text-[var(--accent-700)]" htmlFor="link-event-type">
                Event type
                <select
                  id="link-event-type"
                  value={linkEventType}
                  onChange={(event) => setLinkEventType(event.target.value)}
                  className="focus-ring mt-1 min-h-[32px] rounded-lg border border-[var(--accent-700)] bg-white px-2 text-xs text-[var(--ink-900)]"
                >
                  <option value="action">action</option>
                  <option value="unlock">unlock</option>
                  <option value="audio_done">audio_done</option>
                  <option value="timer">timer</option>
                  <option value="serial">serial</option>
                  <option value="none">none</option>
                </select>
              </label>
              <button
                type="button"
                onClick={() => setLinkSourceId(null)}
                className="focus-ring min-h-[32px] rounded-lg border border-[var(--accent-700)] px-3 text-[10px] font-semibold uppercase tracking-[0.15em]"
              >
                Cancel link
              </button>
            </div>
          </div>
        )}

        <div className="mt-3 rounded-xl border border-[var(--mist-400)] bg-white/60 px-3 py-2 text-xs text-[var(--ink-700)]">
          <div className="flex flex-wrap items-center gap-2">
            <span className="rounded-full border border-[var(--mist-400)] px-2 py-0.5">
              Nodes: <span className="font-semibold">{graphHealth.nodeCount}</span>
            </span>
            <span className="rounded-full border border-[var(--mist-400)] px-2 py-0.5">
              Links: <span className="font-semibold">{graphHealth.edgeCount}</span>
            </span>
            <span className="rounded-full border border-[var(--mist-400)] px-2 py-0.5">
              Endings: <span className="font-semibold">{graphHealth.terminalCount}</span>
            </span>
            <span className="rounded-full border border-[var(--mist-400)] px-2 py-0.5">
              Initial: <span className="font-semibold">{graphHealth.selectedInitial?.stepId ?? 'none'}</span>
            </span>
          </div>
          {graphHealth.warnings.length > 0 ? (
            <div className="mt-2 space-y-1 text-[var(--accent-700)]">
              {graphHealth.warnings.map((warning) => (
                <p key={warning}>- {warning}</p>
              ))}
            </div>
          ) : (
            <p className="mt-2 text-[var(--teal-500)]">Graph structure looks consistent.</p>
          )}
        </div>

        <div
          ref={canvasRef}
          className="relative mt-4 overflow-auto rounded-2xl border border-white/60 bg-white/40"
          style={{ height: `${CANVAS_HEIGHT}px` }}
          onMouseMove={handleCanvasMouseMove}
          onMouseUp={() => setDragState(null)}
          onMouseLeave={() => setDragState(null)}
          onDoubleClick={handleCanvasDoubleClick}
        >
          <div
            className="relative"
            style={{
              width: `${graphBounds.width}px`,
              height: `${graphBounds.height}px`,
            }}
          >
            <svg className="pointer-events-none absolute left-0 top-0" width={graphBounds.width} height={graphBounds.height}>
              {renderedEdges.map((edge) => (
                <g key={edge.id}>
                  <path d={edge.path} fill="none" stroke="rgba(31,42,68,0.55)" strokeWidth={2.5} />
                  <text x={edge.labelX} y={edge.labelY} textAnchor="middle" fontSize={11} fill="#1f2a44">
                    {edge.eventLabel}
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
                  onDoubleClick={(event) => event.stopPropagation()}
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
                          handleAddChildNode(node.id)
                        }}
                        className="rounded-md border border-[var(--teal-500)] px-2 py-1 text-[10px] text-[var(--teal-500)]"
                      >
                        +Child
                      </button>
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
                      onFocus={pushHistory}
                      onClick={(event) => event.stopPropagation()}
                      className="focus-ring mt-1 min-h-[32px] w-full rounded-lg border border-[var(--mist-400)] px-2 text-xs text-[var(--ink-900)]"
                    />
                  </label>
                  <label className="mt-2 block text-[10px] uppercase tracking-[0.15em] text-[var(--ink-500)]">
                    Screen
                    <input
                      value={node.screenSceneId}
                      onChange={(event) => updateNode(node.id, { screenSceneId: event.target.value })}
                      onFocus={pushHistory}
                      onClick={(event) => event.stopPropagation()}
                      className="focus-ring mt-1 min-h-[32px] w-full rounded-lg border border-[var(--mist-400)] px-2 text-xs text-[var(--ink-900)]"
                    />
                  </label>
                  <label className="mt-2 block text-[10px] uppercase tracking-[0.15em] text-[var(--ink-500)]">
                    Audio pack
                    <input
                      value={node.audioPackId}
                      onChange={(event) => updateNode(node.id, { audioPackId: event.target.value })}
                      onFocus={pushHistory}
                      onClick={(event) => event.stopPropagation()}
                      placeholder="PACK_BOOT_RADIO"
                      className="focus-ring mt-1 min-h-[32px] w-full rounded-lg border border-[var(--mist-400)] px-2 text-xs text-[var(--ink-900)]"
                    />
                  </label>
                  <label className="mt-2 block text-[10px] uppercase tracking-[0.15em] text-[var(--ink-500)]">
                    Actions (comma)
                    <input
                      value={node.actionsCsv}
                      onChange={(event) => updateNode(node.id, { actionsCsv: event.target.value })}
                      onFocus={pushHistory}
                      onClick={(event) => event.stopPropagation()}
                      placeholder="ACTION_TRACE_STEP"
                      className="focus-ring mt-1 min-h-[32px] w-full rounded-lg border border-[var(--mist-400)] px-2 text-xs text-[var(--ink-900)]"
                    />
                  </label>
                  <label className="mt-2 block text-[10px] uppercase tracking-[0.15em] text-[var(--ink-500)]">
                    Apps (comma)
                    <input
                      value={node.appsCsv}
                      onChange={(event) => updateNode(node.id, { appsCsv: event.target.value })}
                      onFocus={pushHistory}
                      onClick={(event) => event.stopPropagation()}
                      placeholder="APP_SCREEN,APP_GATE"
                      className="focus-ring mt-1 min-h-[32px] w-full rounded-lg border border-[var(--mist-400)] px-2 text-xs text-[var(--ink-900)]"
                    />
                  </label>
                  <label className="mt-2 flex items-center justify-between rounded-lg border border-[var(--mist-400)] px-2 py-1 text-[10px] uppercase tracking-[0.15em] text-[var(--ink-500)]">
                    MP3 gate open
                    <input
                      type="checkbox"
                      checked={node.mp3GateOpen}
                      onChange={(event) => updateNode(node.id, { mp3GateOpen: event.target.checked })}
                      onFocus={pushHistory}
                      onClick={(event) => event.stopPropagation()}
                      className="h-4 w-4 accent-[var(--teal-500)]"
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
                              onFocus={pushHistory}
                              onClick={(event) => event.stopPropagation()}
                              onChange={(event) =>
                                setEdges((previous) =>
                                  previous.map((candidate) =>
                                    candidate.id === edge.id
                                      ? {
                                          ...candidate,
                                          eventName: event.target.value,
                                          eventType: inferEventType(event.target.value),
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
                          <div className="grid grid-cols-2 gap-1">
                            <select
                              value={edge.eventType}
                              onFocus={pushHistory}
                              onClick={(event) => event.stopPropagation()}
                              onChange={(event) =>
                                setEdges((previous) =>
                                  previous.map((candidate) =>
                                    candidate.id === edge.id
                                      ? {
                                          ...candidate,
                                          eventType: event.target.value,
                                        }
                                      : candidate,
                                  ),
                                )
                              }
                              className="focus-ring min-h-[26px] rounded-md border border-[var(--mist-400)] px-2 text-[10px] text-[var(--ink-900)]"
                            >
                              <option value="action">action</option>
                              <option value="unlock">unlock</option>
                              <option value="audio_done">audio_done</option>
                              <option value="timer">timer</option>
                              <option value="serial">serial</option>
                              <option value="none">none</option>
                            </select>
                            <select
                              value={edge.trigger}
                              onFocus={pushHistory}
                              onClick={(event) => event.stopPropagation()}
                              onChange={(event) =>
                                setEdges((previous) =>
                                  previous.map((candidate) =>
                                    candidate.id === edge.id
                                      ? {
                                          ...candidate,
                                          trigger: event.target.value,
                                        }
                                      : candidate,
                                  ),
                                )
                              }
                              className="focus-ring min-h-[26px] rounded-md border border-[var(--mist-400)] px-2 text-[10px] text-[var(--ink-900)]"
                            >
                              <option value="on_event">on_event</option>
                              <option value="after_ms">after_ms</option>
                              <option value="immediate">immediate</option>
                            </select>
                            <input
                              type="number"
                              value={edge.afterMs}
                              min={0}
                              onFocus={pushHistory}
                              onClick={(event) => event.stopPropagation()}
                              onChange={(event) =>
                                setEdges((previous) =>
                                  previous.map((candidate) =>
                                    candidate.id === edge.id
                                      ? {
                                          ...candidate,
                                          afterMs: Number.parseInt(event.target.value || '0', 10) || 0,
                                        }
                                      : candidate,
                                  ),
                                )
                              }
                              className="focus-ring min-h-[26px] rounded-md border border-[var(--mist-400)] px-2 text-[10px] text-[var(--ink-900)]"
                            />
                            <input
                              type="number"
                              value={edge.priority}
                              min={0}
                              max={255}
                              onFocus={pushHistory}
                              onClick={(event) => event.stopPropagation()}
                              onChange={(event) =>
                                setEdges((previous) =>
                                  previous.map((candidate) =>
                                    candidate.id === edge.id
                                      ? {
                                          ...candidate,
                                          priority: Number.parseInt(event.target.value || '0', 10) || 0,
                                        }
                                      : candidate,
                                  ),
                                )
                              }
                              className="focus-ring min-h-[26px] rounded-md border border-[var(--mist-400)] px-2 text-[10px] text-[var(--ink-900)]"
                            />
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
