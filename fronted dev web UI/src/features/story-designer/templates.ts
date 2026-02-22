export const STORY_TEMPLATE_LIBRARY: Record<string, string> = {
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
  - id: APP_WIFI
    app: WIFI_STACK
  - id: APP_ESPNOW
    app: ESPNOW_STACK
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
      - APP_WIFI
      - APP_ESPNOW
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
      - APP_WIFI
      - APP_ESPNOW
    mp3_gate_open: false
    transitions:
      - trigger: on_event
        event_type: audio_done
        event_name: AUDIO_DONE
        target_step_id: STEP_DONE
        after_ms: 0
        priority: 100
  - step_id: STEP_DONE
    screen_scene_id: SCENE_READY
    audio_pack_id: ""
    actions:
      - ACTION_TRACE_STEP
      - ACTION_REFRESH_SD
    apps:
      - APP_SCREEN
      - APP_GATE
      - APP_WIFI
      - APP_ESPNOW
    mp3_gate_open: true
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
}

