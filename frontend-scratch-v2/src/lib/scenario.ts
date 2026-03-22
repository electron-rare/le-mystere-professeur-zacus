import YAML from 'yaml';
import { z } from 'zod';
import type { ScenarioDocument, ScenarioStep, StepTransition } from '../types';

const scenarioSchema = z.object({
  id: z.string().min(1),
  version: z.number().int().positive(),
  title: z.string().min(1),
  players: z.object({
    min: z.number().int().min(1),
    max: z.number().int().min(1),
  }),
  duration: z.object({
    total_minutes: z.number().int().positive(),
  }),
  canon: z.object({
    introduction: z.string().min(1),
    stakes: z.string().min(1),
  }),
  stations: z.array(z.record(z.string(), z.unknown())),
  puzzles: z.array(z.record(z.string(), z.unknown())),
  steps_narrative: z.array(
    z.object({
      step_id: z.string().min(1),
      scene: z.string().min(1),
      narrative: z.string().min(1),
    }),
  ),
  firmware: z
    .object({
      initial_step: z.string().min(1),
      steps_reference_order: z.array(z.string().min(1)).optional(),
      steps: z.array(
        z.object({
          step_id: z.string().min(1),
          screen_scene_id: z.string().min(1),
          audio_pack_id: z.string(),
          actions: z.array(z.string()),
          apps: z.array(z.string()),
          transitions: z.array(
            z.object({
              event_type: z.string().min(1),
              event_name: z.string().min(1),
              target_step_id: z.string().min(1),
              priority: z.number().int(),
              after_ms: z.number().int().min(0),
            }),
          ),
        }),
      ),
    })
    .optional(),
});

export function normalizeId(raw: string, fallback: string): string {
  const cleaned = raw
    .trim()
    .replace(/[^a-zA-Z0-9_]+/g, '_')
    .replace(/^_+|_+$/g, '')
    .toUpperCase();
  return cleaned || fallback;
}

function normalizeTransition(transition: StepTransition): StepTransition {
  return {
    eventType: transition.eventType,
    eventName: normalizeId(transition.eventName, 'BTN_NEXT'),
    targetStepId: normalizeId(transition.targetStepId, 'STEP_NEXT'),
    priority: transition.priority ?? 0,
    afterMs: transition.afterMs ?? 0,
  };
}

function resolveScenarioStepOrder(document: ScenarioDocument): string[] {
  if (Array.isArray(document.firmware?.steps) && document.firmware.steps.length > 0) {
    return document.firmware.steps.map((step) => step.step_id);
  }
  if (
    Array.isArray(document.firmware?.steps_reference_order) &&
    document.firmware.steps_reference_order.length > 0
  ) {
    return document.firmware.steps_reference_order;
  }
  return document.steps_narrative.map((step) => step.step_id);
}

export function buildScenarioFromBlocks(
  scenarioId: string,
  steps: ScenarioStep[],
): ScenarioDocument {
  const normalizedSteps = steps.map((step, index) => {
    const position = index + 1;
    return {
      stepId: normalizeId(step.stepId, `STEP_${position}`),
      sceneId: normalizeId(step.sceneId, `SCENE_${position}`),
      audioPack: step.audioPack ?? '',
      actions: step.actions ?? [],
      apps: step.apps ?? [],
      transitions: (step.transitions ?? []).map(normalizeTransition),
    };
  });

  const fallbackStep =
    normalizedSteps.length > 0
      ? normalizedSteps
      : [
          {
            stepId: 'STEP_BOOT',
            sceneId: 'SCENE_BOOT',
            audioPack: '',
            actions: [],
            apps: [],
            transitions: [],
          },
        ];

  const firmwareSteps = fallbackStep.map((step, i) => ({
    step_id: step.stepId,
    screen_scene_id: step.sceneId,
    audio_pack_id: step.audioPack,
    actions: step.actions,
    apps: step.apps,
    transitions: step.transitions.length
      ? step.transitions.map((transition) => ({
          event_type: transition.eventType,
          event_name: transition.eventName,
          target_step_id: transition.targetStepId,
          priority: transition.priority,
          after_ms: transition.afterMs,
        }))
      : i < fallbackStep.length - 1
        ? [
            {
              event_type: 'serial',
              event_name: 'BTN_NEXT',
              target_step_id: fallbackStep[i + 1].stepId,
              priority: 0,
              after_ms: 0,
            },
          ]
        : [],
  }));

  return {
    id: normalizeId(scenarioId, 'ZACUS_V2_NEW'),
    version: 1,
    title: 'Nouveau scenario Zacus',
    players: { min: 6, max: 14 },
    duration: { total_minutes: 105 },
    canon: {
      introduction: 'Scenario genere depuis frontend scratch-like.',
      stakes: 'Valider les transitions et deployer via API Story V2.',
    },
    stations: [],
    puzzles: [],
    steps_narrative: fallbackStep.map((s) => ({
      step_id: s.stepId,
      scene: s.sceneId,
      narrative: `Etape ${s.stepId} en scene ${s.sceneId}.`,
    })),
    firmware: {
      initial_step: fallbackStep[0].stepId,
      steps_reference_order: fallbackStep.map((step) => step.stepId),
      steps: firmwareSteps,
    },
  };
}

export function validateScenarioDocument(
  document: ScenarioDocument,
): { ok: true } | { ok: false; error: string } {
  const parsed = scenarioSchema.safeParse(document);
  if (!parsed.success) {
    return {
      ok: false,
      error: parsed.error.issues[0]?.message ?? 'schema validation error',
    };
  }

  if (parsed.data.players.min > parsed.data.players.max) {
    return { ok: false, error: 'players.min must be <= players.max' };
  }

  const stepOrder = resolveScenarioStepOrder(parsed.data);
  const uniqueStepIds = new Set(stepOrder);
  if (uniqueStepIds.size !== stepOrder.length) {
    return { ok: false, error: 'duplicate step ids in scenario step order' };
  }

  if (parsed.data.firmware) {
    const runtimeStepIds = new Set(parsed.data.firmware.steps.map((step) => step.step_id));
    if (!runtimeStepIds.has(parsed.data.firmware.initial_step)) {
      return { ok: false, error: 'firmware.initial_step must exist in firmware.steps' };
    }
    for (const step of parsed.data.firmware.steps) {
      for (const transition of step.transitions) {
        if (!runtimeStepIds.has(transition.target_step_id)) {
          return {
            ok: false,
            error: `transition target missing: ${transition.target_step_id}`,
          };
        }
      }
    }
    if (
      Array.isArray(parsed.data.firmware.steps_reference_order) &&
      parsed.data.firmware.steps_reference_order.length > 0 &&
      parsed.data.firmware.steps_reference_order.join('|') !==
        parsed.data.firmware.steps.map((step) => step.step_id).join('|')
    ) {
      return {
        ok: false,
        error: 'firmware.steps_reference_order must match firmware.steps order when both are present',
      };
    }
  }

  return { ok: true };
}

export function scenarioToYaml(document: ScenarioDocument): string {
  return YAML.stringify(document);
}

/** Parse a YAML string back into steps for the Blockly designer */
export function parseYamlToSteps(
  yamlStr: string,
): { id: string; steps: ScenarioStep[] } | { error: string } {
  try {
    const data = YAML.parse(yamlStr);
    if (!data || typeof data !== 'object') {
      return { error: 'Invalid YAML: not an object' };
    }

    const id = data.id ?? 'UNKNOWN';
    const steps: ScenarioStep[] = [];

    const normalizeTransitionList = (items: unknown): StepTransition[] => {
      if (!Array.isArray(items)) return [];
      return items.map((transition) => ({
        eventType:
          transition?.event_type ??
          transition?.eventType ??
          transition?.trigger ??
          'serial',
        eventName: transition?.event_name ?? transition?.eventName ?? 'BTN_NEXT',
        targetStepId:
          transition?.target_step_id ?? transition?.targetStepId ?? transition?.target ?? '',
        priority: Number(transition?.priority ?? 0),
        afterMs: Number(transition?.after_ms ?? transition?.afterMs ?? 0),
      }));
    };

    const fwSteps = data.runtime3?.steps ?? data.firmware?.steps ?? data.steps;
    if (Array.isArray(fwSteps)) {
      for (const s of fwSteps) {
        steps.push({
          stepId: s.id ?? s.step_id ?? s.stepId ?? '',
          sceneId: s.scene_id ?? s.screen_scene_id ?? s.sceneId ?? s.scene ?? '',
          audioPack: s.audio_pack_id ?? '',
          actions: s.actions ?? [],
          apps: s.apps ?? [],
          transitions: normalizeTransitionList(s.transitions),
        });
      }
    }

    // Fallback: steps_narrative
    if (steps.length === 0 && Array.isArray(data.steps_narrative)) {
      for (const s of data.steps_narrative) {
        steps.push({
          stepId: s.step_id ?? '',
          sceneId: s.scene ?? '',
          transitions: [],
        });
      }
    }

    // Fallback: firmware.steps_reference_order
    if (steps.length === 0 && Array.isArray(data.firmware?.steps_reference_order)) {
      for (const id of data.firmware.steps_reference_order) {
        steps.push({ stepId: id, sceneId: id, transitions: [] });
      }
    }

    if (steps.length === 0) {
      return { error: 'No steps found in YAML' };
    }

    return { id, steps };
  } catch (err) {
    return { error: `YAML parse error: ${err instanceof Error ? err.message : err}` };
  }
}
