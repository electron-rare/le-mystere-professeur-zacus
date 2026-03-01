import YAML from 'yaml';
import { z } from 'zod';
import type { ScenarioDocument, ScenarioStep } from '../types';

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
  steps_reference_order: z.array(z.string().min(1)),
  steps_narrative: z.array(
    z.object({
      step_id: z.string().min(1),
      scene: z.string().min(1),
      narrative: z.string().min(1),
    }),
  ),
});

function normalizeId(raw: string, fallback: string): string {
  const cleaned = raw
    .trim()
    .replace(/[^a-zA-Z0-9_]+/g, '_')
    .replace(/^_+|_+$/g, '')
    .toUpperCase();
  return cleaned || fallback;
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
    };
  });

  const fallbackStep =
    normalizedSteps.length > 0
      ? normalizedSteps
      : [{ stepId: 'STEP_BOOT', sceneId: 'SCENE_BOOT' }];

  return {
    id: normalizeId(scenarioId, 'ZACUS_V2_NEW'),
    version: 1,
    title: 'Nouveau scenario Zacus',
    players: {
      min: 6,
      max: 14,
    },
    duration: {
      total_minutes: 90,
    },
    canon: {
      introduction: 'Scenario genere depuis frontend scratch-like.',
      stakes: 'Valider les transitions et deployer via API Story V2.',
    },
    stations: [],
    puzzles: [],
    steps_reference_order: fallbackStep.map((step) => step.stepId),
    steps_narrative: fallbackStep.map((step) => ({
      step_id: step.stepId,
      scene: step.sceneId,
      narrative: `Etape ${step.stepId} en scene ${step.sceneId}.`,
    })),
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

  const uniqueStepIds = new Set(parsed.data.steps_reference_order);
  if (uniqueStepIds.size !== parsed.data.steps_reference_order.length) {
    return { ok: false, error: 'duplicate step ids in steps_reference_order' };
  }

  return { ok: true };
}

export function scenarioToYaml(document: ScenarioDocument): string {
  return YAML.stringify(document);
}
