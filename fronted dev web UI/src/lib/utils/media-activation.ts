import type { RuntimeSnapshot } from '../runtimeService'

const normalize = (value: unknown) => (typeof value === 'string' ? value.trim().toUpperCase() : '')

const hasMediaManagerToken = (value: unknown) => normalize(value).includes('MEDIA_MANAGER')

export type MediaActivationState = {
  active: boolean
  reason: 'screen' | 'step' | 'legacy' | 'none'
  screen: string
  step: string
}

export const resolveMediaActivation = (snapshot: RuntimeSnapshot | null): MediaActivationState => {
  if (!snapshot) {
    return { active: false, reason: 'none', screen: '', step: '' }
  }

  const story = snapshot.story ?? {}
  const storyRecord = typeof story === 'object' && story !== null ? (story as Record<string, unknown>) : {}
  const screen = normalize(snapshot.currentScreen || storyRecord.screen || storyRecord.scene)
  const step = normalize(snapshot.currentStep || storyRecord.step || storyRecord.current_step || storyRecord.current)

  if (screen === 'SCENE_MEDIA_MANAGER') {
    return { active: true, reason: 'screen', screen, step }
  }

  if (step === 'STEP_MEDIA_MANAGER') {
    return { active: true, reason: 'step', screen, step }
  }

  const legacyCandidate = [screen, step, storyRecord.currentStep, storyRecord.current_scene, storyRecord.currentStepId]
  if (legacyCandidate.some((candidate) => hasMediaManagerToken(candidate))) {
    return { active: true, reason: 'legacy', screen, step }
  }

  return { active: false, reason: 'none', screen, step }
}
