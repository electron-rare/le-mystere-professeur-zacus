import { describe, expect, it } from 'vitest'
import {
  generateStoryYamlFromGraph,
  importStoryYamlToGraph,
  type StoryGraphDocument,
  validateStoryGraph,
} from '../../src/features/story-designer'

const canonicalYaml = `id: STORY_CANON
version: 2
initial_step: STEP_A
app_bindings:
  - id: APP_LA
    app: LA_DETECTOR
    config:
      hold_ms: 5000
      unlock_event: UNLOCK
      require_listening: true
  - id: APP_AUDIO
    app: AUDIO_PACK
steps:
  - step_id: STEP_A
    screen_scene_id: SCENE_A
    audio_pack_id: PACK_A
    actions: [ACTION_TRACE_STEP]
    apps: [APP_LA]
    mp3_gate_open: false
    transitions:
      - trigger: on_event
        event_type: unlock
        event_name: UNLOCK
        target_step_id: STEP_B
        after_ms: 0
        priority: 100
  - step_id: STEP_B
    screen_scene_id: SCENE_B
    audio_pack_id: ""
    actions: [ACTION_TRACE_STEP]
    apps: [APP_AUDIO]
    mp3_gate_open: true
    transitions: []
`

const normalizeDocument = (document: StoryGraphDocument) => ({
  scenarioId: document.scenarioId,
  version: document.version,
  initialStep: document.initialStep,
  appBindings: [...document.appBindings]
    .map((binding) => ({
      id: binding.id,
      app: binding.app,
      config: binding.config
        ? {
            hold_ms: binding.config.hold_ms,
            unlock_event: binding.config.unlock_event,
            require_listening: binding.config.require_listening,
          }
        : undefined,
    }))
    .sort((left, right) => left.id.localeCompare(right.id)),
  nodes: [...document.nodes]
    .map((node) => ({
      stepId: node.stepId,
      screenSceneId: node.screenSceneId,
      audioPackId: node.audioPackId,
      actions: [...node.actions],
      apps: [...node.apps].sort(),
      mp3GateOpen: node.mp3GateOpen,
      isInitial: node.isInitial,
    }))
    .sort((left, right) => left.stepId.localeCompare(right.stepId)),
  edges: [...document.edges]
    .map((edge) => ({
      fromNodeId: edge.fromNodeId,
      toNodeId: edge.toNodeId,
      trigger: edge.trigger,
      eventType: edge.eventType,
      eventName: edge.eventName,
      afterMs: edge.afterMs,
      priority: edge.priority,
    }))
    .sort((left, right) => `${left.fromNodeId}-${left.toNodeId}`.localeCompare(`${right.fromNodeId}-${right.toNodeId}`)),
})

describe('storyYaml import/export', () => {
  it('importe un YAML Story V2 canonique', () => {
    const result = importStoryYamlToGraph(canonicalYaml)

    expect(result.errors).toEqual([])
    expect(result.document).toBeDefined()
    expect(result.document?.nodes).toHaveLength(2)
    expect(result.document?.edges).toHaveLength(1)
    expect(result.document?.appBindings).toHaveLength(2)
    expect(result.document?.initialStep).toBe('STEP_A')
    expect(result.document?.edges[0]?.eventType).toBe('unlock')
  })

  it('importe un YAML legacy simplifie et mappe les transitions', () => {
    const legacyYaml = `id: LEGACY
version: 2
initial_step_id: STEP_WAIT_UNLOCK
steps:
  - id: STEP_WAIT_UNLOCK
    screen_scene_id: SCENE_LOCKED
    transitions:
      - event: unlock
        target: STEP_DONE
  - id: STEP_DONE
    screen_scene_id: SCENE_DONE
    transitions: []
`

    const result = importStoryYamlToGraph(legacyYaml)

    expect(result.errors).toEqual([])
    expect(result.warnings.length).toBeGreaterThan(0)
    expect(result.document?.nodes).toHaveLength(2)
    expect(result.document?.edges).toHaveLength(1)
    expect(result.document?.edges[0]).toMatchObject({
      trigger: 'on_event',
      eventType: 'unlock',
      eventName: 'UNLOCK',
      afterMs: 0,
      priority: 100,
    })
  })

  it('preserve les configs simples hors LA_DETECTOR', () => {
    const yamlWithSimpleConfig = `id: CFG_SIMPLE
version: 2
initial_step: STEP_A
app_bindings:
  - id: APP_AUDIO
    app: AUDIO_PACK
    config:
      volume_pct: 70
      ducking: true
      output: SPEAKER
steps:
  - step_id: STEP_A
    apps: [APP_AUDIO]
    transitions: []
`

    const imported = importStoryYamlToGraph(yamlWithSimpleConfig)
    expect(imported.errors).toEqual([])
    expect(imported.document?.appBindings[0]?.config).toMatchObject({
      volume_pct: 70,
      ducking: true,
      output: 'SPEAKER',
    })

    const exported = generateStoryYamlFromGraph(imported.document as StoryGraphDocument)
    const reimported = importStoryYamlToGraph(exported)
    expect(reimported.errors).toEqual([])
    expect(reimported.document?.appBindings[0]?.config).toMatchObject({
      volume_pct: 70,
      ducking: true,
      output: 'SPEAKER',
    })
  })

  it('retourne une erreur claire si le YAML est invalide', () => {
    const result = importStoryYamlToGraph('id: BAD\nsteps: [')

    expect(result.document).toBeUndefined()
    expect(result.errors.length).toBeGreaterThan(0)
  })

  it('clamp les transitions invalides (after_ms et priority)', () => {
    const yamlWithInvalidTransition = `id: CLAMP_TEST
version: 2
initial_step: STEP_A
steps:
  - step_id: STEP_A
    transitions:
      - trigger: after_ms
        event_type: timer
        event_name: TIMER_A
        target_step_id: STEP_B
        after_ms: -42
        priority: 999
  - step_id: STEP_B
    transitions: []
`

    const result = importStoryYamlToGraph(yamlWithInvalidTransition)
    expect(result.document).toBeDefined()
    expect(result.document?.edges[0]?.afterMs).toBe(0)
    expect(result.document?.edges[0]?.priority).toBe(255)
  })

  it('maintient une structure stable en round-trip import -> export -> import', () => {
    const imported = importStoryYamlToGraph(canonicalYaml)
    expect(imported.document).toBeDefined()

    const exportedYaml = generateStoryYamlFromGraph(imported.document as StoryGraphDocument)
    const reimported = importStoryYamlToGraph(exportedYaml)

    expect(reimported.errors).toEqual([])
    expect(reimported.document).toBeDefined()
    expect(normalizeDocument(reimported.document as StoryGraphDocument)).toEqual(
      normalizeDocument(imported.document as StoryGraphDocument),
    )
  })
})

describe('validation', () => {
  it('signale les bindings dupliques et priorites invalides', () => {
    const base = importStoryYamlToGraph(canonicalYaml).document as StoryGraphDocument
    const document: StoryGraphDocument = {
      ...base,
      appBindings: [
        { id: 'APP_AUDIO', app: 'AUDIO_PACK' },
        { id: 'APP_AUDIO', app: 'AUDIO_PACK' },
      ],
      edges: [
        {
          ...base.edges[0],
          priority: 999,
        },
      ],
    }

    const result = validateStoryGraph(document)

    expect(result.errors.some((entry) => entry.includes('app_binding duplique'))).toBe(true)
    expect(result.errors.some((entry) => entry.includes('priority invalide'))).toBe(true)
  })
})
