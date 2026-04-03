import type { GroupProfile, NpcProfilingConfig } from '@zacus/shared';

/**
 * Determine the group profile based on time taken to solve the first profiling puzzle.
 * - TECH: solves within tech_threshold_s seconds
 * - NON_TECH: takes longer than nontech_threshold_s seconds
 * - MIXED: in between
 */
export function detectGroupProfile(
  elapsedSeconds: number,
  config: NpcProfilingConfig,
): GroupProfile {
  if (elapsedSeconds <= config.tech_threshold_s) {
    return 'TECH';
  }
  if (elapsedSeconds >= config.nontech_threshold_s) {
    return 'NON_TECH';
  }
  return 'MIXED';
}

/**
 * Derive a confidence score [0..1] for the detected profile.
 * Used by the dashboard UI to display profiling certainty.
 */
export function profileConfidence(
  elapsedSeconds: number,
  config: NpcProfilingConfig,
): number {
  const range = config.nontech_threshold_s - config.tech_threshold_s;
  if (range <= 0) return 1;

  if (elapsedSeconds <= config.tech_threshold_s) {
    // How far below tech threshold — more confident the smaller the ratio
    return Math.min(1, 1 - elapsedSeconds / config.tech_threshold_s);
  }
  if (elapsedSeconds >= config.nontech_threshold_s) {
    // How far above nontech threshold
    const excess = elapsedSeconds - config.nontech_threshold_s;
    return Math.min(1, excess / config.nontech_threshold_s);
  }
  // In the mixed zone — confidence is how far from center
  const center = (config.tech_threshold_s + config.nontech_threshold_s) / 2;
  const distFromCenter = Math.abs(elapsedSeconds - center);
  return distFromCenter / (range / 2);
}
