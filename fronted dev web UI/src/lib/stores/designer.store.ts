import { writable } from 'svelte/store'
import {
  autoLayoutStoryGraph,
  generateStoryYamlFromGraph,
  importStoryYamlToGraph,
  STORY_TEMPLATE_LIBRARY,
  validateStoryGraph,
  type StoryEdge,
  type StoryGraphDocument,
  type StoryNode,
} from '../../features/story-designer'
import { scenarioStore } from './scenario.store'

const STORAGE_DRAFT = 'studio:v3:draft'
const STORAGE_GRAPH = 'studio:v3:graph'
const MAX_HISTORY = 80

type StatusTone = 'info' | 'success' | 'warning' | 'error'

type DesignerState = {
  ready: boolean
  draft: string
  document: StoryGraphDocument
  status: string
  statusTone: StatusTone
  importWarnings: string[]
  localErrors: string[]
  localWarnings: string[]
  busyAction: 'validate' | 'deploy' | 'test' | null
  selectedNodeId: string | null
  selectedEdgeId: string | null
  linkingFromNodeId: string | null
  historyPast: StoryGraphDocument[]
  historyFuture: StoryGraphDocument[]
}

export type DesignerGraphDocumentV3 = StoryGraphDocument

const cloneDocument = (document: StoryGraphDocument) => JSON.parse(JSON.stringify(document)) as StoryGraphDocument

const isBrowser = () => typeof window !== 'undefined'

const isGraphDocument = (value: unknown): value is StoryGraphDocument => {
  if (!value || typeof value !== 'object') {
    return false
  }
  const parsed = value as Partial<StoryGraphDocument>
  return (
    typeof parsed.scenarioId === 'string' &&
    typeof parsed.version === 'number' &&
    typeof parsed.initialStep === 'string' &&
    Array.isArray(parsed.nodes) &&
    Array.isArray(parsed.edges) &&
    Array.isArray(parsed.appBindings)
  )
}

const fallbackDocument = (): StoryGraphDocument => {
  const imported = importStoryYamlToGraph(STORY_TEMPLATE_LIBRARY.DEFAULT)
  if (imported.document) {
    return autoLayoutStoryGraph(imported.document, {
      direction: 'LR',
      nodeWidth: 220,
      nodeHeight: 150,
    })
  }

  return {
    scenarioId: 'DEFAULT',
    version: 2,
    initialStep: 'STEP_1',
    appBindings: [],
    nodes: [
      {
        id: 'node-1',
        stepId: 'STEP_1',
        screenSceneId: 'SCENE_1',
        audioPackId: '',
        actions: ['ACTION_TRACE_STEP'],
        apps: ['APP_SCREEN'],
        mp3GateOpen: false,
        x: 80,
        y: 80,
        isInitial: true,
      },
    ],
    edges: [],
  }
}

const loadInitial = (): Pick<DesignerState, 'draft' | 'document'> => {
  if (!isBrowser()) {
    const document = fallbackDocument()
    return {
      draft: STORY_TEMPLATE_LIBRARY.DEFAULT,
      document,
    }
  }

  // reset old keys as requested by spec
  window.localStorage.removeItem('story-draft')
  window.localStorage.removeItem('story-graph-document')

  const storedDraft = window.localStorage.getItem(STORAGE_DRAFT)
  const storedGraph = window.localStorage.getItem(STORAGE_GRAPH)

  if (storedDraft && storedGraph) {
    try {
      const parsedGraph = JSON.parse(storedGraph) as unknown
      if (isGraphDocument(parsedGraph)) {
        return {
          draft: storedDraft,
          document: parsedGraph,
        }
      }
    } catch {
      // fallback below
    }
  }

  const imported = importStoryYamlToGraph(storedDraft || STORY_TEMPLATE_LIBRARY.DEFAULT)
  if (imported.document) {
    return {
      draft: storedDraft || STORY_TEMPLATE_LIBRARY.DEFAULT,
      document: autoLayoutStoryGraph(imported.document, {
        direction: 'LR',
        nodeWidth: 220,
        nodeHeight: 150,
      }),
    }
  }

  return {
    draft: STORY_TEMPLATE_LIBRARY.DEFAULT,
    document: fallbackDocument(),
  }
}

const persistState = (draft: string, document: StoryGraphDocument) => {
  if (!isBrowser()) {
    return
  }
  window.localStorage.setItem(STORAGE_DRAFT, draft)
  window.localStorage.setItem(STORAGE_GRAPH, JSON.stringify(document))
}

const nextNodeId = (document: StoryGraphDocument) => {
  let index = document.nodes.length + 1
  const ids = new Set(document.nodes.map((node) => node.id))
  let candidate = `node-${index}`
  while (ids.has(candidate)) {
    index += 1
    candidate = `node-${index}`
  }
  return candidate
}

const nextStepId = (document: StoryGraphDocument) => {
  const existing = new Set(document.nodes.map((node) => node.stepId.toUpperCase()))
  let index = document.nodes.length + 1
  let candidate = `STEP_${index}`
  while (existing.has(candidate)) {
    index += 1
    candidate = `STEP_${index}`
  }
  return candidate
}

const nextEdgeId = () => `edge-${Date.now().toString(36)}-${Math.random().toString(36).slice(2, 7)}`

const initialLoaded = loadInitial()

const defaultState = (): DesignerState => ({
  ready: false,
  draft: initialLoaded.draft,
  document: initialLoaded.document,
  status: 'Studio prêt.',
  statusTone: 'info',
  importWarnings: [],
  localErrors: [],
  localWarnings: [],
  busyAction: null,
  selectedNodeId: null,
  selectedEdgeId: null,
  linkingFromNodeId: null,
  historyPast: [],
  historyFuture: [],
})

const applyHistory = (current: DesignerState, nextDocument: StoryGraphDocument, status = current.status, tone = current.statusTone): DesignerState => {
  const historyPast = [...current.historyPast, cloneDocument(current.document)].slice(-MAX_HISTORY)
  const next = {
    ...current,
    document: nextDocument,
    historyPast,
    historyFuture: [],
    status,
    statusTone: tone,
    selectedEdgeId: null,
  }
  persistState(next.draft, next.document)
  return next
}

const createDesignerStore = () => {
  const { subscribe, update } = writable<DesignerState>(defaultState())

  const markReady = () => {
    update((state) => {
      persistState(state.draft, state.document)
      return { ...state, ready: true }
    })
  }

  const setDraft = (draft: string) => {
    update((state) => {
      const next = { ...state, draft }
      persistState(next.draft, next.document)
      return next
    })
  }

  const setStatus = (status: string, tone: StatusTone = 'info') => {
    update((state) => ({ ...state, status, statusTone: tone }))
  }

  const validateLocal = () => {
    update((state) => {
      const validation = validateStoryGraph(state.document)
      return {
        ...state,
        localErrors: validation.errors,
        localWarnings: validation.warnings,
        status: validation.errors.length === 0 ? 'Validation locale OK.' : 'Validation locale KO.',
        statusTone: validation.errors.length === 0 ? 'success' : 'error',
      }
    })
  }

  const importYaml = () => {
    update((state) => {
      const imported = importStoryYamlToGraph(state.draft)
      if (!imported.document) {
        return {
          ...state,
          importWarnings: imported.warnings,
          localErrors: imported.errors,
          status: imported.errors[0] || 'Import YAML impossible.',
          statusTone: 'error',
        }
      }

      const nextDocument = autoLayoutStoryGraph(imported.document, {
        direction: 'LR',
        nodeWidth: 220,
        nodeHeight: 150,
      })

      const next = applyHistory(state, nextDocument, 'YAML importé dans le graphe.', 'success')
      return {
        ...next,
        importWarnings: imported.warnings,
        localErrors: [],
      }
    })
    validateLocal()
  }

  const exportYaml = () => {
    update((state) => {
      const yaml = generateStoryYamlFromGraph(state.document)
      const next = {
        ...state,
        draft: yaml,
        status: 'Graphe exporté vers YAML.',
        statusTone: 'success' as StatusTone,
      }
      persistState(next.draft, next.document)
      return next
    })
  }

  const updateScenarioMeta = (patch: Partial<Pick<StoryGraphDocument, 'scenarioId' | 'version'>>) => {
    update((state) => {
      const nextDocument = {
        ...state.document,
        ...patch,
      }
      const next = {
        ...state,
        document: nextDocument,
      }
      persistState(next.draft, next.document)
      return next
    })
  }

  const addNode = (x = 120, y = 120) => {
    update((state) => {
      const node: StoryNode = {
        id: nextNodeId(state.document),
        stepId: nextStepId(state.document),
        screenSceneId: 'SCENE_NEW',
        audioPackId: '',
        actions: ['ACTION_TRACE_STEP'],
        apps: ['APP_SCREEN'],
        mp3GateOpen: false,
        x,
        y,
        isInitial: state.document.nodes.length === 0,
      }
      const nextDocument = {
        ...state.document,
        nodes: [...state.document.nodes, node],
      }
      return applyHistory(state, nextDocument, 'Node ajouté.', 'success')
    })
  }

  const removeNode = (nodeId: string) => {
    update((state) => {
      const nextNodes = state.document.nodes.filter((node) => node.id !== nodeId)
      const nextEdges = state.document.edges.filter((edge) => edge.fromNodeId !== nodeId && edge.toNodeId !== nodeId)
      if (nextNodes.length > 0 && nextNodes.every((node) => !node.isInitial)) {
        nextNodes[0] = { ...nextNodes[0], isInitial: true }
      }
      const nextDocument = {
        ...state.document,
        nodes: nextNodes,
        edges: nextEdges,
      }
      return applyHistory(state, nextDocument, 'Node supprimé.', 'warning')
    })
    validateLocal()
  }

  const updateNode = (nodeId: string, patch: Partial<StoryNode>) => {
    update((state) => {
      const nextNodes = state.document.nodes.map((node) => (node.id === nodeId ? { ...node, ...patch } : node))
      const nextDocument = {
        ...state.document,
        nodes: nextNodes,
      }
      return applyHistory(state, nextDocument, 'Node mis à jour.', 'info')
    })
    validateLocal()
  }

  const setInitialNode = (nodeId: string) => {
    update((state) => {
      const nextNodes = state.document.nodes.map((node) => ({
        ...node,
        isInitial: node.id === nodeId,
      }))
      const nextDocument = {
        ...state.document,
        nodes: nextNodes,
        initialStep: nextNodes.find((node) => node.id === nodeId)?.stepId || state.document.initialStep,
      }
      return applyHistory(state, nextDocument, 'Node initial défini.', 'info')
    })
  }

  const addEdge = (fromNodeId: string, toNodeId: string) => {
    if (fromNodeId === toNodeId) {
      setStatus('Lien refusé: source et cible identiques.', 'warning')
      return
    }

    update((state) => {
      const exists = state.document.edges.some((edge) => edge.fromNodeId === fromNodeId && edge.toNodeId === toNodeId)
      if (exists) {
        return {
          ...state,
          status: 'Lien déjà existant.',
          statusTone: 'warning',
          linkingFromNodeId: null,
        }
      }

      const edge: StoryEdge = {
        id: nextEdgeId(),
        fromNodeId,
        toNodeId,
        trigger: 'on_event',
        eventType: 'action',
        eventName: 'ACTION_NEXT',
        afterMs: 0,
        priority: 100,
      }
      const nextDocument = {
        ...state.document,
        edges: [...state.document.edges, edge],
      }
      const next = applyHistory(state, nextDocument, 'Lien ajouté.', 'success')
      return { ...next, linkingFromNodeId: null, selectedEdgeId: edge.id }
    })
    validateLocal()
  }

  const updateEdge = (edgeId: string, patch: Partial<StoryEdge>) => {
    update((state) => {
      const nextEdges = state.document.edges.map((edge) => (edge.id === edgeId ? { ...edge, ...patch } : edge))
      const nextDocument = {
        ...state.document,
        edges: nextEdges,
      }
      return applyHistory(state, nextDocument, 'Lien mis à jour.', 'info')
    })
    validateLocal()
  }

  const removeEdge = (edgeId: string) => {
    update((state) => {
      const nextEdges = state.document.edges.filter((edge) => edge.id !== edgeId)
      const nextDocument = {
        ...state.document,
        edges: nextEdges,
      }
      const next = applyHistory(state, nextDocument, 'Lien supprimé.', 'warning')
      return { ...next, selectedEdgeId: null }
    })
    validateLocal()
  }

  const autoLayout = () => {
    update((state) => {
      const nextDocument = autoLayoutStoryGraph(cloneDocument(state.document), {
        direction: 'LR',
        nodeWidth: 220,
        nodeHeight: 150,
      })
      return applyHistory(state, nextDocument, 'Auto-layout appliqué.', 'success')
    })
  }

  const undo = () => {
    update((state) => {
      const previous = state.historyPast[state.historyPast.length - 1]
      if (!previous) {
        return {
          ...state,
          status: 'Aucune action à annuler.',
          statusTone: 'warning',
        }
      }

      const nextPast = state.historyPast.slice(0, -1)
      const nextFuture = [cloneDocument(state.document), ...state.historyFuture].slice(0, MAX_HISTORY)
      const next = {
        ...state,
        document: cloneDocument(previous),
        historyPast: nextPast,
        historyFuture: nextFuture,
        status: 'Annulation effectuée.',
        statusTone: 'info' as StatusTone,
      }
      persistState(next.draft, next.document)
      return next
    })
    validateLocal()
  }

  const redo = () => {
    update((state) => {
      const candidate = state.historyFuture[0]
      if (!candidate) {
        return {
          ...state,
          status: 'Aucune action à rétablir.',
          statusTone: 'warning',
        }
      }

      const nextFuture = state.historyFuture.slice(1)
      const nextPast = [...state.historyPast, cloneDocument(state.document)].slice(-MAX_HISTORY)
      const next = {
        ...state,
        document: cloneDocument(candidate),
        historyPast: nextPast,
        historyFuture: nextFuture,
        status: 'Rétablissement effectué.',
        statusTone: 'info' as StatusTone,
      }
      persistState(next.draft, next.document)
      return next
    })
    validateLocal()
  }

  const selectNode = (nodeId: string | null) => {
    update((state) => ({ ...state, selectedNodeId: nodeId, selectedEdgeId: null }))
  }

  const selectEdge = (edgeId: string | null) => {
    update((state) => ({ ...state, selectedEdgeId: edgeId, selectedNodeId: null }))
  }

  const startLinkMode = (nodeId: string) => {
    update((state) => ({ ...state, linkingFromNodeId: nodeId, status: `Mode liaison: ${nodeId} -> ...`, statusTone: 'info' }))
  }

  const cancelLinkMode = () => {
    update((state) => ({ ...state, linkingFromNodeId: null, status: 'Mode liaison annulé.', statusTone: 'info' }))
  }

  const runRemoteValidate = async () => {
    update((state) => ({ ...state, busyAction: 'validate' }))
    try {
      await scenarioStore.validateYaml(generateStoryYamlFromGraph(cloneDocument(getState().document)))
      setStatus('Validation API exécutée.', 'success')
    } catch (error) {
      setStatus(error instanceof Error ? error.message : 'Validation API impossible.', 'error')
    } finally {
      update((state) => ({ ...state, busyAction: null }))
    }
  }

  const runDeploy = async () => {
    update((state) => ({ ...state, busyAction: 'deploy' }))
    try {
      await scenarioStore.deployYaml(generateStoryYamlFromGraph(cloneDocument(getState().document)))
      setStatus('Déploiement API exécuté.', 'success')
    } catch (error) {
      setStatus(error instanceof Error ? error.message : 'Déploiement impossible.', 'error')
    } finally {
      update((state) => ({ ...state, busyAction: null }))
    }
  }

  const runTest = async () => {
    update((state) => ({ ...state, busyAction: 'test' }))
    try {
      await scenarioStore.deployAndStart(generateStoryYamlFromGraph(cloneDocument(getState().document)))
      setStatus('Test run lancé.', 'success')
    } catch (error) {
      setStatus(error instanceof Error ? error.message : 'Test run impossible.', 'error')
    } finally {
      update((state) => ({ ...state, busyAction: null }))
    }
  }

  const getState = (): DesignerState => {
    let current = defaultState()
    const unsubscribe = subscribe((state) => {
      current = state
    })
    unsubscribe()
    return current
  }

  return {
    subscribe,
    markReady,
    setDraft,
    setStatus,
    validateLocal,
    importYaml,
    exportYaml,
    updateScenarioMeta,
    addNode,
    removeNode,
    updateNode,
    setInitialNode,
    addEdge,
    updateEdge,
    removeEdge,
    autoLayout,
    undo,
    redo,
    selectNode,
    selectEdge,
    startLinkMode,
    cancelLinkMode,
    runRemoteValidate,
    runDeploy,
    runTest,
    getState,
  }
}

export const designerStore = createDesignerStore()
