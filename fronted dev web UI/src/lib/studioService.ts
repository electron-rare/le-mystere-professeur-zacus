import { stringify } from 'yaml'

export type ScenarioDifficulty = 'initiation' | 'standard' | 'expert'

export type ScenarioGeneratorInput = {
  scenarioId: string
  title: string
  missionSummary: string
  durationMinutes: number
  minPlayers: number
  maxPlayers: number
  difficulty: ScenarioDifficulty
  includeMediaManager: boolean
  customPrompt: string
  aiHint: string
}

export type GeneratedScenario = {
  yaml: string
  rationale: string
  source: 'local' | 'ai'
  generatedAt: string
}

type Transition = {
  trigger: 'on_event'
  event_type: string
  event_name: string
  target_step_id: string
  after_ms: number
  priority: number
}

export type PrintableTemplate = {
  id: string
  label: string
  category: string
  format: 'A4' | 'A5' | 'A6'
  sides?: 'recto' | 'verso' | 'recto_verso'
  variant?: string
  prompt: string
}

export type PrintableManifestItem = {
  id: string
  category: string
  format: string
  sides: string
  variant?: string
  prompt: string
}

export type PrintableManifest = {
  manifest_id: string
  version: number
  scenario_id: string
  title: string
  created_with: string
  items: PrintableManifestItem[]
}

export type GeneratedPrintables = {
  yaml: string
  markdown: string
}

const trimOr = (value: string, fallback: string) => {
  const next = value.trim()
  return next.length > 0 ? next : fallback
}

const sanitizeScenarioId = (value: string) => {
  const raw = (value || 'CUSTOM').trim().toUpperCase()
  const cleaned = raw.replace(/[^A-Z0-9_]/g, '_').replace(/_+/g, '_')
  return cleaned.length > 0 ? cleaned : 'CUSTOM'
}

const clamp = (value: number, min: number, max: number) => {
  const next = Number.isFinite(value) ? Math.round(value) : min
  return Math.min(max, Math.max(min, next))
}

const BASE_APP_BINDINGS = [
  { id: 'APP_AUDIO', app: 'AUDIO_PACK' },
  { id: 'APP_SCREEN', app: 'SCREEN_SCENE' },
  { id: 'APP_GATE', app: 'MP3_GATE' },
  { id: 'APP_WIFI', app: 'WIFI_STACK' },
  { id: 'APP_ESPNOW', app: 'ESPNOW_STACK' },
  { id: 'APP_QR_UNLOCK', app: 'QR_UNLOCK_APP' },
] as const

const BASE_STEPS = [
  { id: 'STEP_U_SON_PROTO', screen: 'SCENE_U_SON_PROTO', audio_pack: '', includeQr: false },
  { id: 'STEP_LA_DETECTOR', screen: 'SCENE_LA_DETECTOR', audio_pack: '', includeQr: false },
  { id: 'STEP_RTC_ESP_ETAPE1', screen: 'SCENE_WIN_ETAPE1', audio_pack: 'PACK_WIN1', includeQr: false },
  { id: 'STEP_WIN_ETAPE1', screen: 'SCENE_WIN_ETAPE1', audio_pack: 'PACK_WIN1', includeQr: false },
  { id: 'STEP_WARNING', screen: 'SCENE_WARNING', audio_pack: '', includeQr: false },
  { id: 'STEP_LEFOU_DETECTOR', screen: 'SCENE_LEFOU_DETECTOR', audio_pack: '', includeQr: false },
  { id: 'STEP_RTC_ESP_ETAPE2', screen: 'SCENE_WIN_ETAPE2', audio_pack: 'PACK_WIN2', includeQr: false },
  { id: 'STEP_QR_DETECTOR', screen: 'SCENE_QR_DETECTOR', audio_pack: 'PACK_QR', includeQr: true },
  { id: 'STEP_FINAL_WIN', screen: 'SCENE_FINAL_WIN', audio_pack: 'PACK_WIN3', includeQr: false },
  { id: 'SCENE_MEDIA_MANAGER', screen: 'SCENE_MEDIA_MANAGER', audio_pack: '', includeQr: false },
] as const

const STEP_TRANSITIONS: Record<string, Transition[]> = {
  STEP_U_SON_PROTO: [
    { trigger: 'on_event', event_type: 'audio_done', event_name: 'AUDIO_DONE', target_step_id: 'STEP_U_SON_PROTO', after_ms: 0, priority: 90 },
    { trigger: 'on_event', event_type: 'button', event_name: 'ANY', target_step_id: 'STEP_LA_DETECTOR', after_ms: 0, priority: 120 },
    { trigger: 'on_event', event_type: 'serial', event_name: 'FORCE_ETAPE2', target_step_id: 'STEP_LA_DETECTOR', after_ms: 0, priority: 130 },
  ],
  STEP_LA_DETECTOR: [
    { trigger: 'on_event', event_type: 'timer', event_name: 'ETAPE2_DUE', target_step_id: 'STEP_U_SON_PROTO', after_ms: 0, priority: 80 },
    { trigger: 'on_event', event_type: 'serial', event_name: 'BTN_NEXT', target_step_id: 'STEP_RTC_ESP_ETAPE1', after_ms: 0, priority: 110 },
    { trigger: 'on_event', event_type: 'unlock', event_name: 'UNLOCK', target_step_id: 'STEP_RTC_ESP_ETAPE1', after_ms: 0, priority: 120 },
    { trigger: 'on_event', event_type: 'action', event_name: 'ACTION_FORCE_ETAPE2', target_step_id: 'STEP_RTC_ESP_ETAPE1', after_ms: 0, priority: 130 },
    { trigger: 'on_event', event_type: 'serial', event_name: 'FORCE_WIN_ETAPE1', target_step_id: 'STEP_RTC_ESP_ETAPE1', after_ms: 0, priority: 140 },
  ],
  STEP_RTC_ESP_ETAPE1: [
    { trigger: 'on_event', event_type: 'espnow', event_name: 'ACK_WIN1', target_step_id: 'STEP_WIN_ETAPE1', after_ms: 0, priority: 130 },
    { trigger: 'on_event', event_type: 'serial', event_name: 'FORCE_DONE', target_step_id: 'STEP_WIN_ETAPE1', after_ms: 0, priority: 125 },
  ],
  STEP_WIN_ETAPE1: [
    { trigger: 'on_event', event_type: 'serial', event_name: 'BTN_NEXT', target_step_id: 'STEP_WARNING', after_ms: 0, priority: 120 },
    { trigger: 'on_event', event_type: 'serial', event_name: 'FORCE_DONE', target_step_id: 'STEP_WARNING', after_ms: 0, priority: 110 },
    { trigger: 'on_event', event_type: 'espnow', event_name: 'ACK_WARNING', target_step_id: 'STEP_WARNING', after_ms: 0, priority: 125 },
  ],
  STEP_WARNING: [
    { trigger: 'on_event', event_type: 'audio_done', event_name: 'AUDIO_DONE', target_step_id: 'STEP_WARNING', after_ms: 0, priority: 80 },
    { trigger: 'on_event', event_type: 'button', event_name: 'ANY', target_step_id: 'STEP_LEFOU_DETECTOR', after_ms: 0, priority: 120 },
    { trigger: 'on_event', event_type: 'serial', event_name: 'FORCE_ETAPE2', target_step_id: 'STEP_LEFOU_DETECTOR', after_ms: 0, priority: 130 },
  ],
  STEP_LEFOU_DETECTOR: [
    { trigger: 'on_event', event_type: 'timer', event_name: 'ETAPE2_DUE', target_step_id: 'STEP_WARNING', after_ms: 0, priority: 100 },
    { trigger: 'on_event', event_type: 'serial', event_name: 'BTN_NEXT', target_step_id: 'STEP_RTC_ESP_ETAPE2', after_ms: 0, priority: 110 },
    { trigger: 'on_event', event_type: 'unlock', event_name: 'UNLOCK', target_step_id: 'STEP_RTC_ESP_ETAPE2', after_ms: 0, priority: 115 },
    { trigger: 'on_event', event_type: 'action', event_name: 'ACTION_FORCE_ETAPE2', target_step_id: 'STEP_RTC_ESP_ETAPE2', after_ms: 0, priority: 125 },
    { trigger: 'on_event', event_type: 'serial', event_name: 'FORCE_WIN_ETAPE2', target_step_id: 'STEP_RTC_ESP_ETAPE2', after_ms: 0, priority: 130 },
  ],
  STEP_RTC_ESP_ETAPE2: [
    { trigger: 'on_event', event_type: 'espnow', event_name: 'ACK_WIN2', target_step_id: 'STEP_QR_DETECTOR', after_ms: 0, priority: 130 },
    { trigger: 'on_event', event_type: 'serial', event_name: 'FORCE_DONE', target_step_id: 'STEP_QR_DETECTOR', after_ms: 0, priority: 120 },
  ],
  STEP_QR_DETECTOR: [
    { trigger: 'on_event', event_type: 'timer', event_name: 'ETAPE2_DUE', target_step_id: 'STEP_WARNING', after_ms: 0, priority: 100 },
    { trigger: 'on_event', event_type: 'serial', event_name: 'BTN_NEXT', target_step_id: 'STEP_FINAL_WIN', after_ms: 0, priority: 110 },
    { trigger: 'on_event', event_type: 'unlock', event_name: 'UNLOCK_QR', target_step_id: 'STEP_FINAL_WIN', after_ms: 0, priority: 140 },
    { trigger: 'on_event', event_type: 'action', event_name: 'ACTION_FORCE_ETAPE2', target_step_id: 'STEP_FINAL_WIN', after_ms: 0, priority: 125 },
    { trigger: 'on_event', event_type: 'serial', event_name: 'FORCE_WIN_ETAPE2', target_step_id: 'STEP_FINAL_WIN', after_ms: 0, priority: 130 },
  ],
  STEP_FINAL_WIN: [
    { trigger: 'on_event', event_type: 'timer', event_name: 'WIN_DUE', target_step_id: 'SCENE_MEDIA_MANAGER', after_ms: 0, priority: 140 },
    { trigger: 'on_event', event_type: 'serial', event_name: 'BTN_NEXT', target_step_id: 'SCENE_MEDIA_MANAGER', after_ms: 0, priority: 120 },
    { trigger: 'on_event', event_type: 'unlock', event_name: 'UNLOCK', target_step_id: 'SCENE_MEDIA_MANAGER', after_ms: 0, priority: 115 },
    { trigger: 'on_event', event_type: 'action', event_name: 'FORCE_WIN_ETAPE2', target_step_id: 'SCENE_MEDIA_MANAGER', after_ms: 0, priority: 130 },
    { trigger: 'on_event', event_type: 'serial', event_name: 'FORCE_WIN_ETAPE2', target_step_id: 'SCENE_MEDIA_MANAGER', after_ms: 0, priority: 135 },
  ],
  SCENE_MEDIA_MANAGER: [
    { trigger: 'on_event', event_type: 'serial', event_name: 'END', target_step_id: 'SCENE_MEDIA_MANAGER', after_ms: 0, priority: 10 },
  ],
}

const printableTemplates: PrintableTemplate[] = [
  { id: 'invitation_a6_recto', label: 'Invitation A6 — Recto', category: 'invitation', format: 'A6', sides: 'recto', prompt: 'src/prompts/invitation_recto.md' },
  { id: 'invitation_a6_verso', label: 'Invitation A6 — Verso', category: 'invitation', format: 'A6', sides: 'verso', prompt: 'src/prompts/invitation_verso.md' },
  { id: 'carte_personnage', label: 'Cartes personnages', category: 'personnage', format: 'A6', sides: 'recto', prompt: 'src/prompts/card_personnage.md' },
  { id: 'carte_lieu', label: 'Cartes lieux', category: 'lieu', format: 'A6', sides: 'recto', prompt: 'src/prompts/card_lieu.md' },
  { id: 'carte_objet', label: 'Cartes objets', category: 'objet', format: 'A6', sides: 'recto', prompt: 'src/prompts/card_objet.md' },
  { id: 'fiche_enquete_a4', label: 'Fiche enquête A4', category: 'fiche_enquete', format: 'A4', sides: 'recto_verso', prompt: 'src/prompts/fiche_enquete.md' },
  { id: 'badge_detective', label: 'Badge détective', category: 'badge', format: 'A6', sides: 'recto', prompt: 'src/prompts/badge_detective.md' },
  { id: 'regles_a4', label: 'Règles A4', category: 'regles', format: 'A4', sides: 'recto', prompt: 'src/prompts/regles.md' },
  { id: 'hotline_a4', label: 'Hotline Professeur Zacus', category: 'hotline', format: 'A4', sides: 'recto', prompt: 'src/prompts/hotline.md' },
  { id: 'zone_z1', label: 'Affiche zone Z1', category: 'zone', format: 'A4', variant: 'Z1', sides: 'recto', prompt: 'src/prompts/zone_affiche.md' },
  { id: 'zone_z2', label: 'Affiche zone Z2', category: 'zone', format: 'A4', variant: 'Z2', sides: 'recto', prompt: 'src/prompts/zone_affiche.md' },
  { id: 'zone_z3', label: 'Affiche zone Z3', category: 'zone', format: 'A4', variant: 'Z3', sides: 'recto', prompt: 'src/prompts/zone_affiche.md' },
  { id: 'zone_z4', label: 'Affiche zone Z4', category: 'zone', format: 'A4', variant: 'Z4', sides: 'recto', prompt: 'src/prompts/zone_affiche.md' },
  { id: 'zone_z5', label: 'Affiche zone Z5', category: 'zone', format: 'A4', variant: 'Z5', sides: 'recto', prompt: 'src/prompts/zone_affiche.md' },
  { id: 'zone_z6', label: 'Affiche zone Z6', category: 'zone', format: 'A4', variant: 'Z6', sides: 'recto', prompt: 'src/prompts/zone_affiche.md' },
]

const normalizeYamlText = (value: unknown) => (typeof value === 'string' ? value.trim() : '')

const normalizeBlueprint = (input: ScenarioGeneratorInput): Required<ScenarioGeneratorInput> => {
  const scenarioId = sanitizeScenarioId(input.scenarioId)
  return {
    scenarioId,
    title: trimOr(input.title, 'Scenario personnalisé'),
    missionSummary: trimOr(input.missionSummary, 'Enquête immersive de coopération. Réactiver la chaîne de progression jusqu’aux étapes finales.'),
    durationMinutes: clamp(Math.round(input.durationMinutes), 20, 360),
    minPlayers: clamp(Math.round(input.minPlayers || 4), 2, 30),
    maxPlayers: clamp(Math.round(input.maxPlayers || 16), 2, 60),
    difficulty: input.difficulty || 'standard',
    includeMediaManager: Boolean(input.includeMediaManager),
    customPrompt: normalizeYamlText(input.customPrompt),
    aiHint: normalizeYamlText(input.aiHint),
  }
}

const buildLocalYamlFromBlueprint = (input: ScenarioGeneratorInput): GeneratedScenario => {
  const blueprint = normalizeBlueprint(input)
  const appBindings = BASE_APP_BINDINGS.map((entry) => ({
    id: entry.id,
    app: entry.app,
  }))

  const steps = BASE_STEPS.map((step, index) => {
    const transitions = STEP_TRANSITIONS[step.id]?.map((transition) => ({
      trigger: transition.trigger,
      event_type: transition.event_type,
      event_name: transition.event_name,
      target_step_id: transition.target_step_id,
      after_ms: transition.after_ms,
      priority: transition.priority,
    }))

    return {
      step_id: step.id,
      screen_scene_id: step.screen,
      audio_pack_id: step.audio_pack,
      actions: ['ACTION_TRACE_STEP'],
      apps: BASE_APP_BINDINGS.map((entry) => entry.id),
      mp3_gate_open: false,
      transitions,
      ...(step.id === 'SCENE_MEDIA_MANAGER' && blueprint.includeMediaManager
        ? {
            apps: [...BASE_APP_BINDINGS.map((entry) => entry.id), 'APP_MEDIA'],
          }
        : {}),
      ...(step.id === 'STEP_QR_DETECTOR' && step.includeQr
        ? {
            mp3_gate_open: blueprint.difficulty === 'expert',
          }
        : {}),
      ...(index === 0 ? { is_initial: true } : {}),
    }
  })

  const scenario = {
    id: blueprint.scenarioId,
    version: 2,
    title: blueprint.title,
    duration_minutes: blueprint.durationMinutes,
    players: {
      min: Math.min(blueprint.minPlayers, blueprint.maxPlayers),
      max: Math.max(blueprint.minPlayers, blueprint.maxPlayers),
    },
    theme: blueprint.missionSummary,
    difficulty: blueprint.difficulty,
    initial_step: 'STEP_U_SON_PROTO',
    debug_transition_bypass_enabled: false,
    app_bindings: appBindings,
    steps,
    note: blueprint.customPrompt || blueprint.aiHint || 'Scénario généré depuis le Studio frontend.',
  }

  return {
    yaml: stringify(scenario),
    rationale: `Scénario généré localement (template officiel de Zacus) — branche média=${blueprint.includeMediaManager ? 'activée' : 'désactivée'}.`,
    source: 'local',
    generatedAt: new Date().toISOString(),
  }
}

const generateScenarioViaAi = async (input: ScenarioGeneratorInput): Promise<GeneratedScenario | null> => {
  const endpoint = import.meta.env.VITE_ZACUS_STUDIO_AI_URL
  if (!endpoint) {
    return null
  }

  try {
    const response = await fetch(endpoint, {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
      },
      body: JSON.stringify({
        mode: 'story_generate',
        scenario: normalizeBlueprint(input),
      }),
    })

    if (!response.ok) {
      return null
    }

    const payload = (await response.json()) as {
      yaml?: unknown
      rationale?: unknown
    }

    const yaml = normalizeYamlText(payload.yaml)
    if (!yaml) {
      return null
    }

    return {
      yaml,
      rationale: normalizeYamlText(payload.rationale) || 'Scénario généré via service IA.',
      source: 'ai',
      generatedAt: new Date().toISOString(),
    }
  } catch {
    return null
  }
}

const generatePrintablesViaAi = async (
  scenarioId: string,
  title: string,
  selected: string[],
): Promise<GeneratedPrintables | null> => {
  const endpoint = import.meta.env.VITE_ZACUS_STUDIO_AI_URL
  if (!endpoint) {
    return null
  }

  try {
    const response = await fetch(endpoint, {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
      },
      body: JSON.stringify({
        mode: 'printables_plan',
        scenarioId,
        title,
        selected,
      }),
    })

    if (!response.ok) {
      return null
    }

    const payload = (await response.json()) as {
      manifest_yaml?: unknown
      markdown?: unknown
    }

    const manifestYaml = normalizeYamlText(payload.manifest_yaml)
    const markdown = normalizeYamlText(payload.markdown)
    if (!manifestYaml || !markdown) {
      return null
    }

    return { yaml: manifestYaml, markdown }
  } catch {
    return null
  }
}

export const generateScenario = async (input: ScenarioGeneratorInput): Promise<GeneratedScenario> => {
  const aiResult = await generateScenarioViaAi(input)
  if (aiResult) {
    return aiResult
  }
  return buildLocalYamlFromBlueprint(input)
}

export const generatePrintables = async (
  scenarioId: string,
  title: string,
  selected: string[],
): Promise<GeneratedPrintables> => {
  const aiResult = await generatePrintablesViaAi(scenarioId, title, selected)
  if (aiResult) {
    return aiResult
  }

  const pickedIds = new Set(selected)
  const manifestItems = printableTemplates
    .filter((entry) => pickedIds.has(entry.id))
    .map((entry) => ({
      id: entry.id,
      category: entry.category,
      format: entry.format,
      sides: entry.sides ?? 'recto',
      variant: entry.variant,
      prompt: entry.prompt,
    }))

  const manifest: PrintableManifest = {
    manifest_id: `${sanitizeScenarioId(scenarioId).toLowerCase()}_printables`,
    version: 1,
    scenario_id: sanitizeScenarioId(scenarioId),
    title: title.trim() || 'Plan imprimables',
    created_with: 'frontend-studio-v1',
    items: manifestItems,
  }

  const yaml = stringify(manifest)
  const markdown = [
    `# Pack imprimables — ${title.trim() || 'Plan imprimables'}`,
    '',
    `- Scenario: ${sanitizeScenarioId(scenarioId)}`,
    `- Items: ${manifestItems.length}`,
    '',
    ...manifestItems.map((entry) => `- ${entry.id} (${entry.category}) — ${entry.format} ${entry.variant ? `/ ${entry.variant}` : ''}`),
    '',
    'Généré depuis le Studio frontend.',
    '',
  ].join('\n')

  return { yaml, markdown }
}

export const getPrintableTemplates = () => printableTemplates
