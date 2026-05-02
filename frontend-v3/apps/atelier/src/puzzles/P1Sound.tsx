import { useState } from 'react';
import { Text } from '@react-three/drei';
import { useSimStore } from '../stores/simStore.js';

interface Props {
  position: [number, number, number];
}

const COLORS = ['#ff3b30', '#34c759', '#0071e3', '#ffcc00'];
const TARGET = [0, 2, 1, 3]; // melody pattern to reproduce

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

export function P1Sound({ position }: Props) {
  const { solvePuzzle, mode } = useSimStore();
  const [pressed, setPressed] = useState<number[]>([]);

  const handleButtonPress = (i: number) => {
    if (mode === 'demo') return;
    const newPressed = [...pressed, i];
    setPressed(newPressed);
    playTone(220 + i * 110);
    if (newPressed.length === TARGET.length) {
      if (newPressed.every((v, j) => v === TARGET[j])) {
        solvePuzzle('P1_SON');
      }
      setPressed([]);
    }
  };

  return (
    <group position={position}>
      <mesh castShadow>
        <boxGeometry args={[0.8, 0.3, 0.8]} />
        <meshStandardMaterial color="#2c2c2e" />
      </mesh>
      {COLORS.map((color, i) => (
        <mesh
          key={i}
          position={[(i % 2) * 0.3 - 0.15, 0.2, Math.floor(i / 2) * 0.3 - 0.15]}
          onClick={() => handleButtonPress(i)}
        >
          <cylinderGeometry args={[0.08, 0.08, 0.06, 16]} />
          <meshStandardMaterial color={color} emissive={color} emissiveIntensity={0.3} />
        </mesh>
      ))}
      <Text position={[0, 0.5, 0]} fontSize={0.1} color="white" anchorX="center">
        P1 Son
      </Text>
    </group>
  );
}
