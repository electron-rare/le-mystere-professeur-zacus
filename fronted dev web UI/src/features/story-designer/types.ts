export const APP_KINDS = [
  'LA_DETECTOR',
  'AUDIO_PACK',
  'SCREEN_SCENE',
  'MP3_GATE',
  'WIFI_STACK',
  'ESPNOW_STACK',
] as const

export const TRANSITION_TRIGGERS = ['on_event', 'after_ms', 'immediate'] as const

export const EVENT_TYPES = ['none', 'unlock', 'audio_done', 'timer', 'serial', 'action'] as const

export type AppKind = (typeof APP_KINDS)[number]
export type TransitionTrigger = (typeof TRANSITION_TRIGGERS)[number]
export type EventType = (typeof EVENT_TYPES)[number]

export type AppBinding = {
  id: string
  app: AppKind
  config?: {
    hold_ms?: number
    unlock_event?: string
    require_listening?: boolean
  }
}

export type StoryNode = {
  id: string
  stepId: string
  screenSceneId: string
  audioPackId: string
  actions: string[]
  apps: string[]
  mp3GateOpen: boolean
  x: number
  y: number
  isInitial: boolean
}

export type StoryEdge = {
  id: string
  fromNodeId: string
  toNodeId: string
  trigger: TransitionTrigger
  eventType: EventType
  eventName: string
  afterMs: number
  priority: number
}

export type StoryGraphDocument = {
  scenarioId: string
  version: number
  initialStep: string
  appBindings: AppBinding[]
  nodes: StoryNode[]
  edges: StoryEdge[]
}

export type ImportResult = {
  document?: StoryGraphDocument
  errors: string[]
  warnings: string[]
}

