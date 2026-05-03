import { describe, it, expect, beforeEach } from 'vitest';
import { ZacusScenarioEngine } from '../src/engine.js';
import { readFileSync } from 'node:fs';
import { resolve, dirname } from 'node:path';
import { fileURLToPath } from 'node:url';

const __dirname = dirname(fileURLToPath(import.meta.url));
const YAML_PATH = resolve(__dirname, '../../../../game/scenarios/zacus_v3_complete.yaml');
const yamlRaw = readFileSync(YAML_PATH, 'utf-8');

describe('ZacusScenarioEngine', () => {
  let engine: ZacusScenarioEngine;

  beforeEach(() => {
    engine = new ZacusScenarioEngine();
    engine.load(yamlRaw);
    engine.start({ targetDuration: 60, mode: '60' });
  });

  it('starts in PROFILING phase', () => {
    const state = engine.tick(Date.now());
    expect(state.phase).toBe('PROFILING');
  });

  it('records solved puzzles on puzzle_solved event', () => {
    const decisions = engine.onEvent({
      type: 'puzzle_solved',
      timestamp: Date.now(),
      data: { puzzle_id: 'P1_SON' },
    });
    expect(engine.getState().solvedPuzzles).toContain('P1_SON');
    expect(decisions.some((d) => d.action === 'change_mood')).toBe(true);
  });

  it('tracks hints per puzzle', () => {
    engine.onEvent({ type: 'hint_given', timestamp: Date.now(), data: { puzzle_id: 'P2_CIRCUIT' } });
    engine.onEvent({ type: 'hint_given', timestamp: Date.now(), data: { puzzle_id: 'P2_CIRCUIT' } });
    expect(engine.getState().hintsGiven['P2_CIRCUIT']).toBe(2);
  });

  it('getScore in progress: base only, no bonus until completion', () => {
    // At t=0 the game is not yet finished (state.completed === false).
    // The fast-completion bonus must not be promised pre-emptively.
    const score = engine.getScore();
    expect(score.baseScore).toBe(1000);
    expect(score.timePenalty).toBe(0);
    expect(score.hintPenalty).toBe(0);
    expect(score.bonus).toBe(0);
    expect(score.total).toBe(1000);
  });

  it('getScore after game_end with fast time: base + fast bonus', () => {
    // game_end at t=0 (elapsedMin = 0 < 48 = targetMin * 0.8) earns the
    // bonus_fast_completion (200) on top of base_score (1000).
    engine.onEvent({ type: 'game_end', timestamp: Date.now(), data: {} });
    const score = engine.getScore();
    expect(score.baseScore).toBe(1000);
    expect(score.bonus).toBe(200);
    expect(score.total).toBe(1200);
  });

  it('sets groupProfile on profile_detected event', () => {
    engine.onEvent({ type: 'profile_detected', timestamp: Date.now(), data: { profile: 'TECH' } });
    expect(engine.getState().groupProfile).toBe('TECH');
  });

  it('advances phase based on elapsed time', () => {
    // 55 minutes in -- should be in CLIMAX (60 - 5 outro < 55 < 60)
    const fakeNow = Date.now() + 55 * 60 * 1000;
    const state = engine.tick(fakeNow);
    expect(['CLIMAX', 'OUTRO']).toContain(state.phase);
  });

  it('assembles code digits on puzzle_solved', () => {
    engine.onEvent({
      type: 'puzzle_solved',
      timestamp: Date.now(),
      data: { puzzle_id: 'P1_SON' },
    });
    // codeAssembled should be non-empty if P1_SON has code_digits
    const state = engine.getState();
    expect(typeof state.codeAssembled).toBe('string');
  });

  it('records skipped puzzles on puzzle_skipped event', () => {
    engine.onEvent({
      type: 'puzzle_skipped',
      timestamp: Date.now(),
      data: { puzzle_id: 'P3_QR' },
    });
    expect(engine.getState().skippedPuzzles).toContain('P3_QR');
  });

  it('transitions to OUTRO on game_end', () => {
    engine.onEvent({ type: 'game_end', timestamp: Date.now(), data: {} });
    expect(engine.getState().phase).toBe('OUTRO');
  });
});
