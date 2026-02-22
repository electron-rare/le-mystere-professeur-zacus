import { useCallback, useEffect, useMemo, useRef, useState, type MouseEvent as ReactMouseEvent } from 'react'
import CodeMirror from '@uiw/react-codemirror'
import { yaml as yamlLanguage } from '@codemirror/lang-yaml'
import {
  Background,
  Controls,
  Handle,
  MarkerType,
  MiniMap,
  Position,
  ReactFlow,
  type ReactFlowInstance,
  type Connection,
  type Edge as FlowEdge,
  type Node as FlowNode,
  type NodeProps,
} from '@xyflow/react'
import '@xyflow/react/dist/style.css'
import type { DeviceCapabilities } from '../lib/deviceApi'
import {
  DEFAULT_AFTER_MS,
  DEFAULT_EVENT_NAME,
  DEFAULT_EVENT_TYPE,
  DEFAULT_NODE_APPS,
  DEFAULT_PRIORITY,
  DEFAULT_SCENARIO_ID,
  DEFAULT_SCENARIO_VERSION,
  DEFAULT_SCENE_ID,
  DEFAULT_STEP_ID,
  DEFAULT_TRIGGER,
  autoLayoutStoryGraph,
  generateStoryYamlFromGraph,
  inferEventTypeFromName,
  importStoryYamlToGraph,
  STORY_TEMPLATE_LIBRARY,
  type AppBinding,
  type EventType,
  type StoryEdge,
  type StoryGraphDocument,
  type StoryNode,
  validateStoryGraph,
} from '../features/story-designer'
import { Badge, Button, Field, InlineNotice, Panel, SectionHeader } from './ui'

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

type TabKey = 'graph' | 'bindings' | 'yaml'

type FlowNodeData = {
  stepId: string
  screenSceneId: string
  audioPackId: string
  isInitial: boolean
  appsCount: number
  actionsCount: number
  pendingLinkTarget: boolean
  pendingLinkCandidate: boolean
}

type StoryCanvasNode = FlowNode<FlowNodeData, 'storyNode'>

type StatusTone = 'info' | 'success' | 'warning' | 'error'
type BindingFieldType = 'text' | 'number' | 'boolean'
type ContextMenuKind = 'pane' | 'node' | 'edge'

type ContextMenuState = {
  kind: ContextMenuKind
  x: number
  y: number
  nodeId?: string
  edgeId?: string
  flowX?: number
  flowY?: number
}

type BindingField = {
  key: string
  label: string
  type: BindingFieldType
  min?: number
  max?: number
  defaultValue: string | number | boolean
}

const NODE_WIDTH = 260
const NODE_HEIGHT = 300

const LA_CONFIG_KEYS = ['hold_ms', 'unlock_event', 'require_listening']

const SIMPLE_BINDING_FIELDS: Partial<Record<AppBinding['app'], BindingField[]>> = {
  AUDIO_PACK: [
    { key: 'volume_pct', label: 'volume_pct', type: 'number', min: 0, max: 100, defaultValue: 85 },
    { key: 'ducking', label: 'ducking', type: 'boolean', defaultValue: false },
    { key: 'output', label: 'output', type: 'text', defaultValue: 'AUTO' },
  ],
  SCREEN_SCENE: [
    { key: 'theme', label: 'theme', type: 'text', defaultValue: 'GLASS' },
    { key: 'timeout_ms', label: 'timeout_ms', type: 'number', min: 0, max: 120000, defaultValue: 0 },
  ],
  MP3_GATE: [
    { key: 'open_event', label: 'open_event', type: 'text', defaultValue: 'AUDIO_DONE' },
    { key: 'debounce_ms', label: 'debounce_ms', type: 'number', min: 0, max: 10000, defaultValue: 200 },
  ],
  WIFI_STACK: [
    { key: 'ssid_profile', label: 'ssid_profile', type: 'text', defaultValue: 'DEFAULT' },
    { key: 'reconnect_ms', label: 'reconnect_ms', type: 'number', min: 0, max: 120000, defaultValue: 5000 },
  ],
  ESPNOW_STACK: [
    { key: 'channel', label: 'channel', type: 'number', min: 1, max: 14, defaultValue: 1 },
    { key: 'peer_group', label: 'peer_group', type: 'text', defaultValue: 'DEFAULT' },
  ],
}

const normalizeTokenInput = (value: string) => value.trim().toUpperCase().replace(/[^A-Z0-9_]/g, '_')

const stripLaDetectorConfig = (config?: AppBinding['config']) => {
  if (!config) {
    return undefined
  }
  const next = { ...config }
  LA_CONFIG_KEYS.forEach((key) => {
    delete next[key]
  })
  return Object.keys(next).length > 0 ? next : undefined
}

const withConfigValue = (
  config: AppBinding['config'] | undefined,
  key: string,
  value: string | number | boolean | undefined,
): AppBinding['config'] | undefined => {
  const next = { ...(config ?? {}) }
  if (value === undefined) {
    delete next[key]
  } else {
    next[key] = value
  }
  return Object.keys(next).length > 0 ? next : undefined
}

const cloneDocument = (document: StoryGraphDocument): StoryGraphDocument =>
  JSON.parse(JSON.stringify(document)) as StoryGraphDocument

const createFallbackDocument = (): StoryGraphDocument => {
  const imported = importStoryYamlToGraph(STORY_TEMPLATE_LIBRARY.DEFAULT)
  if (imported.document) {
    return autoLayoutStoryGraph(imported.document, {
      direction: 'LR',
      nodeWidth: NODE_WIDTH,
      nodeHeight: NODE_HEIGHT,
    })
  }

  return {
    scenarioId: DEFAULT_SCENARIO_ID,
    version: DEFAULT_SCENARIO_VERSION,
    initialStep: `${DEFAULT_STEP_ID}_1`,
    appBindings: [
      {
        id: 'APP_SCREEN',
        app: 'SCREEN_SCENE',
      },
    ],
    nodes: [
      {
        id: 'node-1',
        stepId: `${DEFAULT_STEP_ID}_1`,
        screenSceneId: DEFAULT_SCENE_ID,
        audioPackId: '',
        actions: ['ACTION_TRACE_STEP'],
        apps: [...DEFAULT_NODE_APPS],
        mp3GateOpen: false,
        x: 80,
        y: 80,
        isInitial: true,
      },
    ],
    edges: [],
  }
}

const loadInitialDraft = () => {
  const stored = localStorage.getItem('story-draft')
  if (stored && stored.trim().length > 0) {
    return stored
  }
  return STORY_TEMPLATE_LIBRARY.DEFAULT
}

const loadInitialDocument = (): StoryGraphDocument => {
  const stored = localStorage.getItem('story-graph-document')
  if (stored) {
    try {
      const parsed = JSON.parse(stored) as StoryGraphDocument
      if (
        parsed &&
        typeof parsed === 'object' &&
        Array.isArray(parsed.nodes) &&
        Array.isArray(parsed.edges) &&
        Array.isArray(parsed.appBindings)
      ) {
        return parsed
      }
    } catch {
      // Fallback below.
    }
  }

  const draft = loadInitialDraft()
  const imported = importStoryYamlToGraph(draft)
  if (imported.document) {
    return autoLayoutStoryGraph(imported.document, {
      direction: 'LR',
      nodeWidth: NODE_WIDTH,
      nodeHeight: NODE_HEIGHT,
    })
  }

  return createFallbackDocument()
}

const nextNodeId = (document: StoryGraphDocument) => {
  const ids = new Set(document.nodes.map((node) => node.id))
  let index = document.nodes.length + 1
  let candidate = `node-${index}`
  while (ids.has(candidate)) {
    index += 1
    candidate = `node-${index}`
  }
  return candidate
}

const nextStepId = (document: StoryGraphDocument) => {
  const ids = new Set(document.nodes.map((node) => normalizeTokenInput(node.stepId)))
  let index = document.nodes.length + 1
  let candidate = `${DEFAULT_STEP_ID}_${index}`
  while (ids.has(candidate)) {
    index += 1
    candidate = `${DEFAULT_STEP_ID}_${index}`
  }
  return candidate
}

const nextBindingId = (document: StoryGraphDocument) => {
  const ids = new Set(document.appBindings.map((binding) => normalizeTokenInput(binding.id)))
  let index = document.appBindings.length + 1
  let candidate = `APP_CUSTOM_${index}`
  while (ids.has(candidate)) {
    index += 1
    candidate = `APP_CUSTOM_${index}`
  }
  return candidate
}

const nextEdgeId = () => `edge-${Date.now().toString(36)}-${Math.random().toString(36).slice(2, 6)}`

const clampContextPosition = (x: number, y: number) => {
  const safeX = Number.isFinite(x) ? x : typeof window !== 'undefined' ? window.innerWidth * 0.5 : 120
  const safeY = Number.isFinite(y) ? y : typeof window !== 'undefined' ? window.innerHeight * 0.5 : 120
  const maxX = typeof window !== 'undefined' ? window.innerWidth - 270 : x
  const maxY = typeof window !== 'undefined' ? window.innerHeight - 320 : y
  return {
    x: Math.max(12, Math.min(safeX, maxX)),
    y: Math.max(12, Math.min(safeY, maxY)),
  }
}

const nodeCardClass =
  'rounded-2xl border bg-white/95 px-3 py-2 shadow-[0_8px_24px_rgba(15,23,42,0.14)] backdrop-blur-sm min-w-[230px] transition-all duration-200'

const StoryFlowNode = ({ data, selected }: NodeProps<StoryCanvasNode>) => (
  <div
    className={`${nodeCardClass} ${
      selected ? 'border-[var(--accent-700)]' : 'border-[var(--mist-400)]'
    } ${data.pendingLinkCandidate ? 'ring-1 ring-[var(--mist-500)] ring-offset-1' : ''} ${
      data.pendingLinkTarget ? 'ring-2 ring-[var(--accent-500)] ring-offset-2 ring-offset-white/50' : ''
    }`}
  >
    <Handle type="target" id="in" position={Position.Left} />
    <Handle type="source" id="out" position={Position.Right} />
    <div className="flex items-center justify-between gap-2">
      <p className="text-xs font-semibold text-[var(--ink-900)]">{data.stepId}</p>
      {data.isInitial ? <Badge tone="success">Initial</Badge> : null}
    </div>
    {data.pendingLinkTarget ? (
      <p className="mt-1 text-[10px] font-semibold text-[var(--accent-800)]">Mode liaison activé - clic droit pour valider</p>
    ) : null}
    <p className="mt-1 text-[11px] uppercase tracking-[0.12em] text-[var(--ink-500)]">Scène: {data.screenSceneId}</p>
    <p className="text-[11px] text-[var(--ink-500)]">
      {data.audioPackId ? `Audio: ${data.audioPackId}` : 'Audio: non défini'}
    </p>
    <p className="mt-2 text-[11px] text-[var(--ink-500)]">Apps liées: {data.appsCount}</p>
    <p className="text-[11px] text-[var(--ink-500)]">Actions: {data.actionsCount}</p>
  </div>
)

const flowNodeTypes = { storyNode: StoryFlowNode }

const StoryDesigner = ({ onValidate, onDeploy, onTestRun, capabilities }: StoryDesignerProps) => {
  const [draft, setDraft] = useState<string>(loadInitialDraft)
  const [document, setDocument] = useState<StoryGraphDocument>(loadInitialDocument)
  const [status, setStatus] = useState('')
  const [statusTone, setStatusTone] = useState<StatusTone>('info')
  const [errors, setErrors] = useState<string[]>([])
  const [importWarnings, setImportWarnings] = useState<string[]>([])
  const [selectedTemplate, setSelectedTemplate] = useState('')
  const [selectedNodeId, setSelectedNodeId] = useState<string | null>(null)
  const [selectedEdgeId, setSelectedEdgeId] = useState<string | null>(null)
  const [activeTab, setActiveTab] = useState<TabKey>('graph')
  const [linkEventName, setLinkEventName] = useState(DEFAULT_EVENT_NAME)
  const [linkEventType, setLinkEventType] = useState<EventType>(DEFAULT_EVENT_TYPE)
  const [historyPast, setHistoryPast] = useState<StoryGraphDocument[]>([])
  const [historyFuture, setHistoryFuture] = useState<StoryGraphDocument[]>([])
  const [busyAction, setBusyAction] = useState<'validate' | 'deploy' | 'test' | null>(null)
  const [contextMenu, setContextMenu] = useState<ContextMenuState | null>(null)
  const [pendingLinkSourceId, setPendingLinkSourceId] = useState<string | null>(null)
  const [pendingLinkHoverNodeId, setPendingLinkHoverNodeId] = useState<string | null>(null)
  const [actionDraft, setActionDraft] = useState('')

  const validateEnabled = capabilities.canValidate
  const deployEnabled = capabilities.canDeploy
  const testRunEnabled = capabilities.canDeploy && capabilities.canSelectScenario && capabilities.canStart

  const documentRef = useRef(document)
  const flowInstanceRef = useRef<ReactFlowInstance<StoryCanvasNode, FlowEdge> | null>(null)

  useEffect(() => {
    documentRef.current = document
  }, [document])

  useEffect(() => {
    const timer = window.setTimeout(() => {
      localStorage.setItem('story-draft', draft)
    }, 350)
    return () => window.clearTimeout(timer)
  }, [draft])

  useEffect(() => {
    const timer = window.setTimeout(() => {
      localStorage.setItem('story-graph-document', JSON.stringify(document))
    }, 350)
    return () => window.clearTimeout(timer)
  }, [document])

  const pushHistorySnapshot = useCallback(() => {
    const snapshot = cloneDocument(documentRef.current)
    setHistoryPast((previous) => {
      const last = previous[previous.length - 1]
      if (last && JSON.stringify(last) === JSON.stringify(snapshot)) {
        return previous
      }
      const next = [...previous, snapshot]
      return next.length > 80 ? next.slice(next.length - 80) : next
    })
    setHistoryFuture([])
  }, [])

  const applyDocumentWithHistory = useCallback(
    (nextDocument: StoryGraphDocument, message: string, tone: StatusTone = 'success') => {
      pushHistorySnapshot()
      setDocument(nextDocument)
      setStatus(message)
      setStatusTone(tone)
      setErrors([])
    },
    [pushHistorySnapshot],
  )

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
    const current = cloneDocument(documentRef.current)
    setHistoryPast((past) => past.slice(0, -1))
    setHistoryFuture((future) => {
      const next = [...future, current]
      return next.length > 80 ? next.slice(next.length - 80) : next
    })
    setDocument(previous)
    setStatus('Annulation appliquée.')
    setStatusTone('info')
  }, [canUndo, historyPast])

  const handleRedo = useCallback(() => {
    if (!canRedo) {
      return
    }
    const nextState = historyFuture[historyFuture.length - 1]
    if (!nextState) {
      return
    }
    const current = cloneDocument(documentRef.current)
    setHistoryFuture((future) => future.slice(0, -1))
    setHistoryPast((past) => {
      const next = [...past, current]
      return next.length > 80 ? next.slice(next.length - 80) : next
    })
    setDocument(nextState)
    setStatus('Rétablissement appliqué.')
    setStatusTone('info')
  }, [canRedo, historyFuture])

  useEffect(() => {
    if (!pendingLinkSourceId) {
      setPendingLinkHoverNodeId(null)
      return
    }
    if (!document.nodes.some((node) => node.id === pendingLinkSourceId)) {
      setPendingLinkSourceId(null)
    }
    setPendingLinkHoverNodeId(null)
  }, [document.nodes, pendingLinkSourceId])

  useEffect(() => {
    if (!contextMenu) {
      return
    }
    const onKeyDown = (event: KeyboardEvent) => {
      if (event.key === 'Escape') {
        setContextMenu(null)
      }
    }
    window.addEventListener('keydown', onKeyDown)
    return () => {
      window.removeEventListener('keydown', onKeyDown)
    }
  }, [contextMenu])

  const graphValidation = useMemo(() => validateStoryGraph(document), [document])

  const duplicateBindingIds = useMemo(() => {
    const countById = new Map<string, number>()
    document.appBindings.forEach((binding) => {
      const key = normalizeTokenInput(binding.id)
      countById.set(key, (countById.get(key) ?? 0) + 1)
    })
    return new Set(Array.from(countById.entries()).filter((entry) => entry[1] > 1).map((entry) => entry[0]))
  }, [document.appBindings])

  const selectedNode = useMemo(
    () => document.nodes.find((node) => node.id === selectedNodeId) ?? null,
    [document.nodes, selectedNodeId],
  )

  const selectedEdge = useMemo(
    () => document.edges.find((edge) => edge.id === selectedEdgeId) ?? null,
    [document.edges, selectedEdgeId],
  )

  useEffect(() => {
    const onKeyDown = (event: KeyboardEvent) => {
      if (event.key === 'Escape') {
        if (pendingLinkSourceId) {
          event.preventDefault()
          setPendingLinkSourceId(null)
          setPendingLinkHoverNodeId(null)
          setContextMenu(null)
          setStatus('Liaison annulée.')
          setStatusTone('info')
          return
        }
        if (contextMenu) {
          event.preventDefault()
          setContextMenu(null)
          return
        }
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

      if (event.key.toLowerCase() === 'l' && (event.ctrlKey || event.metaKey) && selectedNode) {
        event.preventDefault()
        setPendingLinkSourceId(selectedNode.id)
        setStatus(`Liaison armée depuis ${selectedNode.stepId}. Clique un node cible.`)
        setStatusTone('info')
        return
      }

      if (!(event.metaKey || event.ctrlKey) || event.key.toLowerCase() !== 'z') {
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
  }, [contextMenu, handleRedo, handleUndo, pendingLinkSourceId, selectedNode])

  const nodeLookup = useMemo(() => new Map(document.nodes.map((node) => [node.id, node])), [document.nodes])
  const bindingIdSet = useMemo(
    () => new Set(document.appBindings.map((binding) => normalizeTokenInput(binding.id))),
    [document.appBindings],
  )

  const selectedNodeOutgoingEdges = useMemo(
    () => (selectedNode ? document.edges.filter((edge) => edge.fromNodeId === selectedNode.id) : []),
    [document.edges, selectedNode],
  )

  const selectedNodeMissingBindings = useMemo(
    () => (selectedNode ? selectedNode.apps.filter((appId) => !bindingIdSet.has(normalizeTokenInput(appId))) : []),
    [bindingIdSet, selectedNode],
  )

  const pendingLinkSource = useMemo(
    () => (pendingLinkSourceId ? document.nodes.find((node) => node.id === pendingLinkSourceId) ?? null : null),
    [document.nodes, pendingLinkSourceId],
  )

  const flowNodes = useMemo<StoryCanvasNode[]>(
    () =>
      document.nodes.map((node) => ({
        id: node.id,
        type: 'storyNode',
        position: { x: node.x, y: node.y },
        data: {
          stepId: node.stepId,
          screenSceneId: node.screenSceneId,
          audioPackId: node.audioPackId,
          isInitial: node.isInitial,
          appsCount: node.apps.length,
          actionsCount: node.actions.length,
          pendingLinkCandidate: Boolean(pendingLinkSourceId && pendingLinkSourceId !== node.id),
          pendingLinkTarget: pendingLinkHoverNodeId === node.id,
        },
      })),
    [document.nodes, pendingLinkSourceId, pendingLinkHoverNodeId],
  )

  const flowEdges = useMemo<FlowEdge[]>(
    () =>
      document.edges.map((edge) => ({
        id: edge.id,
        source: edge.fromNodeId,
        target: edge.toNodeId,
        label: `${edge.eventType}:${edge.eventName}`,
        labelStyle: {
          fill: 'var(--ink-700)',
          fontSize: 11,
          fontWeight: 600,
        },
        style: {
          stroke: 'var(--ink-700)',
          strokeWidth: 2.2,
        },
        markerEnd: {
          type: MarkerType.ArrowClosed,
          color: 'var(--ink-700)',
        },
      })),
    [document.edges],
  )

  const importYamlTextToGraph = useCallback(
    (yamlText: string, successMessage: string) => {
      const imported = importStoryYamlToGraph(yamlText)
      setImportWarnings(imported.warnings)
      if (!imported.document || imported.errors.length > 0) {
        setErrors(imported.errors.length > 0 ? imported.errors : ['Import YAML invalide.'])
        setStatus('Import YAML échoué.')
        setStatusTone('error')
        return false
      }
      const nextDocument = autoLayoutStoryGraph(imported.document, {
        direction: 'LR',
        nodeWidth: NODE_WIDTH,
        nodeHeight: NODE_HEIGHT,
      })
      applyDocumentWithHistory(nextDocument, successMessage, 'success')
      setSelectedNodeId(null)
      setSelectedEdgeId(null)
      return true
    },
    [applyDocumentWithHistory],
  )

  const handleTemplateChange = useCallback(
    (templateKey: string) => {
      setSelectedTemplate(templateKey)
      if (!templateKey || !STORY_TEMPLATE_LIBRARY[templateKey]) {
        return
      }
      const templateYaml = STORY_TEMPLATE_LIBRARY[templateKey]
      setDraft(templateYaml)
      const imported = importYamlTextToGraph(templateYaml, `Template ${templateKey} chargé.`)
      if (!imported) {
        setStatus('Template chargé dans le YAML, mais import graphe incomplet.')
        setStatusTone('warning')
      }
    },
    [importYamlTextToGraph],
  )

  const handleImportFromYaml = useCallback(() => {
    importYamlTextToGraph(draft, 'YAML importé dans le graphe.')
  }, [draft, importYamlTextToGraph])

  const handleGenerateYaml = useCallback(() => {
    const generated = generateStoryYamlFromGraph(document)
    setDraft(generated)
    setErrors([])
    setStatus('YAML généré depuis le graphe.')
    setStatusTone('success')
  }, [document])

  const linkNodes = useCallback(
    (sourceNodeId: string, targetNodeId: string, defaultMessage = 'Lien créé.') => {
      if (!sourceNodeId || !targetNodeId) {
        return false
      }
      if (sourceNodeId === targetNodeId) {
        setStatus('Un lien doit pointer vers un node différent.')
        setStatusTone('warning')
        return false
      }

      const current = cloneDocument(documentRef.current)
      const sourceExists = current.nodes.some((node) => node.id === sourceNodeId)
      const targetExists = current.nodes.some((node) => node.id === targetNodeId)
      if (!sourceExists || !targetExists) {
        setStatus('Source ou cible introuvable pour créer le lien.')
        setStatusTone('error')
        return false
      }

      const existing = current.edges.find((edge) => edge.fromNodeId === sourceNodeId && edge.toNodeId === targetNodeId)
      if (existing) {
        existing.eventName = normalizeTokenInput(linkEventName || DEFAULT_EVENT_NAME)
        existing.eventType = linkEventType
        existing.trigger = DEFAULT_TRIGGER
        existing.afterMs = DEFAULT_AFTER_MS
        existing.priority = DEFAULT_PRIORITY
        applyDocumentWithHistory(current, 'Lien mis à jour.', 'success')
        setSelectedEdgeId(existing.id)
        setSelectedNodeId(null)
        return true
      }

      const edge: StoryEdge = {
        id: nextEdgeId(),
        fromNodeId: sourceNodeId,
        toNodeId: targetNodeId,
        trigger: DEFAULT_TRIGGER,
        eventType: linkEventType,
        eventName: normalizeTokenInput(linkEventName || DEFAULT_EVENT_NAME),
        afterMs: DEFAULT_AFTER_MS,
        priority: DEFAULT_PRIORITY,
      }
      current.edges.push(edge)
      applyDocumentWithHistory(current, defaultMessage, 'success')
      setSelectedEdgeId(edge.id)
      setSelectedNodeId(null)
      return true
    },
    [applyDocumentWithHistory, linkEventName, linkEventType],
  )

  const addNodeAtPosition = useCallback(
    (position?: { x: number; y: number }, message = 'Node ajouté.') => {
      const current = cloneDocument(documentRef.current)
      const node: StoryNode = {
        id: nextNodeId(current),
        stepId: nextStepId(current),
        screenSceneId: DEFAULT_SCENE_ID,
        audioPackId: '',
        actions: ['ACTION_TRACE_STEP'],
        apps: [...DEFAULT_NODE_APPS],
        mp3GateOpen: false,
        x: position ? Math.round(position.x) : 80 + (current.nodes.length % 3) * 300,
        y: position ? Math.round(position.y) : 80 + Math.floor(current.nodes.length / 3) * 220,
        isInitial: current.nodes.length === 0,
      }
      current.nodes.push(node)
      if (node.isInitial) {
        current.initialStep = node.stepId
      }
      applyDocumentWithHistory(current, message, 'success')
      setSelectedNodeId(node.id)
      setSelectedEdgeId(null)
      return node.id
    },
    [applyDocumentWithHistory],
  )

  const handleAddNode = useCallback(() => {
    addNodeAtPosition()
  }, [addNodeAtPosition])

  const addChildNodeFrom = useCallback(
    (sourceNodeId: string) => {
      const current = cloneDocument(documentRef.current)
      const source = current.nodes.find((node) => node.id === sourceNodeId)
      if (!source) {
        setStatus('Node source introuvable.')
        setStatusTone('error')
        return false
      }

      const childNode: StoryNode = {
        id: nextNodeId(current),
        stepId: nextStepId(current),
        screenSceneId: source.screenSceneId || DEFAULT_SCENE_ID,
        audioPackId: source.audioPackId || '',
        actions: source.actions.length > 0 ? [...source.actions] : ['ACTION_TRACE_STEP'],
        apps: source.apps.length > 0 ? [...source.apps] : [...DEFAULT_NODE_APPS],
        mp3GateOpen: source.mp3GateOpen,
        x: source.x + 320,
        y: source.y,
        isInitial: false,
      }

      const edge: StoryEdge = {
        id: nextEdgeId(),
        fromNodeId: source.id,
        toNodeId: childNode.id,
        trigger: DEFAULT_TRIGGER,
        eventType: linkEventType,
        eventName: normalizeTokenInput(linkEventName || DEFAULT_EVENT_NAME),
        afterMs: DEFAULT_AFTER_MS,
        priority: DEFAULT_PRIORITY,
      }

      current.nodes.push(childNode)
      current.edges.push(edge)
      applyDocumentWithHistory(current, 'Node enfant ajouté et lié.', 'success')
      setSelectedNodeId(childNode.id)
      setSelectedEdgeId(edge.id)
      return true
    },
    [applyDocumentWithHistory, linkEventName, linkEventType],
  )

  const handleAddChildNode = useCallback(() => {
    if (!selectedNode) {
      setStatus('Sélectionne un node pour créer un enfant.')
      setStatusTone('warning')
      return
    }
    addChildNodeFrom(selectedNode.id)
  }, [addChildNodeFrom, selectedNode])

  const resetLinkMode = useCallback(() => {
    setPendingLinkSourceId(null)
    setPendingLinkHoverNodeId(null)
  }, [])

  const commitPendingLink = useCallback(
    (targetId: string) => {
      if (!pendingLinkSourceId) {
        return false
      }

      const linked = linkNodes(pendingLinkSourceId, targetId)
      if (linked) {
        resetLinkMode()
      }
      return linked
    },
    [linkNodes, pendingLinkSourceId, resetLinkMode],
  )

  const handleAutoLayout = useCallback(() => {
    const current = cloneDocument(documentRef.current)
    const laidOut = autoLayoutStoryGraph(current, {
      direction: 'LR',
      nodeWidth: NODE_WIDTH,
      nodeHeight: NODE_HEIGHT,
    })
    applyDocumentWithHistory(laidOut, 'Auto-layout appliqué.', 'success')
  }, [applyDocumentWithHistory])

  const handleResetGraph = useCallback(() => {
    const fallback = createFallbackDocument()
    applyDocumentWithHistory(fallback, 'Graphe réinitialisé.', 'warning')
    setSelectedNodeId(null)
    setSelectedEdgeId(null)
    resetLinkMode()
  }, [applyDocumentWithHistory, resetLinkMode])

  const handleConnect = useCallback(
    (connection: Connection) => {
      if (!connection.source || !connection.target || connection.source === connection.target) {
        return
      }
      const created = linkNodes(connection.source, connection.target)
      if (created) {
        resetLinkMode()
      }
    },
    [linkNodes, resetLinkMode],
  )

  const handleNodeDragStop = useCallback(
    (_event: unknown, draggedNode: StoryCanvasNode) => {
      const current = cloneDocument(documentRef.current)
      const node = current.nodes.find((entry) => entry.id === draggedNode.id)
      if (!node) {
        return
      }
      node.x = Math.round(draggedNode.position.x)
      node.y = Math.round(draggedNode.position.y)
      applyDocumentWithHistory(current, 'Position du node mise à jour.', 'info')
    },
    [applyDocumentWithHistory],
  )

  const handleDeleteEdgeById = useCallback(
    (edgeId: string) => {
      const current = cloneDocument(documentRef.current)
      const exists = current.edges.some((edge) => edge.id === edgeId)
      if (!exists) {
        return false
      }
      current.edges = current.edges.filter((edge) => edge.id !== edgeId)
      applyDocumentWithHistory(current, 'Lien supprimé.', 'warning')
      setSelectedEdgeId(null)
      return true
    },
    [applyDocumentWithHistory],
  )

  const handleReverseEdgeDirection = useCallback(
    (edgeId: string) => {
      const current = cloneDocument(documentRef.current)
      const edge = current.edges.find((entry) => entry.id === edgeId)
      if (!edge) {
        return false
      }

      const hasReverse = current.edges.some(
        (entry) => entry.id !== edgeId && entry.fromNodeId === edge.toNodeId && entry.toNodeId === edge.fromNodeId,
      )
      if (hasReverse) {
        setStatus('Un lien inverse existe deja entre ces nodes.')
        setStatusTone('warning')
        return false
      }

      const previousSource = edge.fromNodeId
      edge.fromNodeId = edge.toNodeId
      edge.toNodeId = previousSource
      applyDocumentWithHistory(current, 'Direction du lien inversée.', 'success')
      setSelectedEdgeId(edgeId)
      setSelectedNodeId(null)
      return true
    },
    [applyDocumentWithHistory],
  )

  const handleDeleteNodeById = useCallback(
    (nodeId: string) => {
      const current = cloneDocument(documentRef.current)
      const node = current.nodes.find((entry) => entry.id === nodeId)
      if (!node) {
        return false
      }
      const deletedWasInitial = node.isInitial
      current.nodes = current.nodes.filter((entry) => entry.id !== nodeId)
      current.edges = current.edges.filter((edge) => edge.fromNodeId !== nodeId && edge.toNodeId !== nodeId)
      if (current.nodes.length > 0 && !current.nodes.some((entry) => entry.isInitial)) {
        current.nodes[0].isInitial = true
      }
      if (deletedWasInitial) {
        current.initialStep = current.nodes.find((entry) => entry.isInitial)?.stepId ?? ''
      }
      applyDocumentWithHistory(current, 'Node supprimé.', 'warning')
      if (pendingLinkSourceId === nodeId) {
        setPendingLinkSourceId(null)
      }
      setSelectedNodeId(null)
      return true
    },
    [applyDocumentWithHistory, pendingLinkSourceId],
  )

  const handleDeleteSelected = useCallback(() => {
    if (selectedNode) {
      handleDeleteNodeById(selectedNode.id)
      return
    }

    if (selectedEdge) {
      handleDeleteEdgeById(selectedEdge.id)
    }
  }, [handleDeleteEdgeById, handleDeleteNodeById, selectedEdge, selectedNode])

  useEffect(() => {
    const onKeyDown = (event: KeyboardEvent) => {
      if (event.key !== 'Delete' && event.key !== 'Backspace') {
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
      if (!selectedNode && !selectedEdge) {
        return
      }
      event.preventDefault()
      handleDeleteSelected()
    }
    window.addEventListener('keydown', onKeyDown)
    return () => window.removeEventListener('keydown', onKeyDown)
  }, [handleDeleteSelected, selectedEdge, selectedNode])

  const handleNodeClick = useCallback(
    (_event: ReactMouseEvent, node: StoryCanvasNode) => {
      setContextMenu(null)
      if (pendingLinkSourceId) {
        commitPendingLink(node.id)
        return
      }
      setSelectedNodeId(node.id)
      setSelectedEdgeId(null)
    },
    [commitPendingLink, pendingLinkSourceId],
  )

  const handleNodeMouseEnter = useCallback(
    (_event: ReactMouseEvent, node: StoryCanvasNode) => {
      if (!pendingLinkSourceId || node.id === pendingLinkSourceId) {
        return
      }
      setPendingLinkHoverNodeId(node.id)
    },
    [pendingLinkSourceId],
  )

  const handleNodeMouseLeave = useCallback(
    (_event: ReactMouseEvent, node: StoryCanvasNode) => {
      if (pendingLinkHoverNodeId === node.id) {
        setPendingLinkHoverNodeId(null)
      }
    },
    [pendingLinkHoverNodeId],
  )

  const handlePaneMouseMove = useCallback(
    () => {
      if (pendingLinkSourceId && pendingLinkHoverNodeId) {
        setPendingLinkHoverNodeId(null)
      }
    },
    [pendingLinkHoverNodeId, pendingLinkSourceId],
  )

  const handleNodeDoubleClick = useCallback(
    (_event: ReactMouseEvent, node: StoryCanvasNode) => {
      addChildNodeFrom(node.id)
    },
    [addChildNodeFrom],
  )

  const handleEdgeClick = useCallback((_event: ReactMouseEvent, edge: FlowEdge) => {
    setContextMenu(null)
    setSelectedEdgeId(edge.id)
    setSelectedNodeId(null)
  }, [])

  const handleNodeContextMenu = useCallback(
    (event: ReactMouseEvent, node: StoryCanvasNode) => {
      event.preventDefault()
      event.stopPropagation()
      if (pendingLinkSourceId && pendingLinkSourceId !== node.id) {
        const linked = commitPendingLink(node.id)
        if (linked) {
          setStatus(
            `Lien créé entre ${
              nodeLookup.get(pendingLinkSourceId)?.stepId ?? pendingLinkSourceId
            } → ${node.data.stepId}.`,
            )
          setStatusTone('success')
        } else {
          setStatus('Impossible de créer la liaison via clic-droit.')
          setStatusTone('error')
        }
        return
      }

      const position = clampContextPosition(event.clientX, event.clientY)
      setSelectedNodeId(node.id)
      setSelectedEdgeId(null)
      setContextMenu({
        kind: 'node',
        x: position.x,
        y: position.y,
        nodeId: node.id,
      })
    },
    [commitPendingLink, nodeLookup, pendingLinkSourceId],
  )

  const handleEdgeContextMenu = useCallback((event: ReactMouseEvent, edge: FlowEdge) => {
    event.preventDefault()
    event.stopPropagation()
    const position = clampContextPosition(event.clientX, event.clientY)
    setSelectedNodeId(null)
    setSelectedEdgeId(edge.id)
    setContextMenu({
      kind: 'edge',
      x: position.x,
      y: position.y,
      edgeId: edge.id,
    })
  }, [])

  const handlePaneContextMenu = useCallback((event: ReactMouseEvent | MouseEvent) => {
    event.preventDefault()
    event.stopPropagation()
    if (pendingLinkSourceId) {
      resetLinkMode()
      setStatus('Liaison annulée.')
      setStatusTone('info')
    }
    const screenX = Number.isFinite(event.clientX) ? event.clientX : window.innerWidth * 0.5
    const screenY = Number.isFinite(event.clientY) ? event.clientY : window.innerHeight * 0.5
    const position = clampContextPosition(screenX, screenY)
    const flowPosition = flowInstanceRef.current?.screenToFlowPosition({
      x: screenX,
      y: screenY,
    })
    setContextMenu({
      kind: 'pane',
      x: position.x,
      y: position.y,
      flowX: flowPosition?.x ?? 120,
      flowY: flowPosition?.y ?? 120,
    })
  }, [pendingLinkSourceId, resetLinkMode])

  const handleAddBinding = useCallback(() => {
    const current = cloneDocument(documentRef.current)
    current.appBindings.push({ id: nextBindingId(current), app: 'SCREEN_SCENE' })
    applyDocumentWithHistory(current, 'Binding ajouté.', 'success')
  }, [applyDocumentWithHistory])

  const handleDuplicateBinding = useCallback(
    (binding: AppBinding) => {
      const current = cloneDocument(documentRef.current)
      const nextId = nextBindingId(current)
      current.appBindings.push({
        ...binding,
        id: nextId,
        config: binding.config ? { ...binding.config } : undefined,
      })
      applyDocumentWithHistory(current, 'Binding dupliqué.', 'success')
    },
    [applyDocumentWithHistory],
  )

  const handleDeleteBinding = useCallback(
    (bindingId: string) => {
      const current = cloneDocument(documentRef.current)
      current.appBindings = current.appBindings.filter((binding) => binding.id !== bindingId)
      current.nodes = current.nodes.map((node) => ({
        ...node,
        apps: node.apps.filter((appId) => appId !== bindingId),
      }))
      applyDocumentWithHistory(current, 'Binding supprimé.', 'warning')
    },
    [applyDocumentWithHistory],
  )

  const updateBindingConfig = useCallback(
    (bindingId: string, key: string, value: string | number | boolean | undefined) => {
      setDocument((current) => ({
        ...current,
        appBindings: current.appBindings.map((entry) =>
          entry.id === bindingId
            ? {
                ...entry,
                config: withConfigValue(entry.config, key, value),
              }
            : entry,
        ),
      }))
    },
    [],
  )

  const runAsyncAction = useCallback(
    async (
      action: 'validate' | 'deploy' | 'test',
      callback: () => Promise<void>,
      fallbackError: string,
      capabilityMessage: string,
      enabled: boolean,
    ) => {
      if (!enabled) {
        setStatus(capabilityMessage)
        setStatusTone('warning')
        return
      }
      setBusyAction(action)
      setErrors([])
      try {
        await callback()
      } catch (error) {
        setStatus(error instanceof Error ? error.message : fallbackError)
        setStatusTone('error')
      } finally {
        setBusyAction(null)
      }
    },
    [],
  )

  const handleValidate = useCallback(
    async () =>
      runAsyncAction(
        'validate',
        async () => {
          const result = await onValidate(draft)
          if (result.valid) {
            setStatus('Validation réussie.')
            setStatusTone('success')
            return
          }
          const nextErrors = result.errors ?? ['YAML invalide.']
          setErrors(nextErrors)
          setStatus('Validation en erreur.')
          setStatusTone('error')
        },
        'Validation impossible.',
        'Validation indisponible en mode legacy.',
        validateEnabled,
      ),
    [draft, onValidate, runAsyncAction, validateEnabled],
  )

  const handleDeploy = useCallback(
    async () =>
      runAsyncAction(
        'deploy',
        async () => {
          const result = await onDeploy(draft)
          if (result.status === 'ok') {
            setStatus(`Déploiement réussi${result.deployed ? ` (${result.deployed})` : ''}.`)
            setStatusTone('success')
            return
          }
          setStatus(result.message ?? 'Déploiement en erreur.')
          setStatusTone('error')
        },
        'Déploiement impossible.',
        'Déploiement indisponible en mode legacy.',
        deployEnabled,
      ),
    [deployEnabled, draft, onDeploy, runAsyncAction],
  )

  const handleTestRun = useCallback(
    async () =>
      runAsyncAction(
        'test',
        async () => {
          await onTestRun(draft)
          setStatus('Test run lancé (30 secondes).')
          setStatusTone('success')
        },
        'Test run impossible.',
        'Test run disponible uniquement avec les APIs Story V2 select/start/deploy.',
        testRunEnabled,
      ),
    [draft, onTestRun, runAsyncAction, testRunEnabled],
  )

  const editorExtensions = useMemo(() => [yamlLanguage()], [])
  const contextNode = useMemo(
    () => (contextMenu?.kind === 'node' && contextMenu.nodeId ? nodeLookup.get(contextMenu.nodeId) ?? null : null),
    [contextMenu, nodeLookup],
  )
  const contextEdge = useMemo(
    () => (contextMenu?.kind === 'edge' && contextMenu.edgeId ? document.edges.find((edge) => edge.id === contextMenu.edgeId) ?? null : null),
    [contextMenu, document.edges],
  )

  useEffect(() => {
    setActionDraft('')
  }, [selectedNodeId])

  const addActionToSelectedNode = useCallback(
    (rawAction: string) => {
      const action = normalizeTokenInput(rawAction)
      if (!action || !selectedNodeId) {
        return
      }
      applyDocumentWithHistory(
        {
          ...documentRef.current,
          nodes: documentRef.current.nodes.map((entry) =>
            entry.id === selectedNodeId
              ? {
                  ...entry,
                  actions: entry.actions.includes(action) ? entry.actions : [...entry.actions, action],
                }
              : entry,
          ),
        },
        'Action ajoutée.',
        'success',
      )
      setActionDraft('')
    },
    [applyDocumentWithHistory, selectedNodeId],
  )

  const removeActionFromSelectedNode = useCallback(
    (actionToDelete: string) => {
      if (!selectedNodeId) {
        return
      }
      applyDocumentWithHistory(
        {
          ...documentRef.current,
          nodes: documentRef.current.nodes.map((entry) =>
            entry.id === selectedNodeId
              ? {
                  ...entry,
                  actions: entry.actions.filter((action) => action !== actionToDelete),
                }
              : entry,
          ),
        },
        'Action supprimée.',
        'info',
      )
    },
    [applyDocumentWithHistory, selectedNodeId],
  )

  const handleActionDraftSubmit = useCallback(() => {
    addActionToSelectedNode(actionDraft)
  }, [actionDraft, addActionToSelectedNode])

  return (
    <section className="space-y-6">
      <SectionHeader
        title="Designer de story"
        subtitle="Édite ton scénario en nodal, puis synchronise le graphe via l'éditeur YAML."
        actions={
          <>
            <Field label="Template" htmlFor="story-template" className="min-w-[210px]">
              <select
                id="story-template"
                value={selectedTemplate}
                onChange={(event) => handleTemplateChange(event.target.value)}
                className="story-input mt-2"
              >
                <option value="">Choisir un template</option>
                {Object.keys(STORY_TEMPLATE_LIBRARY).map((templateKey) => (
                  <option key={templateKey} value={templateKey}>
                    {templateKey}
                  </option>
                ))}
              </select>
            </Field>
            <Button variant="outline" size="sm" onClick={handleImportFromYaml}>
              Import YAML → Graphe
            </Button>
            <Button variant="primary" size="sm" onClick={handleGenerateYaml}>
              Export Graphe → YAML
            </Button>
          </>
        }
      />

      <InlineNotice tone="info">
        Import YAML se fait depuis l'éditeur YAML (copier-coller), puis bouton <strong>Import YAML → Graphe</strong>.
      </InlineNotice>

      <Panel>
        <div className="mb-2 flex flex-wrap items-center gap-2 text-xs text-[var(--ink-500)]">
          <span className="rounded-full border border-[var(--mist-400)] bg-white/70 px-2 py-1">
            Lien: clic-glisser ou clic-droit
          </span>
          <span className="rounded-full border border-[var(--mist-400)] bg-white/70 px-2 py-1">Raccourci: Ctrl/Cmd + L</span>
          <span className="rounded-full border border-[var(--mist-400)] bg-white/70 px-2 py-1">Annuler: Echap</span>
          <span className="rounded-full border border-[var(--mist-400)] bg-white/70 px-2 py-1">
            Edition rapide: double-clic sur node
          </span>
          <span className="rounded-full border border-[var(--mist-400)] bg-white/70 px-2 py-1">
            Historique: Ctrl/Cmd+Z
          </span>
        </div>

        <div
          role="toolbar"
          aria-label="Actions graphe"
          className="grid gap-3 md:grid-cols-[minmax(0,1fr)_repeat(8,auto)] md:items-end"
        >
          <Field label="Scenario ID" htmlFor="scenario-id">
            <input
              id="scenario-id"
              value={document.scenarioId}
              onFocus={pushHistorySnapshot}
              onChange={(event) =>
                setDocument((current) => ({
                  ...current,
                  scenarioId: normalizeTokenInput(event.target.value),
                }))
              }
              className="story-input mt-2"
            />
          </Field>

          <Button variant="outline" onClick={handleAddNode}>
            Ajouter node
          </Button>
          <Button variant="outline" onClick={handleAddChildNode}>
            Ajouter enfant
          </Button>
          <Button
            variant="outline"
            onClick={() => {
              if (selectedNode) {
                setPendingLinkSourceId(selectedNode.id)
                setStatus(`Liaison démarrée depuis ${selectedNode.stepId}.`)
                setStatusTone('info')
              } else {
                setStatus("Sélectionne d'abord un node source.")
                setStatusTone('warning')
              }
            }}
          >
            Lancer liaison
          </Button>
          <Button variant="outline" onClick={handleAutoLayout}>
            Auto-layout
          </Button>
          <Button variant="outline" onClick={handleUndo} disabled={!canUndo}>
            Annuler
          </Button>
          <Button variant="outline" onClick={handleRedo} disabled={!canRedo}>
            Retablir
          </Button>
          <Button variant="outline" onClick={handleDeleteSelected} disabled={!selectedNode && !selectedEdge}>
            Supprimer sélection
          </Button>
          <Button variant="outline" onClick={() => setPendingLinkSourceId(null)} disabled={!pendingLinkSourceId}>
            Annuler liaison
          </Button>
          <Button variant="ghost" onClick={handleResetGraph}>
            Reinitialiser
          </Button>
        </div>

        <div className="mt-4 grid gap-3 md:grid-cols-2">
          <Field label="Event par défaut (nouveau lien)">
            <input
              value={linkEventName}
              onChange={(event) => {
                const nextName = normalizeTokenInput(event.target.value)
                setLinkEventName(nextName)
                setLinkEventType(inferEventTypeFromName(nextName))
              }}
              className="story-input mt-2 min-h-[40px]"
            />
          </Field>
          <Field label="Event type par défaut">
            <select
              value={linkEventType}
              onChange={(event) => setLinkEventType(event.target.value as EventType)}
              className="story-input mt-2 min-h-[40px]"
            >
              <option value="action">action</option>
              <option value="unlock">unlock</option>
              <option value="audio_done">audio_done</option>
              <option value="timer">timer</option>
              <option value="serial">serial</option>
              <option value="none">none</option>
            </select>
          </Field>
        </div>

        <div className="mt-4 flex flex-wrap gap-2">
          <Badge tone="info">Nodes: {document.nodes.length}</Badge>
          <Badge tone="info">Liens: {document.edges.length}</Badge>
          <Badge tone="neutral">Initial: {document.initialStep || 'auto'}</Badge>
          {graphValidation.errors.length > 0 ? <Badge tone="error">Erreurs: {graphValidation.errors.length}</Badge> : null}
          {graphValidation.warnings.length > 0 ? (
            <Badge tone="warning">Avertissements: {graphValidation.warnings.length}</Badge>
          ) : null}
        </div>

        {pendingLinkSource ? (
          <InlineNotice className="mt-3" tone="warning">
            Liaison en attente depuis <strong>{pendingLinkSource.stepId}</strong>. Cible un node avec un clic ou un clic
            droit pour finaliser la connexion.
          </InlineNotice>
        ) : null}

        {graphValidation.errors.length === 0 && graphValidation.warnings.length === 0 ? (
          <InlineNotice className="mt-3" tone="success">
            Graphe sain: aucune incohérence détectée.
          </InlineNotice>
        ) : null}
      </Panel>

      <div className="flex gap-2 lg:hidden">
        <Button variant={activeTab === 'graph' ? 'primary' : 'outline'} onClick={() => setActiveTab('graph')}>
          Graphe
        </Button>
        <Button variant={activeTab === 'bindings' ? 'primary' : 'outline'} onClick={() => setActiveTab('bindings')}>
          Bindings
        </Button>
        <Button variant={activeTab === 'yaml' ? 'primary' : 'outline'} onClick={() => setActiveTab('yaml')}>
          YAML
        </Button>
      </div>

      <div className="grid gap-6 xl:grid-cols-[1.45fr_1fr]">
        <Panel className={activeTab !== 'graph' ? 'hidden lg:block' : ''}>
          <div className="mb-3 flex flex-wrap gap-2">
            <Badge tone="neutral">Drag & drop: nodes</Badge>
            <Badge tone="neutral">Click edge: edition transition</Badge>
            <Badge tone="neutral">Cmd/Ctrl+Z: undo</Badge>
          </div>
          <div className="h-[62vh] min-h-[520px] rounded-2xl border border-white/70 bg-white/60">
            <ReactFlow
              className="story-flow"
              nodes={flowNodes}
              edges={flowEdges}
              nodeTypes={flowNodeTypes}
              onPaneMouseMove={handlePaneMouseMove}
              fitView
              onInit={(instance) => {
                flowInstanceRef.current = instance
              }}
              onConnect={handleConnect}
              onNodeClick={handleNodeClick}
              onNodeDoubleClick={handleNodeDoubleClick}
              onNodeMouseEnter={handleNodeMouseEnter}
              onNodeMouseLeave={handleNodeMouseLeave}
              onEdgeClick={handleEdgeClick}
              onNodeContextMenu={handleNodeContextMenu}
              onEdgeContextMenu={handleEdgeContextMenu}
              onPaneContextMenu={handlePaneContextMenu}
              onPaneClick={(event) => {
                if (event.detail >= 2) {
                  const flowPosition = flowInstanceRef.current?.screenToFlowPosition({
                    x: Number.isFinite(event.clientX) ? event.clientX : window.innerWidth * 0.5,
                    y: Number.isFinite(event.clientY) ? event.clientY : window.innerHeight * 0.5,
                  }) ?? { x: 120, y: 120 }

                  addNodeAtPosition(flowPosition, 'Node ajouté au double-clic.')
                  return
                }

                setContextMenu(null)
                if (pendingLinkSourceId) {
                  setStatus('Liaison annulée.')
                  setStatusTone('warning')
                  resetLinkMode()
                }
                setSelectedNodeId(null)
                setSelectedEdgeId(null)
              }}
              onNodeDragStop={handleNodeDragStop}
              proOptions={{ hideAttribution: true }}
            >
              <MiniMap pannable zoomable />
              <Controls showInteractive={false} />
              <Background gap={20} size={1.2} />
            </ReactFlow>
          </div>
        </Panel>

        <div className="space-y-6">
          <Panel className={activeTab !== 'bindings' ? 'hidden lg:block' : ''}>
            <SectionHeader
              title="Bindings Apps"
              subtitle="Édition guidée des app_bindings avec config contextuelle."
              actions={
                <Button variant="outline" size="sm" onClick={handleAddBinding}>
                  Ajouter
                </Button>
              }
            />

            <div className="soft-scrollbar mt-4 max-h-[58vh] space-y-3 overflow-y-auto pr-1">
              {document.appBindings.map((binding) => (
                <div key={binding.id} className="rounded-2xl border border-[var(--mist-400)] bg-white/80 p-3">
                  {duplicateBindingIds.has(normalizeTokenInput(binding.id)) ? (
                    <InlineNotice tone="warning" className="mb-2">
                      L'ID de ce binding est dupliqué.
                    </InlineNotice>
                  ) : null}
                  <div className="grid gap-2 sm:grid-cols-[1fr_180px_auto_auto] sm:items-end">
                    <Field label="ID">
                      <input
                        value={binding.id}
                        onFocus={pushHistorySnapshot}
                        onChange={(event) => {
                          const nextId = normalizeTokenInput(event.target.value)
                          setDocument((current) => ({
                            ...current,
                            appBindings: current.appBindings.map((entry) =>
                              entry.id === binding.id
                                ? {
                                    ...entry,
                                    id: nextId,
                                  }
                                : entry,
                            ),
                            nodes: current.nodes.map((node) => ({
                              ...node,
                              apps: node.apps.map((appId) => (appId === binding.id ? nextId : appId)),
                            })),
                          }))
                        }}
                        className="story-input mt-2 min-h-[38px]"
                      />
                    </Field>

                    <Field label="App">
                      <select
                        value={binding.app}
                        onFocus={pushHistorySnapshot}
                        onChange={(event) => {
                          const nextApp = event.target.value as AppBinding['app']
                          setDocument((current) => ({
                            ...current,
                            appBindings: current.appBindings.map((entry) =>
                              entry.id === binding.id
                                ? {
                                    ...entry,
                                    app: nextApp,
                                    config:
                                      nextApp === 'LA_DETECTOR'
                                        ? {
                                            ...(entry.config ?? {}),
                                            hold_ms: entry.config?.hold_ms ?? 3000,
                                            unlock_event: entry.config?.unlock_event ?? 'UNLOCK',
                                            require_listening: entry.config?.require_listening ?? true,
                                          }
                                        : stripLaDetectorConfig(entry.config),
                                  }
                                : entry,
                            ),
                          }))
                        }}
                        className="story-input mt-2 min-h-[38px]"
                      >
                        <option value="LA_DETECTOR">LA_DETECTOR</option>
                        <option value="AUDIO_PACK">AUDIO_PACK</option>
                        <option value="SCREEN_SCENE">SCREEN_SCENE</option>
                        <option value="MP3_GATE">MP3_GATE</option>
                        <option value="WIFI_STACK">WIFI_STACK</option>
                        <option value="ESPNOW_STACK">ESPNOW_STACK</option>
                      </select>
                    </Field>

                    <Button variant="outline" size="sm" onClick={() => handleDuplicateBinding(binding)}>
                      Dupliquer
                    </Button>
                    <Button variant="danger" size="sm" onClick={() => handleDeleteBinding(binding.id)}>
                      Supprimer
                    </Button>
                  </div>

                  {binding.app === 'LA_DETECTOR' ? (
                    <div className="mt-3 grid gap-2 sm:grid-cols-3">
                      <Field label="hold_ms">
                        <input
                          type="number"
                          min={100}
                          max={60000}
                          value={binding.config?.hold_ms ?? 3000}
                          onFocus={pushHistorySnapshot}
                          onChange={(event) => {
                            const nextValue = Number.parseInt(event.target.value || '3000', 10)
                            updateBindingConfig(
                              binding.id,
                              'hold_ms',
                              Number.isFinite(nextValue) ? Math.max(100, Math.min(60000, nextValue)) : 3000,
                            )
                          }}
                          className="story-input mt-2 min-h-[38px]"
                        />
                      </Field>

                      <Field label="unlock_event">
                        <input
                          value={binding.config?.unlock_event ?? 'UNLOCK'}
                          onFocus={pushHistorySnapshot}
                          onChange={(event) => {
                            updateBindingConfig(binding.id, 'unlock_event', normalizeTokenInput(event.target.value))
                          }}
                          className="story-input mt-2 min-h-[38px]"
                        />
                      </Field>

                      <Field label="require_listening">
                        <label className="mt-2 flex min-h-[38px] items-center gap-2 rounded-lg border border-[var(--mist-400)] bg-white px-3 text-sm text-[var(--ink-700)]">
                          <input
                            type="checkbox"
                            checked={binding.config?.require_listening ?? true}
                            onFocus={pushHistorySnapshot}
                            onChange={(event) => {
                              updateBindingConfig(binding.id, 'require_listening', event.target.checked)
                            }}
                            className="h-4 w-4 accent-[var(--teal-500)]"
                          />
                          <span>{binding.config?.require_listening ?? true ? 'true' : 'false'}</span>
                        </label>
                      </Field>
                    </div>
                  ) : (SIMPLE_BINDING_FIELDS[binding.app] ?? []).length > 0 ? (
                    <div className="mt-3 rounded-2xl border border-[var(--mist-400)] bg-white/70 p-3">
                      <p className="text-[11px] uppercase tracking-[0.14em] text-[var(--ink-500)]">
                        Config simple ({binding.app})
                      </p>
                      <div className="mt-2 grid gap-2 sm:grid-cols-2">
                        {(SIMPLE_BINDING_FIELDS[binding.app] ?? []).map((field) => {
                          if (field.type === 'boolean') {
                            const checked = Boolean(binding.config?.[field.key] ?? field.defaultValue)
                            return (
                              <Field key={field.key} label={field.label}>
                                <label className="mt-2 flex min-h-[38px] items-center gap-2 rounded-lg border border-[var(--mist-400)] bg-white px-3 text-sm text-[var(--ink-700)]">
                                  <input
                                    type="checkbox"
                                    checked={checked}
                                    onFocus={pushHistorySnapshot}
                                    onChange={(event) => {
                                      updateBindingConfig(binding.id, field.key, event.target.checked)
                                    }}
                                    className="h-4 w-4 accent-[var(--teal-500)]"
                                  />
                                  <span>{checked ? 'true' : 'false'}</span>
                                </label>
                              </Field>
                            )
                          }

                          if (field.type === 'number') {
                            const currentValue = binding.config?.[field.key]
                            const normalizedValue =
                              typeof currentValue === 'number'
                                ? currentValue
                                : typeof currentValue === 'string'
                                  ? Number.parseInt(currentValue, 10)
                                  : Number(field.defaultValue)
                            return (
                              <Field key={field.key} label={field.label}>
                                <input
                                  type="number"
                                  min={field.min}
                                  max={field.max}
                                  value={Number.isFinite(normalizedValue) ? normalizedValue : Number(field.defaultValue)}
                                  onFocus={pushHistorySnapshot}
                                  onChange={(event) => {
                                    const parsed = Number.parseInt(event.target.value || `${field.defaultValue}`, 10)
                                    const min = field.min ?? Number.MIN_SAFE_INTEGER
                                    const max = field.max ?? Number.MAX_SAFE_INTEGER
                                    const clamped = Number.isFinite(parsed)
                                      ? Math.max(min, Math.min(max, parsed))
                                      : Number(field.defaultValue)
                                    updateBindingConfig(binding.id, field.key, clamped)
                                  }}
                                  className="story-input mt-2 min-h-[38px]"
                                />
                              </Field>
                            )
                          }

                          return (
                            <Field key={field.key} label={field.label}>
                              <input
                                value={String(binding.config?.[field.key] ?? field.defaultValue)}
                                onFocus={pushHistorySnapshot}
                                onChange={(event) => {
                                  const nextValue = event.target.value.trim()
                                  updateBindingConfig(binding.id, field.key, nextValue || String(field.defaultValue))
                                }}
                                className="story-input mt-2 min-h-[38px]"
                              />
                            </Field>
                          )
                        })}
                      </div>
                    </div>
                  ) : (
                    <InlineNotice className="mt-3" tone="info">
                      Cette app ne nécessite pas de configuration guidée avancée.
                    </InlineNotice>
                  )}
                </div>
              ))}
            </div>
          </Panel>

          <Panel>
            <SectionHeader
              title="Édition sélection"
              subtitle="Édite un node ou un lien sélectionné dans le graphe."
            />

            {selectedNode ? (
              <div className="mt-4 space-y-3">
                <div className="flex flex-wrap gap-2">
                  <Badge tone="success">Node sélectionné</Badge>
                  <Badge tone="neutral">{selectedNode.id}</Badge>
                </div>
                <Field label="Step ID">
                  <input
                    value={selectedNode.stepId}
                    onFocus={pushHistorySnapshot}
                    onChange={(event) => {
                      const nextStep = normalizeTokenInput(event.target.value)
                      setDocument((current) => ({
                        ...current,
                        initialStep: current.initialStep === selectedNode.stepId ? nextStep : current.initialStep,
                        nodes: current.nodes.map((node) =>
                          node.id === selectedNode.id
                            ? {
                                ...node,
                                stepId: nextStep,
                              }
                            : node,
                        ),
                      }))
                    }}
                    className="story-input mt-2 min-h-[40px]"
                  />
                </Field>

                <Field label="Scene">
                  <input
                    value={selectedNode.screenSceneId}
                    onFocus={pushHistorySnapshot}
                    onChange={(event) => {
                      const nextScene = normalizeTokenInput(event.target.value)
                      setDocument((current) => ({
                        ...current,
                        nodes: current.nodes.map((node) =>
                          node.id === selectedNode.id
                            ? {
                                ...node,
                                screenSceneId: nextScene,
                              }
                            : node,
                        ),
                      }))
                    }}
                    className="story-input mt-2 min-h-[40px]"
                  />
                </Field>

                <Field label="Audio pack">
                  <input
                    value={selectedNode.audioPackId}
                    onFocus={pushHistorySnapshot}
                    onChange={(event) => {
                      const nextAudio = normalizeTokenInput(event.target.value)
                      setDocument((current) => ({
                        ...current,
                        nodes: current.nodes.map((node) =>
                          node.id === selectedNode.id
                            ? {
                                ...node,
                                audioPackId: nextAudio,
                              }
                            : node,
                        ),
                      }))
                    }}
                    className="story-input mt-2 min-h-[40px]"
                  />
                </Field>

                <Field label="Actions (tokens)">
                  <div className="mt-2 flex flex-wrap gap-2">
                    {(selectedNode.actions.length === 0 ? ['Aucune action'] : selectedNode.actions).map((action) => {
                      if (action === 'Aucune action') {
                        return (
                          <span
                            key={action}
                            className="rounded-full border border-dashed border-[var(--mist-400)] bg-white/65 px-3 py-1 text-[11px] text-[var(--ink-500)]"
                          >
                            Aucune action
                          </span>
                        )
                      }

                      return (
                        <button
                          type="button"
                          key={action}
                          className="group inline-flex items-center gap-1 rounded-full border border-[var(--accent-500)] bg-white/75 px-2.5 py-1 text-[10px] font-semibold uppercase tracking-[0.1em] text-[var(--accent-800)]"
                          onClick={() => {
                            removeActionFromSelectedNode(action)
                          }}
                          title="Supprimer cette action"
                        >
                          {action}
                          <span className="text-[12px] text-[var(--accent-700)] transition group-hover:scale-110">×</span>
                        </button>
                      )
                    })}
                  </div>
                  <div className="mt-3 flex gap-2">
                    <input
                      value={actionDraft}
                      placeholder="NOUVELLE_ACTION"
                      onChange={(event) => {
                        setActionDraft(event.target.value.toUpperCase())
                      }}
                      onKeyDown={(event) => {
                        if (event.key === 'Enter') {
                          event.preventDefault()
                          handleActionDraftSubmit()
                        }
                      }}
                      className="story-input min-h-[40px] flex-1"
                    />
                    <Button
                      type="button"
                      size="sm"
                      onClick={() => {
                        handleActionDraftSubmit()
                      }}
                    >
                      Ajouter
                    </Button>
                  </div>
                  <p className="mt-2 text-xs text-[var(--ink-500)]">
                    Clique une action pour la retirer. Valide ou appuie sur Entrée pour ajouter.
                  </p>
                </Field>

                <div className="space-y-2 rounded-2xl border border-[var(--mist-400)] bg-white/70 p-3">
                  <p className="text-xs uppercase tracking-[0.16em] text-[var(--ink-500)]">Apps liées</p>
                  {selectedNodeMissingBindings.length > 0 ? (
                    <InlineNotice tone="warning">
                      {`Bindings non définis: ${selectedNodeMissingBindings.join(', ')}.`}
                    </InlineNotice>
                  ) : null}
                  <div className="grid gap-2 sm:grid-cols-2">
                    {document.appBindings.map((binding) => {
                      const checked = selectedNode.apps.includes(binding.id)
                      return (
                        <label key={binding.id} className="flex items-center gap-2 text-sm text-[var(--ink-700)]">
                          <input
                            type="checkbox"
                            checked={checked}
                            onFocus={pushHistorySnapshot}
                            onChange={(event) => {
                              const nextChecked = event.target.checked
                              setDocument((current) => ({
                                ...current,
                                nodes: current.nodes.map((node) => {
                                  if (node.id !== selectedNode.id) {
                                    return node
                                  }
                                  const existing = new Set(node.apps)
                                  if (nextChecked) {
                                    existing.add(binding.id)
                                  } else {
                                    existing.delete(binding.id)
                                  }
                                  return {
                                    ...node,
                                    apps: Array.from(existing),
                                  }
                                }),
                              }))
                            }}
                            className="h-4 w-4 accent-[var(--teal-500)]"
                          />
                          <span>{binding.id}</span>
                        </label>
                      )
                    })}
                  </div>
                </div>

                <div className="space-y-2 rounded-2xl border border-[var(--mist-400)] bg-white/70 p-3">
                  <div className="flex flex-wrap items-center justify-between gap-2">
                    <p className="text-xs uppercase tracking-[0.16em] text-[var(--ink-500)]">Liens sortants</p>
                    <Button
                      variant="outline"
                      size="sm"
                      onClick={() => {
                        setPendingLinkSourceId(selectedNode.id)
                        setStatus(`Liaison armée depuis ${selectedNode.stepId}. Clique un node cible.`)
                        setStatusTone('info')
                      }}
                    >
                      Démarrer liaison
                    </Button>
                  </div>
                  {selectedNodeOutgoingEdges.length === 0 ? (
                    <p className="text-sm text-[var(--ink-500)]">Aucun lien sortant.</p>
                  ) : (
                    <div className="space-y-2">
                      {selectedNodeOutgoingEdges.map((edge) => {
                        const targetNode = nodeLookup.get(edge.toNodeId)
                        return (
                          <div
                            key={edge.id}
                            className="grid gap-2 rounded-xl border border-[var(--mist-400)] bg-white/80 p-2 sm:grid-cols-[1fr_auto_auto]"
                          >
                            <p className="text-xs text-[var(--ink-700)]">
                              {edge.eventType}:{edge.eventName} → {targetNode?.stepId ?? edge.toNodeId}
                            </p>
                            <Button
                              size="sm"
                              variant="ghost"
                              onClick={() => {
                                setSelectedNodeId(null)
                                setSelectedEdgeId(edge.id)
                              }}
                            >
                              Éditer
                            </Button>
                            <Button
                              size="sm"
                              variant="danger"
                              onClick={() => {
                                handleDeleteEdgeById(edge.id)
                              }}
                            >
                              Supprimer
                            </Button>
                          </div>
                        )
                      })}
                    </div>
                  )}
                </div>

                <div className="flex flex-wrap gap-2">
                  <Button
                    variant={selectedNode.isInitial ? 'primary' : 'outline'}
                    onClick={() => {
                      pushHistorySnapshot()
                      setDocument((current) => ({
                        ...current,
                        initialStep: selectedNode.stepId,
                        nodes: current.nodes.map((node) => ({
                          ...node,
                          isInitial: node.id === selectedNode.id,
                        })),
                      }))
                    }}
                  >
                    {selectedNode.isInitial ? 'Node initial' : 'Définir initial'}
                  </Button>

                  <label className="inline-flex items-center gap-2 rounded-full border border-[var(--mist-400)] px-3 text-sm text-[var(--ink-700)]">
                    <input
                      type="checkbox"
                      checked={selectedNode.mp3GateOpen}
                      onFocus={pushHistorySnapshot}
                      onChange={(event) => {
                        const nextChecked = event.target.checked
                        setDocument((current) => ({
                          ...current,
                          nodes: current.nodes.map((node) =>
                            node.id === selectedNode.id
                              ? {
                                  ...node,
                                  mp3GateOpen: nextChecked,
                                }
                              : node,
                          ),
                        }))
                      }}
                      className="h-4 w-4 accent-[var(--teal-500)]"
                    />
                    MP3 gate open
                  </label>
                </div>
              </div>
            ) : null}

            {!selectedNode && selectedEdge ? (
              <div className="mt-4 space-y-3">
                <div className="flex flex-wrap gap-2">
                  <Badge tone="warning">Lien sélectionné</Badge>
                  <Badge tone="neutral">{selectedEdge.id}</Badge>
                </div>
                <div className="flex flex-wrap gap-2">
                  <Button
                    size="sm"
                    variant="outline"
                    onClick={() => {
                      handleReverseEdgeDirection(selectedEdge.id)
                    }}
                  >
                    Inverser sens
                  </Button>
                  <Button
                    size="sm"
                    variant="danger"
                    onClick={() => {
                      handleDeleteEdgeById(selectedEdge.id)
                    }}
                  >
                    Supprimer lien
                  </Button>
                </div>
                <Field label="trigger">
                  <select
                    value={selectedEdge.trigger}
                    onFocus={pushHistorySnapshot}
                    onChange={(event) => {
                      const trigger = event.target.value as StoryEdge['trigger']
                      setDocument((current) => ({
                        ...current,
                        edges: current.edges.map((edge) =>
                          edge.id === selectedEdge.id
                            ? {
                                ...edge,
                                trigger,
                              }
                            : edge,
                        ),
                      }))
                    }}
                    className="story-input mt-2 min-h-[40px]"
                  >
                    <option value="on_event">on_event</option>
                    <option value="after_ms">after_ms</option>
                    <option value="immediate">immediate</option>
                  </select>
                </Field>

                <Field label="event_type">
                  <select
                    value={selectedEdge.eventType}
                    onFocus={pushHistorySnapshot}
                    onChange={(event) => {
                      const eventType = event.target.value as StoryEdge['eventType']
                      setDocument((current) => ({
                        ...current,
                        edges: current.edges.map((edge) =>
                          edge.id === selectedEdge.id
                            ? {
                                ...edge,
                                eventType,
                              }
                            : edge,
                        ),
                      }))
                    }}
                    className="story-input mt-2 min-h-[40px]"
                  >
                    <option value="none">none</option>
                    <option value="unlock">unlock</option>
                    <option value="audio_done">audio_done</option>
                    <option value="timer">timer</option>
                    <option value="serial">serial</option>
                    <option value="action">action</option>
                  </select>
                </Field>

                <Field label="event_name">
                  <input
                    value={selectedEdge.eventName}
                    onFocus={pushHistorySnapshot}
                    onChange={(event) => {
                      const eventName = normalizeTokenInput(event.target.value)
                      setDocument((current) => ({
                        ...current,
                        edges: current.edges.map((edge) =>
                          edge.id === selectedEdge.id
                            ? {
                                ...edge,
                                eventName,
                              }
                            : edge,
                        ),
                      }))
                    }}
                    className="story-input mt-2 min-h-[40px]"
                  />
                </Field>

                <div className="grid gap-2 sm:grid-cols-2">
                  <Field label="after_ms">
                    <input
                      type="number"
                      min={0}
                      value={selectedEdge.afterMs}
                      onFocus={pushHistorySnapshot}
                      onChange={(event) => {
                        const nextValue = Number.parseInt(event.target.value || '0', 10)
                        setDocument((current) => ({
                          ...current,
                          edges: current.edges.map((edge) =>
                            edge.id === selectedEdge.id
                              ? {
                                  ...edge,
                                  afterMs: Number.isFinite(nextValue) ? Math.max(0, nextValue) : 0,
                                }
                              : edge,
                          ),
                        }))
                      }}
                      className="story-input mt-2 min-h-[40px]"
                    />
                  </Field>

                  <Field label="priority">
                    <input
                      type="number"
                      min={0}
                      max={255}
                      value={selectedEdge.priority}
                      onFocus={pushHistorySnapshot}
                      onChange={(event) => {
                        const nextValue = Number.parseInt(event.target.value || '100', 10)
                        setDocument((current) => ({
                          ...current,
                          edges: current.edges.map((edge) =>
                            edge.id === selectedEdge.id
                              ? {
                                  ...edge,
                                  priority: Number.isFinite(nextValue)
                                    ? Math.max(0, Math.min(255, nextValue))
                                    : 100,
                                }
                              : edge,
                          ),
                        }))
                      }}
                      className="story-input mt-2 min-h-[40px]"
                    />
                  </Field>
                </div>
              </div>
            ) : null}

            {!selectedNode && !selectedEdge ? (
              <InlineNotice className="mt-4" tone="info">
                Sélectionne un node ou un lien dans le graphe pour éditer ses paramètres.
              </InlineNotice>
            ) : null}
          </Panel>

          <Panel className={activeTab !== 'yaml' ? 'hidden lg:block' : ''}>
            <SectionHeader
              title="YAML"
              subtitle="Source de vérité éditable; import manuel vers graphe depuis cet éditeur."
              actions={
                <>
                  <Button variant="outline" size="sm" onClick={handleImportFromYaml}>
                    Importer vers graphe
                  </Button>
                  <Button variant="outline" size="sm" onClick={handleGenerateYaml}>
                    Générer depuis graphe
                  </Button>
                </>
              }
            />
            <InlineNotice className="mt-4" tone="info">
              Upload de fichier YAML non activé pour l'instant.
            </InlineNotice>
            <div className="mt-4">
              <CodeMirror
                value={draft}
                height="42vh"
                extensions={editorExtensions}
                onChange={setDraft}
                basicSetup={{ lineNumbers: true, foldGutter: false }}
              />
            </div>
          </Panel>

          <Panel>
            <SectionHeader title="Validation & déploiement" subtitle="Actions Story V2 avec fallback explicite en legacy." />

            {!validateEnabled || !deployEnabled ? (
              <InlineNotice className="mt-4" tone="info">
                Mode lecture/edition: validate/deploy requierent les APIs Story V2.
              </InlineNotice>
            ) : null}

            <div className="mt-4 grid gap-3 sm:grid-cols-2">
              <Button
                variant="outline"
                onClick={() => {
                  void handleValidate()
                }}
                disabled={busyAction !== null || !validateEnabled}
              >
                Valider
              </Button>
              <Button
                variant="secondary"
                onClick={() => {
                  void handleDeploy()
                }}
                disabled={busyAction !== null || !deployEnabled}
              >
                Déployer
              </Button>
              <Button
                variant="outline"
                className="sm:col-span-2"
                onClick={() => {
                  void handleTestRun()
                }}
                disabled={busyAction !== null || !testRunEnabled}
              >
                Test run (30s)
              </Button>
            </div>

            {status ? (
              <InlineNotice className="mt-4" tone={statusTone}>
                {status}
              </InlineNotice>
            ) : null}

            {errors.length > 0 ? (
              <InlineNotice className="mt-3" tone="error">
                {errors.map((error) => (
                  <p key={error}>- {error}</p>
                ))}
              </InlineNotice>
            ) : null}

            {importWarnings.length > 0 ? (
              <InlineNotice className="mt-3" tone="warning">
                {importWarnings.map((warning) => (
                  <p key={warning}>- {warning}</p>
                ))}
              </InlineNotice>
            ) : null}

            {graphValidation.warnings.length > 0 ? (
              <InlineNotice className="mt-3" tone="warning">
                {graphValidation.warnings.map((warning) => (
                  <p key={warning}>- {warning}</p>
                ))}
              </InlineNotice>
            ) : null}

            {graphValidation.errors.length > 0 ? (
              <InlineNotice className="mt-3" tone="error">
                {graphValidation.errors.map((error) => (
                  <p key={error}>- {error}</p>
                ))}
              </InlineNotice>
            ) : null}
          </Panel>
        </div>
      </div>

      {contextMenu ? (
        <div
          data-testid="story-context-menu"
          className="fixed z-40"
          style={{ left: contextMenu.x, top: contextMenu.y }}
          onClick={(event) => event.stopPropagation()}
        >
          <Panel className="w-[250px] rounded-2xl p-3">
            {contextMenu.kind === 'pane' ? (
              <div className="space-y-2">
                <p className="text-xs uppercase tracking-[0.16em] text-[var(--ink-500)]">Canvas</p>
                <Button
                  variant="outline"
                  size="sm"
                  fullWidth
                  data-testid="story-context-canvas-add-node"
                  onClick={() => {
                    addNodeAtPosition(
                      {
                        x: contextMenu.flowX ?? 120,
                        y: contextMenu.flowY ?? 120,
                      },
                      'Node ajouté à la position du pointeur.',
                    )
                    setContextMenu(null)
                  }}
                >
                  Ajouter node ici
                </Button>
                <Button
                  variant="outline"
                  size="sm"
                  fullWidth
                  data-testid="story-context-canvas-layout"
                  onClick={() => {
                    handleAutoLayout()
                    setContextMenu(null)
                  }}
                >
                  Auto-layout
                </Button>
                <Button
                  variant="ghost"
                  size="sm"
                  fullWidth
                  onClick={() => {
                    setContextMenu(null)
                  }}
                >
                  Fermer
                </Button>
              </div>
            ) : null}

            {contextMenu.kind === 'node' && contextNode ? (
              <div className="space-y-2">
                <p className="text-xs uppercase tracking-[0.16em] text-[var(--ink-500)]">Node {contextNode.stepId}</p>
                <Button
                  variant="outline"
                  size="sm"
                  fullWidth
                  data-testid="story-context-node-select"
                  onClick={() => {
                    setSelectedNodeId(contextNode.id)
                    setSelectedEdgeId(null)
                    setContextMenu(null)
                  }}
                >
                  Sélectionner
                </Button>
                <Button
                  variant="outline"
                  size="sm"
                  fullWidth
                  data-testid="story-context-node-add-child"
                  onClick={() => {
                    addChildNodeFrom(contextNode.id)
                    setContextMenu(null)
                  }}
                >
                  Ajouter enfant lié
                </Button>
                <Button
                  variant="outline"
                  size="sm"
                  fullWidth
                  data-testid="story-context-node-link"
                  onClick={() => {
                    setPendingLinkSourceId(contextNode.id)
                    setStatus(`Liaison armée depuis ${contextNode.stepId}. Clique un node cible.`)
                    setStatusTone('info')
                    setContextMenu(null)
                  }}
                >
                  Démarrer liaison
                </Button>
                <Button
                  variant="ghost"
                  size="sm"
                  fullWidth
                  data-testid="story-context-node-initial"
                  onClick={() => {
                    pushHistorySnapshot()
                    setDocument((current) => ({
                      ...current,
                      initialStep: contextNode.stepId,
                      nodes: current.nodes.map((node) => ({
                        ...node,
                        isInitial: node.id === contextNode.id,
                      })),
                    }))
                    setStatus(`Node ${contextNode.stepId} défini comme initial.`)
                    setStatusTone('success')
                    setContextMenu(null)
                  }}
                >
                  Définir comme initial
                </Button>
                <Button
                  variant="danger"
                  size="sm"
                  fullWidth
                  data-testid="story-context-node-delete"
                  onClick={() => {
                    handleDeleteNodeById(contextNode.id)
                    setContextMenu(null)
                  }}
                >
                  Supprimer node
                </Button>
              </div>
            ) : null}

            {contextMenu.kind === 'edge' && contextEdge ? (
              <div className="space-y-2">
                <p className="text-xs uppercase tracking-[0.16em] text-[var(--ink-500)]">Lien {contextEdge.id}</p>
                <Button
                  variant="outline"
                  size="sm"
                  fullWidth
                  data-testid="story-context-edge-select"
                  onClick={() => {
                    setSelectedEdgeId(contextEdge.id)
                    setSelectedNodeId(null)
                    setContextMenu(null)
                  }}
                >
                  Sélectionner
                </Button>
                <Button
                  variant="outline"
                  size="sm"
                  fullWidth
                  data-testid="story-context-edge-reverse"
                  onClick={() => {
                    handleReverseEdgeDirection(contextEdge.id)
                    setContextMenu(null)
                  }}
                >
                  Inverser direction
                </Button>
                <Button
                  variant="danger"
                  size="sm"
                  fullWidth
                  data-testid="story-context-edge-delete"
                  onClick={() => {
                    handleDeleteEdgeById(contextEdge.id)
                    setContextMenu(null)
                  }}
                >
                  Supprimer lien
                </Button>
              </div>
            ) : null}
          </Panel>
        </div>
      ) : null}
    </section>
  )
}

export default StoryDesigner
