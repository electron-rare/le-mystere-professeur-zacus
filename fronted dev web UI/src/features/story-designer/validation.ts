import { APP_KINDS, EVENT_TYPES, TRANSITION_TRIGGERS, type StoryGraphDocument } from './types'

type ValidationResult = {
  errors: string[]
  warnings: string[]
}

const isToken = (value: string) => /^[A-Z0-9_]+$/.test(value)

export const validateStoryGraph = (document: StoryGraphDocument): ValidationResult => {
  const errors: string[] = []
  const warnings: string[] = []

  if (!document.scenarioId.trim()) {
    errors.push('Le scenario doit avoir un ID.')
  }

  if (!Number.isFinite(document.version) || document.version < 1) {
    errors.push('La version du scenario est invalide.')
  }

  if (document.nodes.length === 0) {
    errors.push('Le scenario doit contenir au moins un node.')
  }

  const initialNodes = document.nodes.filter((node) => node.isInitial)
  if (initialNodes.length === 0) {
    errors.push('Un node initial est obligatoire.')
  }
  if (initialNodes.length > 1) {
    errors.push('Un seul node initial est autorise.')
  }

  const stepIds = new Set<string>()
  document.nodes.forEach((node) => {
    const normalized = node.stepId.trim().toUpperCase()
    if (!normalized) {
      errors.push(`Node ${node.id}: step_id manquant.`)
      return
    }
    if (!isToken(normalized)) {
      warnings.push(`Node ${node.id}: step_id "${node.stepId}" contient des caracteres non standards.`)
    }
    if (stepIds.has(normalized)) {
      errors.push(`step_id duplique: ${normalized}.`)
    }
    stepIds.add(normalized)
  })

  const bindingIds = new Set<string>()
  document.appBindings.forEach((binding) => {
    const normalized = binding.id.trim().toUpperCase()
    if (!normalized) {
      errors.push('Un app_binding a un id vide.')
      return
    }
    if (!APP_KINDS.includes(binding.app)) {
      errors.push(`app_binding ${binding.id}: app invalide (${binding.app}).`)
    }
    if (bindingIds.has(normalized)) {
      errors.push(`app_binding duplique: ${normalized}.`)
    }
    bindingIds.add(normalized)
  })

  document.nodes.forEach((node) => {
    node.apps.forEach((appId) => {
      const normalized = appId.trim().toUpperCase()
      if (!bindingIds.has(normalized)) {
        warnings.push(`Node ${node.stepId}: app_binding introuvable (${normalized}).`)
      }
    })
  })

  const nodeIds = new Set(document.nodes.map((node) => node.id))
  document.edges.forEach((edge) => {
    if (!nodeIds.has(edge.fromNodeId) || !nodeIds.has(edge.toNodeId)) {
      errors.push(`Edge ${edge.id}: lien source/cible invalide.`)
    }
    if (!TRANSITION_TRIGGERS.includes(edge.trigger)) {
      errors.push(`Edge ${edge.id}: trigger invalide (${edge.trigger}).`)
    }
    if (!EVENT_TYPES.includes(edge.eventType)) {
      errors.push(`Edge ${edge.id}: event_type invalide (${edge.eventType}).`)
    }
    if (!edge.eventName.trim()) {
      errors.push(`Edge ${edge.id}: event_name vide.`)
    }
    if (!Number.isFinite(edge.afterMs) || edge.afterMs < 0) {
      errors.push(`Edge ${edge.id}: after_ms invalide.`)
    }
    if (!Number.isFinite(edge.priority) || edge.priority < 0 || edge.priority > 255) {
      errors.push(`Edge ${edge.id}: priority invalide (0..255).`)
    }
  })

  return { errors, warnings }
}

