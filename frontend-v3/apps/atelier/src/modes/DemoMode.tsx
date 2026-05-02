import { useEffect } from 'react';
import { useFrame, useThree } from '@react-three/fiber';
import * as THREE from 'three';
import { useSimStore } from '../stores/simStore.js';

// Cinematic camera waypoints (position + target)
const WAYPOINTS: Array<{ pos: [number, number, number]; target: [number, number, number]; duration: number }> = [
  { pos: [0, 3, 6], target: [0, 0, 0], duration: 5000 },
  { pos: [-4, 2, -2], target: [-4, 0, -2], duration: 4000 },
  { pos: [-4, 2, 2], target: [-4, 0, 2], duration: 4000 },
  { pos: [0, 2, -3.5], target: [0, 0, -3.5], duration: 4000 },
  { pos: [4, 2, -2], target: [4, 0, -2], duration: 4000 },
  { pos: [4, 2, 2], target: [4, 0, 2], duration: 4000 },
  { pos: [0, 4, 0], target: [0, 0, 0], duration: 6000 },
];

/**
 * DemoMode: auto-play with cinematic camera moving between puzzle stations.
 * Mount inside a Canvas as a child component (uses useFrame + useThree).
 */
export function DemoCameraController() {
  const { camera } = useThree();
  const mode = useSimStore((s) => s.mode);

  let waypointIndex = 0;
  let elapsed = 0;

  useFrame((_, delta) => {
    if (mode !== 'demo') return;
    elapsed += delta * 1000;
    const wp = WAYPOINTS[waypointIndex % WAYPOINTS.length]!;

    if (elapsed >= wp.duration) {
      elapsed = 0;
      waypointIndex = (waypointIndex + 1) % WAYPOINTS.length;
    }

    const t = Math.min(elapsed / wp.duration, 1);
    const smoothT = t < 0.5 ? 2 * t * t : -1 + (4 - 2 * t) * t; // ease in-out

    const targetPos = new THREE.Vector3(...wp.pos);
    camera.position.lerp(targetPos, smoothT * delta * 2);

    const lookTarget = new THREE.Vector3(...wp.target);
    camera.lookAt(lookTarget);
  });

  return null;
}

/**
 * DemoMode component: auto-solves puzzles at intervals for showcase.
 */
export function DemoMode() {
  const { solvePuzzle, solvedPuzzles } = useSimStore();
  const PUZZLE_SEQUENCE = ['P1_SON', 'P2_CIRCUIT', 'P3_QR', 'P4_RADIO', 'P5_MORSE', 'P6_SYMBOLES', 'P7_COFFRE'];

  useEffect(() => {
    if (solvedPuzzles.length >= PUZZLE_SEQUENCE.length) return;

    const timer = setInterval(() => {
      const nextPuzzle = PUZZLE_SEQUENCE[solvedPuzzles.length];
      if (nextPuzzle && !solvedPuzzles.includes(nextPuzzle)) {
        solvePuzzle(nextPuzzle);
      }
    }, 8000);

    return () => clearInterval(timer);
  }, [solvedPuzzles.length, solvePuzzle]);

  return null;
}
