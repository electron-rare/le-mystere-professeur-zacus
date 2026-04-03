import { useRef, useState } from 'react';
import { Text } from '@react-three/drei';
import * as THREE from 'three';
import { useSimStore } from '../store/simStore.js';
import { P1Sound } from '../puzzles/P1Sound.js';
import { P5Morse } from '../puzzles/P5Morse.js';
import { P6Symbols } from '../puzzles/P6Symbols.js';
import { P7Coffre } from '../puzzles/P7Coffre.js';

// Positions arranged around the central table (polar layout)
const STATION_POSITIONS: Record<string, [number, number, number]> = {
  P1_SON:      [-4, 0, -2],
  P2_CIRCUIT:  [-4, 0,  2],
  P3_QR:       [ 0, 0, -3.5],
  P4_RADIO:    [ 4, 0, -2],
  P5_MORSE:    [ 4, 0,  2],
  P6_SYMBOLES: [ 0, 0,  3.5],
  P7_COFFRE:   [ 5, 0,  0],
};

function playTone(hz: number, duration = 0.2) {
  const ctx = new AudioContext();
  const osc = ctx.createOscillator();
  const gain = ctx.createGain();
  osc.frequency.value = hz;
  gain.gain.setValueAtTime(0.3, ctx.currentTime);
  gain.gain.exponentialRampToValueAtTime(0.001, ctx.currentTime + duration);
  osc.connect(gain).connect(ctx.destination);
  osc.start();
  osc.stop(ctx.currentTime + duration);
}

export function PuzzleStations() {
  return (
    <group>
      <P1Sound position={STATION_POSITIONS['P1_SON']!} />
      <P2CircuitLed position={STATION_POSITIONS['P2_CIRCUIT']!} />
      <P3QrTreasure position={STATION_POSITIONS['P3_QR']!} />
      <P4Radio position={STATION_POSITIONS['P4_RADIO']!} />
      <P5Morse position={STATION_POSITIONS['P5_MORSE']!} />
      <P6Symbols position={STATION_POSITIONS['P6_SYMBOLES']!} />
      <P7Coffre position={STATION_POSITIONS['P7_COFFRE']!} />
    </group>
  );
}

// ---- P2: Circuit LED ----
function P2CircuitLed({ position }: { position: [number, number, number] }) {
  const { solvePuzzle } = useSimStore();
  return (
    <group position={position}>
      <mesh castShadow onClick={() => solvePuzzle('P2_CIRCUIT')}>
        <boxGeometry args={[0.9, 0.1, 0.7]} />
        <meshStandardMaterial color="#1a2a1a" />
      </mesh>
      <Text position={[0, 0.3, 0]} fontSize={0.1} color="white" anchorX="center">
        P2 Circuit
      </Text>
    </group>
  );
}

// ---- P3: QR Treasure ----
function P3QrTreasure({ position }: { position: [number, number, number] }) {
  const { solvePuzzle } = useSimStore();
  const [scanned, setScanned] = useState(0);

  const handleScan = (i: number) => {
    if (i !== scanned) return; // must scan in order
    const next = scanned + 1;
    setScanned(next);
    if (next === 6) solvePuzzle('P3_QR');
  };

  return (
    <group position={position}>
      {[0, 1, 2, 3, 4, 5].map((i) => (
        <mesh
          key={i}
          position={[(i % 3) * 0.3 - 0.3, 1.5, -3.4 + Math.floor(i / 3) * 0.4]}
          onClick={() => handleScan(i)}
        >
          <boxGeometry args={[0.25, 0.25, 0.02]} />
          <meshStandardMaterial color={i < scanned ? '#34c759' : '#f5f5dc'} />
        </mesh>
      ))}
      <Text position={[0, 2.2, -3.4]} fontSize={0.1} color="white" anchorX="center">
        P3 QR
      </Text>
    </group>
  );
}

// ---- P4: Radio ----
function P4Radio({ position }: { position: [number, number, number] }) {
  const { solvePuzzle, mode } = useSimStore();
  const dialRef = useRef<THREE.Mesh>(null);
  const [freq, setFreq] = useState(88.0);
  const TARGET_FREQ = 93.5;

  const handleDialDrag = (delta: number) => {
    if (mode === 'demo') return;
    const newFreq = Math.max(88, Math.min(108, freq + delta * 0.1));
    const rounded = parseFloat(newFreq.toFixed(1));
    setFreq(rounded);
    if (Math.abs(rounded - TARGET_FREQ) < 0.2) {
      solvePuzzle('P4_RADIO');
    }
  };

  return (
    <group position={position}>
      {/* Radio body */}
      <mesh castShadow>
        <boxGeometry args={[1.2, 0.6, 0.5]} />
        <meshStandardMaterial color="#8B4513" roughness={0.8} />
      </mesh>
      {/* Dial */}
      <mesh
        ref={dialRef}
        position={[0.3, 0.1, 0.26]}
        onPointerMove={(e) => {
          if (e.buttons === 1) handleDialDrag(e.movementX);
        }}
      >
        <cylinderGeometry args={[0.12, 0.12, 0.05, 16]} />
        <meshStandardMaterial color="#c0c0c0" metalness={0.9} roughness={0.1} />
      </mesh>
      {/* Frequency display */}
      <Text position={[-0.1, 0.05, 0.26]} fontSize={0.08} color="#00ff00">
        {freq.toFixed(1)} MHz
      </Text>
      <Text position={[0, 0.5, 0]} fontSize={0.1} color="white" anchorX="center">
        P4 Radio
      </Text>
    </group>
  );
}
