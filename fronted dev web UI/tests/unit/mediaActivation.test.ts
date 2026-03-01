import { describe, expect, it } from 'vitest'
import { resolveMediaActivation } from '../../src/lib/utils/media-activation'
import type { RuntimeSnapshot } from '../../src/lib/runtimeService'

const baseSnapshot = (patch: Partial<RuntimeSnapshot>): RuntimeSnapshot => ({
  source: 'freenove_legacy',
  status: 'idle',
  progressPct: 0,
  media: {
    ready: true,
    playing: false,
    recording: false,
    record_simulated: true,
    record_limit_seconds: 20,
    record_elapsed_seconds: 0,
    record_file: '',
    last_error: '',
    media_dirs: {
      music_dir: '/music',
      picture_dir: '/picture',
      record_dir: '/recorder',
    },
  },
  ...patch,
})

describe('resolveMediaActivation', () => {
  it('priorise story.screen == SCENE_MEDIA_MANAGER', () => {
    const activation = resolveMediaActivation(
      baseSnapshot({
        currentScreen: 'SCENE_MEDIA_MANAGER',
        currentStep: 'STEP_X',
      }),
    )

    expect(activation.active).toBe(true)
    expect(activation.reason).toBe('screen')
  })

  it('fallback sur story.step == STEP_MEDIA_MANAGER', () => {
    const activation = resolveMediaActivation(
      baseSnapshot({
        currentScreen: 'SCENE_X',
        currentStep: 'STEP_MEDIA_MANAGER',
      }),
    )

    expect(activation.active).toBe(true)
    expect(activation.reason).toBe('step')
  })

  it('tolere legacy avec token MEDIA_MANAGER', () => {
    const activation = resolveMediaActivation(
      baseSnapshot({
        story: {
          current_scene: 'my_media_manager_scene',
        },
      }),
    )

    expect(activation.active).toBe(true)
    expect(activation.reason).toBe('legacy')
  })
})
