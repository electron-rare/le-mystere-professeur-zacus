import type { AppBinding, EventType, TransitionTrigger } from './types'

export const DEFAULT_SCENARIO_ID = 'NODAL_STORY'
export const DEFAULT_SCENARIO_VERSION = 2
export const DEFAULT_STEP_ID = 'STEP_NODE'
export const DEFAULT_SCENE_ID = 'SCENE_LOCKED'
export const DEFAULT_AUDIO_PACK = ''
export const DEFAULT_ACTIONS = ['ACTION_TRACE_STEP']

export const DEFAULT_TRIGGER: TransitionTrigger = 'on_event'
export const DEFAULT_EVENT_TYPE: EventType = 'action'
export const DEFAULT_EVENT_NAME = 'BTN_NEXT'
export const DEFAULT_AFTER_MS = 0
export const DEFAULT_PRIORITY = 100

export const DEFAULT_APP_BINDINGS: AppBinding[] = [
  {
    id: 'APP_LA',
    app: 'LA_DETECTOR',
    config: {
      hold_ms: 3000,
      unlock_event: 'UNLOCK',
      require_listening: true,
    },
  },
  { id: 'APP_AUDIO', app: 'AUDIO_PACK' },
  { id: 'APP_SCREEN', app: 'SCREEN_SCENE' },
  { id: 'APP_GATE', app: 'MP3_GATE' },
]

export const DEFAULT_NODE_APPS = ['APP_SCREEN', 'APP_GATE']

