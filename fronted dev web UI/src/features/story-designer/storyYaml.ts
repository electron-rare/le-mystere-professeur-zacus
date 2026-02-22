import { parse, stringify } from 'yaml'
import {
  DEFAULT_ACTIONS,
  DEFAULT_AFTER_MS,
  DEFAULT_APP_BINDINGS,
  DEFAULT_AUDIO_PACK,
  DEFAULT_EVENT_NAME,
  DEFAULT_EVENT_TYPE,
  DEFAULT_NODE_APPS,
  DEFAULT_PRIORITY,
  DEFAULT_SCENARIO_ID,
  DEFAULT_SCENARIO_VERSION,
  DEFAULT_SCENE_ID,
  DEFAULT_STEP_ID,
  DEFAULT_TRIGGER,
} from './constants'
import { APP_KINDS, EVENT_TYPES, TRANSITION_TRIGGERS, type AppBinding, type EventType, type ImportResult, type StoryEdge, type StoryGraphDocument, type StoryNode } from './types'

const isRecord = (value: unknown): value is Record<string, unknown> =>
  typeof value === 'object' && value !== null

const normalizeToken = (value: unknown, fallback: string) => {
  const text = typeof value === 'string' ? value.trim().toUpperCase() : ''
  const normalized = text.replace(/[^A-Z0-9_]/g, '_')
  return normalized.length > 0 ? normalized : fallback
}

const ensureUniqueToken = (
  value: unknown,
  fallback: string,
  used: Set<string>,
  warnings: string[],
  context: string,
) => {
  const base = normalizeToken(value, fallback)
  let candidate = base
  let suffix = 2
  while (used.has(candidate)) {
    candidate = `${base}_${suffix}`
    suffix += 1
  }
  if (candidate !== base) {
    warnings.push(`${context}: collision d'ID resolue (${base} -> ${candidate}).`)
  }
  used.add(candidate)
  return candidate
}

const toStringArray = (value: unknown, normalize = true): string[] => {
  if (!Array.isArray(value)) {
    return []
  }
  const entries = value
    .map((item) => (typeof item === 'string' ? item : ''))
    .map((item) => (normalize ? normalizeToken(item, '') : item.trim()))
    .filter((item) => item.length > 0)
  return Array.from(new Set(entries))
}

const toNumber = (value: unknown, fallback: number) => {
  if (typeof value === 'number' && Number.isFinite(value)) {
    return value
  }
  if (typeof value === 'string' && value.trim().length > 0) {
    const parsed = Number(value)
    if (Number.isFinite(parsed)) {
      return parsed
    }
  }
  return fallback
}

const clampInteger = (value: number, min: number, max: number) => Math.min(max, Math.max(min, Math.trunc(value)))

export const inferEventTypeFromName = (eventName: string): EventType => {
  const normalized = normalizeToken(eventName, DEFAULT_EVENT_NAME)
  if (normalized === 'UNLOCK') {
    return 'unlock'
  }
  if (normalized === 'AUDIO_DONE') {
    return 'audio_done'
  }
  if (normalized.startsWith('TIMER')) {
    return 'timer'
  }
  if (normalized.startsWith('FORCE') || normalized === 'SKIP') {
    return 'serial'
  }
  return DEFAULT_EVENT_TYPE
}

const coerceTrigger = (value: unknown): (typeof TRANSITION_TRIGGERS)[number] => {
  const normalized = typeof value === 'string' ? value.trim().toLowerCase() : ''
  return TRANSITION_TRIGGERS.includes(normalized as (typeof TRANSITION_TRIGGERS)[number])
    ? (normalized as (typeof TRANSITION_TRIGGERS)[number])
    : DEFAULT_TRIGGER
}

const coerceEventType = (value: unknown, eventName: string): EventType => {
  const normalized = typeof value === 'string' ? value.trim().toLowerCase() : ''
  return EVENT_TYPES.includes(normalized as EventType)
    ? (normalized as EventType)
    : inferEventTypeFromName(eventName)
}

const coerceAppKind = (value: unknown): (typeof APP_KINDS)[number] => {
  const normalized = normalizeToken(value, 'SCREEN_SCENE')
  return APP_KINDS.includes(normalized as (typeof APP_KINDS)[number])
    ? (normalized as (typeof APP_KINDS)[number])
    : 'SCREEN_SCENE'
}

const cloneDefaultBindings = (): AppBinding[] =>
  DEFAULT_APP_BINDINGS.map((binding) => ({
    ...binding,
    config: binding.config ? { ...binding.config } : undefined,
  }))

const buildDefaultNode = (index: number, stepId: string): StoryNode => ({
  id: `node-${index + 1}`,
  stepId,
  screenSceneId: DEFAULT_SCENE_ID,
  audioPackId: DEFAULT_AUDIO_PACK,
  actions: [...DEFAULT_ACTIONS],
  apps: [...DEFAULT_NODE_APPS],
  mp3GateOpen: false,
  x: 48 + (index % 3) * 320,
  y: 48 + Math.floor(index / 3) * 240,
  isInitial: false,
})

type ParsedTransition = {
  fromNodeId: string
  targetStepId: string
  trigger: (typeof TRANSITION_TRIGGERS)[number]
  eventType: EventType
  eventName: string
  afterMs: number
  priority: number
}

export const importStoryYamlToGraph = (yamlText: string): ImportResult => {
  const warnings: string[] = []
  const errors: string[] = []

  let root: unknown
  try {
    root = parse(yamlText)
  } catch (error) {
    return {
      errors: [error instanceof Error ? error.message : 'Impossible de parser le YAML.'],
      warnings,
    }
  }

  if (!isRecord(root)) {
    return { errors: ['Le YAML doit contenir un objet racine.'], warnings }
  }

  const scenarioId = normalizeToken(root.id, DEFAULT_SCENARIO_ID)
  if (scenarioId !== root.id) {
    warnings.push('ID de scenario normalise pour respecter le format token.')
  }

  const version = clampInteger(toNumber(root.version, DEFAULT_SCENARIO_VERSION), 1, 99)
  const initialStepRaw = root.initial_step ?? root.initial_step_id
  const initialStepToken = normalizeToken(initialStepRaw, '')
  if (!initialStepToken) {
    warnings.push('initial_step absent: le premier node sera defini comme initial.')
  }

  const bindingIds = new Set<string>()
  const appBindings: AppBinding[] = []
  const rawBindings = Array.isArray(root.app_bindings) ? root.app_bindings : []
  if (!Array.isArray(root.app_bindings)) {
    warnings.push('app_bindings absent: bindings par defaut appliques.')
  }

  rawBindings.forEach((binding, index) => {
    if (!isRecord(binding)) {
      warnings.push(`app_bindings[${index}] ignore: format invalide.`)
      return
    }
    const id = ensureUniqueToken(binding.id, `APP_UNKNOWN_${index + 1}`, bindingIds, warnings, `app_bindings[${index}]`)
    const app = coerceAppKind(binding.app)
    const nextBinding: AppBinding = { id, app }
    if (app === 'LA_DETECTOR') {
      const config = isRecord(binding.config) ? binding.config : {}
      nextBinding.config = {
        hold_ms: clampInteger(toNumber(config.hold_ms, 3000), 100, 60000),
        unlock_event: normalizeToken(config.unlock_event, 'UNLOCK'),
        require_listening: typeof config.require_listening === 'boolean' ? config.require_listening : true,
      }
    }
    appBindings.push(nextBinding)
  })

  if (appBindings.length === 0) {
    cloneDefaultBindings().forEach((binding) => {
      const id = ensureUniqueToken(binding.id, 'APP_UNKNOWN', bindingIds, warnings, 'app_bindings')
      appBindings.push({ ...binding, id })
    })
  }

  const rawSteps = Array.isArray(root.steps) ? root.steps : []
  if (!Array.isArray(root.steps)) {
    return {
      errors: ['steps est requis et doit etre une liste.'],
      warnings,
    }
  }
  if (rawSteps.length === 0) {
    return {
      errors: ['steps ne peut pas etre vide.'],
      warnings,
    }
  }

  const stepIds = new Set<string>()
  const nodes: StoryNode[] = []
  const parsedTransitions: ParsedTransition[] = []

  rawSteps.forEach((step, index) => {
    if (!isRecord(step)) {
      warnings.push(`steps[${index}] ignore: format invalide.`)
      return
    }
    const stepId = ensureUniqueToken(
      step.step_id ?? step.id,
      `${DEFAULT_STEP_ID}_${index + 1}`,
      stepIds,
      warnings,
      `steps[${index}]`,
    )
    const node = buildDefaultNode(index, stepId)
    node.screenSceneId = normalizeToken(step.screen_scene_id, DEFAULT_SCENE_ID)
    node.audioPackId = normalizeToken(step.audio_pack_id, '')
    node.actions = toStringArray(step.actions)
    if (node.actions.length === 0) {
      node.actions = [...DEFAULT_ACTIONS]
    }
    node.apps = toStringArray(step.apps)
    if (node.apps.length === 0) {
      node.apps = [...DEFAULT_NODE_APPS]
    }
    node.mp3GateOpen = typeof step.mp3_gate_open === 'boolean' ? step.mp3_gate_open : false
    nodes.push(node)

    const transitions = Array.isArray(step.transitions) ? step.transitions : []
    transitions.forEach((transition, transitionIndex) => {
      if (!isRecord(transition)) {
        warnings.push(`steps[${index}].transitions[${transitionIndex}] ignore: format invalide.`)
        return
      }
      const target = normalizeToken(transition.target_step_id ?? transition.target, '')
      if (!target) {
        warnings.push(`steps[${index}].transitions[${transitionIndex}] ignore: target manquant.`)
        return
      }
      const eventName = normalizeToken(transition.event_name ?? transition.event, DEFAULT_EVENT_NAME)
      parsedTransitions.push({
        fromNodeId: node.id,
        targetStepId: target,
        trigger: coerceTrigger(transition.trigger),
        eventType: coerceEventType(transition.event_type, eventName),
        eventName,
        afterMs: Math.max(0, clampInteger(toNumber(transition.after_ms, DEFAULT_AFTER_MS), 0, Number.MAX_SAFE_INTEGER)),
        priority: clampInteger(toNumber(transition.priority, DEFAULT_PRIORITY), 0, 255),
      })
    })
  })

  if (nodes.length === 0) {
    return {
      errors: ['Aucun step valide detecte dans le YAML.'],
      warnings,
    }
  }

  const stepIdToNodeId = new Map(nodes.map((node) => [node.stepId, node.id]))

  let initialStep = initialStepToken
  if (!initialStep || !stepIdToNodeId.has(initialStep)) {
    if (initialStep && !stepIdToNodeId.has(initialStep)) {
      warnings.push(`initial_step (${initialStep}) introuvable: fallback sur ${nodes[0]?.stepId}.`)
    }
    initialStep = nodes[0]?.stepId ?? DEFAULT_STEP_ID
  }
  nodes.forEach((node) => {
    node.isInitial = node.stepId === initialStep
  })

  const edges: StoryEdge[] = []
  parsedTransitions.forEach((transition, index) => {
    const targetNodeId = stepIdToNodeId.get(transition.targetStepId)
    if (!targetNodeId) {
      warnings.push(`Transition ignoree: target_step_id introuvable (${transition.targetStepId}).`)
      return
    }
    edges.push({
      id: `edge-${index + 1}`,
      fromNodeId: transition.fromNodeId,
      toNodeId: targetNodeId,
      trigger: transition.trigger,
      eventType: transition.eventType,
      eventName: transition.eventName,
      afterMs: transition.afterMs,
      priority: transition.priority,
    })
  })

  const ensureBinding = (requestedId: string) => {
    const normalizedId = normalizeToken(requestedId, '')
    if (!normalizedId) {
      return
    }
    if (appBindings.some((binding) => binding.id === normalizedId)) {
      return
    }
    const uniqueId = ensureUniqueToken(normalizedId, 'APP_UNKNOWN', bindingIds, warnings, 'app_bindings(auto)')
    appBindings.push({ id: uniqueId, app: 'SCREEN_SCENE' })
    warnings.push(`app_binding auto-cree pour reference manquante: ${uniqueId}.`)
    nodes.forEach((node) => {
      node.apps = node.apps.map((appId) => (normalizeToken(appId, '') === normalizedId ? uniqueId : appId))
    })
  }

  nodes.forEach((node) => node.apps.forEach(ensureBinding))

  return {
    document: {
      scenarioId,
      version,
      initialStep,
      appBindings,
      nodes,
      edges,
    },
    errors,
    warnings,
  }
}

export const generateStoryYamlFromGraph = (document: StoryGraphDocument): string => {
  const warnings: string[] = []
  const bindingIds = new Set<string>()
  const appBindings = document.appBindings.map((binding, index) => {
    const id = ensureUniqueToken(binding.id, `APP_UNKNOWN_${index + 1}`, bindingIds, warnings, 'app_bindings')
    const app = coerceAppKind(binding.app)
    const next: AppBinding = { id, app }
    if (app === 'LA_DETECTOR') {
      next.config = {
        hold_ms: clampInteger(toNumber(binding.config?.hold_ms, 3000), 100, 60000),
        unlock_event: normalizeToken(binding.config?.unlock_event, 'UNLOCK'),
        require_listening:
          typeof binding.config?.require_listening === 'boolean' ? binding.config.require_listening : true,
      }
    }
    return next
  })

  if (appBindings.length === 0) {
    cloneDefaultBindings().forEach((binding) => {
      const id = ensureUniqueToken(binding.id, 'APP_UNKNOWN', bindingIds, warnings, 'app_bindings(default)')
      appBindings.push({ ...binding, id })
    })
  }

  const sortedNodes = [...document.nodes].sort((a, b) => a.y - b.y || a.x - b.x || a.stepId.localeCompare(b.stepId))
  const stepIds = new Set<string>()
  const stepIdByNodeId = new Map<string, string>()

  sortedNodes.forEach((node, index) => {
    const stepId = ensureUniqueToken(node.stepId, `${DEFAULT_STEP_ID}_${index + 1}`, stepIds, warnings, 'steps')
    stepIdByNodeId.set(node.id, stepId)
  })

  const initialNode = sortedNodes.find((node) => node.isInitial) ?? sortedNodes[0]
  const initialStep = (initialNode && stepIdByNodeId.get(initialNode.id)) || `${DEFAULT_STEP_ID}_1`

  const edgesBySource = new Map<string, StoryEdge[]>()
  document.edges.forEach((edge) => {
    const current = edgesBySource.get(edge.fromNodeId) ?? []
    current.push(edge)
    edgesBySource.set(edge.fromNodeId, current)
  })

  sortedNodes.forEach((node) => {
    node.apps.forEach((appId) => {
      const normalized = normalizeToken(appId, '')
      if (!normalized) {
        return
      }
      if (appBindings.some((binding) => binding.id === normalized)) {
        return
      }
      const id = ensureUniqueToken(normalized, 'APP_UNKNOWN', bindingIds, warnings, 'app_bindings(step)')
      appBindings.push({ id, app: 'SCREEN_SCENE' })
    })
  })

  const root = {
    id: normalizeToken(document.scenarioId, DEFAULT_SCENARIO_ID),
    version: clampInteger(toNumber(document.version, DEFAULT_SCENARIO_VERSION), 1, 99),
    initial_step: initialStep,
    app_bindings: appBindings.map((binding) => ({
      id: binding.id,
      app: binding.app,
      ...(binding.app === 'LA_DETECTOR' && binding.config
        ? {
            config: {
              hold_ms: clampInteger(toNumber(binding.config.hold_ms, 3000), 100, 60000),
              unlock_event: normalizeToken(binding.config.unlock_event, 'UNLOCK'),
              require_listening:
                typeof binding.config.require_listening === 'boolean' ? binding.config.require_listening : true,
            },
          }
        : {}),
    })),
    steps: sortedNodes.map((node, index) => {
      const stepId = stepIdByNodeId.get(node.id) ?? `${DEFAULT_STEP_ID}_${index + 1}`
      const transitions =
        edgesBySource.get(node.id)?.flatMap((edge) => {
          const targetStepId = stepIdByNodeId.get(edge.toNodeId)
          if (!targetStepId) {
            return []
          }
          const eventName = normalizeToken(edge.eventName, DEFAULT_EVENT_NAME)
          return [
            {
              trigger: coerceTrigger(edge.trigger),
              event_type: coerceEventType(edge.eventType, eventName),
              event_name: eventName,
              target_step_id: targetStepId,
              after_ms: Math.max(0, clampInteger(toNumber(edge.afterMs, DEFAULT_AFTER_MS), 0, Number.MAX_SAFE_INTEGER)),
              priority: clampInteger(toNumber(edge.priority, DEFAULT_PRIORITY), 0, 255),
            },
          ]
        }) ?? []

      const apps = Array.from(
        new Set(
          node.apps
            .map((appId) => normalizeToken(appId, ''))
            .filter((appId) => appId.length > 0),
        ),
      )

      return {
        step_id: stepId,
        screen_scene_id: normalizeToken(node.screenSceneId, DEFAULT_SCENE_ID),
        audio_pack_id: normalizeToken(node.audioPackId, ''),
        actions:
          toStringArray(node.actions, true).length > 0
            ? toStringArray(node.actions, true)
            : [...DEFAULT_ACTIONS],
        apps: apps.length > 0 ? apps : [...DEFAULT_NODE_APPS],
        mp3_gate_open: Boolean(node.mp3GateOpen),
        transitions: transitions.length > 0 ? transitions : [],
      }
    }),
  }

  const yaml = stringify(root, { lineWidth: 0 })
  return yaml.endsWith('\n') ? yaml : `${yaml}\n`
}

