import type { ScenarioYaml } from '@zacus/shared';

export interface ValidationResult {
  valid: boolean;
  errors: string[];
  warnings: string[];
  summary: string;
}

/**
 * Validate a parsed ScenarioYaml against 6 rules derived from the Zacus V3 spec.
 * Returns a ValidationResult with errors, warnings, and a human-readable summary.
 */
export function validateScenario(scenario: ScenarioYaml): ValidationResult {
  const errors: string[] = [];
  const warnings: string[] = [];

  // Rule 1: All puzzle code_digits must sum to exactly 8 total digits
  const totalDigits = scenario.puzzles.reduce((sum, p) => {
    const count = (p.code_digits?.length ?? 0) + (p.code_digit != null ? 1 : 0);
    return sum + count;
  }, 0);
  if (totalDigits !== 8) {
    errors.push(`Les digits de code totalisent ${totalDigits} chiffre(s) — attendu 8`);
  }

  // Rule 2: Every puzzle must be referenced in at least one phase
  const referencedPuzzles = new Set(
    scenario.phases.flatMap((ph) => ph.puzzles.map((p) => p.puzzle_ref)),
  );
  for (const puzzle of scenario.puzzles) {
    if (!referencedPuzzles.has(puzzle.id)) {
      errors.push(`Puzzle ${puzzle.id} n'est référencé dans aucune phase`);
    }
  }

  // Rule 3: PROFILING phase must have exactly 2 puzzles (one TECH, one BOTH)
  const profilingPhase = scenario.phases.find((ph) => ph.type === 'PROFILING');
  if (!profilingPhase) {
    errors.push('Aucune phase PROFILING définie');
  } else {
    if (profilingPhase.puzzles.length !== 2) {
      errors.push(
        `Phase PROFILING: ${profilingPhase.puzzles.length} puzzle(s) — attendu 2`,
      );
    }
    const filters = profilingPhase.puzzles.map((p) => p.profile_filter);
    if (!filters.includes('TECH')) warnings.push('Phase PROFILING: aucun puzzle TECH');
    if (!filters.includes('BOTH')) warnings.push('Phase PROFILING: aucun puzzle BOTH');
  }

  // Rule 4: CLIMAX phase must exist and contain P7_COFFRE
  const climaxPhase = scenario.phases.find((ph) => ph.type === 'CLIMAX');
  if (!climaxPhase) {
    errors.push('Aucune phase CLIMAX définie');
  } else {
    const hasCoffre = climaxPhase.puzzles.some((p) => p.puzzle_ref === 'P7_COFFRE');
    if (!hasCoffre) {
      errors.push('Phase CLIMAX: P7_COFFRE (Coffre Final) absent');
    }
  }

  // Rule 5: Duration target must be configured
  if (!scenario.duration.target_minutes || scenario.duration.target_minutes <= 0) {
    errors.push('Durée cible non configurée ou invalide');
  }

  // Rule 6: Scenario must have at least 3 phases (PROFILING, ADAPTIVE, CLIMAX)
  const requiredPhases = ['PROFILING', 'ADAPTIVE', 'CLIMAX'] as const;
  for (const required of requiredPhases) {
    if (!scenario.phases.some((ph) => ph.type === required)) {
      errors.push(`Phase ${required} manquante`);
    }
  }

  const valid = errors.length === 0;
  const puzzleCount = scenario.puzzles.length;
  const durationMin = scenario.duration.target_minutes;
  const summary = valid
    ? `Scénario valide — ${puzzleCount} puzzles, ~${durationMin} min`
    : `${errors.length} erreur(s) à corriger`;

  return { valid, errors, warnings, summary };
}
