import type { ScenarioDocument, StepTransition } from '../types';

export type Runtime3EventType =
  | 'button'
  | 'serial'
  | 'timer'
  | 'audio_done'
  | 'unlock'
  | 'espnow'
  | 'action';

export interface Runtime3Transition {
  id: string;
  event_type: Runtime3EventType;
  event_name: string;
  target_step_id: string;
  priority: number;
  after_ms: number;
}

export interface Runtime3Step {
  id: string;
  scene_id: string;
  audio_pack_id: string;
  actions: string[];
  apps: string[];
  transitions: Runtime3Transition[];
}

export interface Runtime3Document {
  schema_version: 'zacus.runtime3.v1';
  scenario: {
    id: string;
    version: number;
    title: string;
    entry_step_id: string;
    source_kind: 'studio';
  };
  steps: Runtime3Step[];
  metadata: {
    generated_by: 'frontend-scratch-v2';
    migration_mode: 'native';
  };
}

function normalizeToken(raw: string, fallback: string): string {
  const cleaned = raw
    .trim()
    .replace(/[^a-zA-Z0-9_]+/g, '_')
    .replace(/^_+|_+$/g, '')
    .toUpperCase();
  return cleaned || fallback;
}

function normalizeEventType(raw: string): Runtime3EventType {
  const allowed: Runtime3EventType[] = [
    'button',
    'serial',
    'timer',
    'audio_done',
    'unlock',
    'espnow',
    'action',
  ];
  return allowed.includes(raw as Runtime3EventType)
    ? (raw as Runtime3EventType)
    : 'serial';
}

function normalizeTransition(
  transition: Pick<StepTransition, 'eventName' | 'targetStepId' | 'priority' | 'afterMs'> & {
    eventType: string;
  },
  index: number,
): Runtime3Transition {
  return {
    id: `TR_${index + 1}_${normalizeToken(transition.targetStepId, 'STEP_NEXT')}`,
    event_type: normalizeEventType(transition.eventType),
    event_name: normalizeToken(transition.eventName, 'BTN_NEXT'),
    target_step_id: normalizeToken(transition.targetStepId, 'STEP_NEXT'),
    priority: transition.priority ?? 0,
    after_ms: transition.afterMs ?? 0,
  };
}

export function compileScenarioDocumentToRuntime3(
  document: ScenarioDocument,
): Runtime3Document {
  const steps = (document.firmware?.steps ?? []).map((step, index) => ({
    id: normalizeToken(step.step_id, `STEP_${index + 1}`),
    scene_id: normalizeToken(step.screen_scene_id, `SCENE_${index + 1}`),
    audio_pack_id: normalizeToken(step.audio_pack_id, ''),
    actions: step.actions ?? [],
    apps: step.apps ?? [],
    transitions: (step.transitions ?? []).map((transition, transitionIndex) =>
      normalizeTransition(
        {
          eventType: transition.event_type,
          eventName: transition.event_name,
          targetStepId: transition.target_step_id,
          priority: transition.priority,
          afterMs: transition.after_ms,
        },
        transitionIndex,
      ),
    ),
  }));

  const entryStepId =
    normalizeToken(
      document.firmware?.initial_step ??
        document.firmware?.steps?.[0]?.step_id ??
        document.firmware?.steps_reference_order?.[0] ??
        document.steps_narrative[0]?.step_id ??
        '',
      'STEP_BOOT',
    ) || 'STEP_BOOT';

  return {
    schema_version: 'zacus.runtime3.v1',
    scenario: {
      id: normalizeToken(document.id, 'ZACUS_RUNTIME3'),
      version: document.version,
      title: document.title,
      entry_step_id: entryStepId,
      source_kind: 'studio',
    },
    steps,
    metadata: {
      generated_by: 'frontend-scratch-v2',
      migration_mode: 'native',
    },
  };
}

export function validateRuntime3Document(
  document: Runtime3Document,
): { ok: true } | { ok: false; error: string } {
  const stepIds = new Set<string>();

  if (document.steps.length === 0) {
    return { ok: false, error: 'runtime3 requires at least one step' };
  }

  for (const step of document.steps) {
    if (stepIds.has(step.id)) {
      return { ok: false, error: `duplicate runtime3 step id: ${step.id}` };
    }
    stepIds.add(step.id);
  }

  if (!stepIds.has(document.scenario.entry_step_id)) {
    return { ok: false, error: 'entry_step_id does not exist in runtime3 steps' };
  }

  for (const step of document.steps) {
    for (const transition of step.transitions) {
      if (!stepIds.has(transition.target_step_id)) {
        return {
          ok: false,
          error: `transition target missing: ${transition.target_step_id}`,
        };
      }
    }
  }

  return { ok: true };
}

export function runtime3ToJson(document: Runtime3Document): string {
  return JSON.stringify(document, null, 2);
}
