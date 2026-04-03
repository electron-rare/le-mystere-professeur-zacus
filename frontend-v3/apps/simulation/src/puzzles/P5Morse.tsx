import { useState, useRef } from 'react';
import { Text } from '@react-three/drei';
import { useSimStore } from '../store/simStore.js';

interface Props {
  position: [number, number, number];
}

// Morse code lookup table
const MORSE: Record<string, string> = {
  A:'.-', B:'-...', C:'-.-.', D:'-..', E:'.', F:'..-.',
  G:'--.', H:'....', I:'..', J:'.---', K:'-.-', L:'.-..',
  M:'--', N:'-.', O:'---', P:'.--.', Q:'--.-', R:'.-.',
  S:'...', T:'-', U:'..-', V:'...-', W:'.--', X:'-..-',
  Y:'-.--', Z:'--..', '0':'-----', '1':'.----', '2':'..---',
  '3':'...--', '4':'....-', '5':'.....', '6':'-....', '7':'--...',
  '8':'---..',  '9':'----.',
};

const TARGET_MESSAGE = 'SOS';
const targetMorse = TARGET_MESSAGE.split('').map((c) => MORSE[c] ?? '').join(' ');

function playTone(hz: number, duration = 0.1) {
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

export function P5Morse({ position }: Props) {
  const { solvePuzzle, mode } = useSimStore();
  const [active, setActive] = useState(false);
  const [inputMorse, setInputMorse] = useState('');
  const pressStartRef = useRef<number>(0);
  const dotDashThreshold = 200; // ms — below = dot, above = dash

  const handlePress = () => {
    if (mode === 'demo') return;
    setActive(true);
    pressStartRef.current = Date.now();
    playTone(800, 0.05);
  };

  const handleRelease = () => {
    if (mode === 'demo') return;
    setActive(false);
    const duration = Date.now() - pressStartRef.current;
    const symbol = duration < dotDashThreshold ? '.' : '-';
    setInputMorse((prev) => {
      const next = prev + symbol;
      if (next.replace(/ /g, '') === targetMorse.replace(/ /g, '')) {
        solvePuzzle('P5_MORSE');
      }
      return next;
    });
  };

  return (
    <group position={position}>
      {/* Base */}
      <mesh castShadow>
        <boxGeometry args={[0.6, 0.1, 0.4]} />
        <meshStandardMaterial color="#3a3a3a" metalness={0.7} />
      </mesh>
      {/* Telegraph key arm */}
      <mesh
        position={[0, active ? 0.05 : 0.1, 0]}
        onPointerDown={handlePress}
        onPointerUp={handleRelease}
      >
        <boxGeometry args={[0.4, 0.04, 0.08]} />
        <meshStandardMaterial color="#c0c0c0" metalness={0.9} emissive={active ? '#ffffff' : '#000000'} emissiveIntensity={active ? 0.3 : 0} />
      </mesh>
      <Text position={[0, 0.3, 0]} fontSize={0.07} color="white" anchorX="center">
        P5 Morse
      </Text>
      <Text position={[0, 0.42, 0]} fontSize={0.05} color="#00ff00" anchorX="center">
        {inputMorse.slice(-20)}
      </Text>
    </group>
  );
}
