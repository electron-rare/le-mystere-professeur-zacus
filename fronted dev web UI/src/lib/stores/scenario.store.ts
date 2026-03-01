import { get } from 'svelte/store'
import {
  deployStory,
  pauseStory,
  resumeStory,
  selectStory,
  skipStory,
  startStory,
  unlockStory,
  validateStory,
} from '../deviceApi'
import { runtimeStore } from './runtime.store'

const normalizeScenarioIdFromYaml = (text: string) => {
  const match = text.match(/^\s*id\s*:\s*([^\n#]+)/im)
  if (!match?.[1]) {
    return ''
  }
  return match[1].trim().toUpperCase()
}

const extractDeployedScenario = (payload: unknown, yaml: string) => {
  if (!payload || typeof payload !== 'object') {
    return normalizeScenarioIdFromYaml(yaml)
  }
  const record = payload as Record<string, unknown>
  if (typeof record.deployed === 'string') {
    return record.deployed.trim()
  }
  if (typeof record.scenario_id === 'string') {
    return record.scenario_id.trim()
  }
  if (typeof record.scenario === 'string') {
    return record.scenario.trim()
  }
  return normalizeScenarioIdFromYaml(yaml)
}

export const scenarioStore = {
  start: async (scenarioId: string) => {
    await runtimeStore.runAction('start', async () => {
      const state = get(runtimeStore)
      if (state.capabilities.canSelectScenario) {
        await selectStory(scenarioId)
      }
      if (state.capabilities.canStart) {
        await startStory()
      }
      runtimeStore.setActiveScenario(scenarioId)
    })
    await runtimeStore.refreshSnapshot()
  },

  pause: async () => {
    await runtimeStore.runAction('pause', pauseStory)
    await runtimeStore.refreshSnapshot()
  },

  resume: async () => {
    await runtimeStore.runAction('resume', resumeStory)
    await runtimeStore.refreshSnapshot()
  },

  skip: async () => {
    await runtimeStore.runAction('skip', skipStory)
    await runtimeStore.refreshSnapshot()
  },

  unlock: async () => {
    await runtimeStore.runAction('unlock', unlockStory)
    await runtimeStore.refreshSnapshot()
  },

  validateYaml: async (yaml: string) => {
    if (!yaml.trim()) {
      runtimeStore.setActionLog('Ajouter un YAML avant validation.')
      return
    }

    await runtimeStore.runAction('validate', async () => {
      const result = (await validateStory(yaml)) as { valid?: boolean; errors?: string[]; error?: string }
      if (typeof result.error === 'string' && result.error.trim()) {
        throw new Error(result.error)
      }
      runtimeStore.setActionLog(result.valid ? 'Validation OK' : `Validation KO: ${(result.errors ?? []).join(' · ') || 'erreur inconnue'}`)
    })
  },

  deployYaml: async (yaml: string) => {
    if (!yaml.trim()) {
      runtimeStore.setActionLog('Ajouter un YAML avant déploiement.')
      return
    }

    await runtimeStore.runAction('deploy', async () => {
      const result = await deployStory(yaml)
      const payload = result as Record<string, unknown>
      if (typeof payload.error === 'string' && payload.error.trim()) {
        throw new Error(payload.error)
      }

      const deployed = extractDeployedScenario(payload, yaml)
      if (!deployed) {
        throw new Error('Le runtime n’a pas renvoyé d’identifiant de scénario.')
      }

      runtimeStore.setActionLog(`Scénario déployé: ${deployed}`)
      runtimeStore.setActiveScenario(deployed)
    })

    await runtimeStore.refreshSnapshot()
  },

  deployAndStart: async (yaml: string) => {
    const state = get(runtimeStore)
    if (!state.capabilities.canSelectScenario || !state.capabilities.canStart) {
      throw new Error('Test run non supporté par ce runtime.')
    }

    let deployed = ''
    await runtimeStore.runAction('test-run', async () => {
      const result = await deployStory(yaml)
      deployed = extractDeployedScenario(result, yaml)
      if (!deployed) {
        throw new Error('Déploiement de test impossible (ID absent).')
      }
      await selectStory(deployed)
      await startStory()
    })
    runtimeStore.setActionLog(`Test run lancé: ${deployed}`)
    await runtimeStore.refreshSnapshot()
  },
}
