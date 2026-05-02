import { useState } from 'react';
import { Text } from '@react-three/drei';
import { useSimStore } from '../stores/simStore.js';

interface Props {
  position: [number, number, number];
}

const CORRECT_CODE = '12345678'; // assembled from all puzzles
const KEYS = ['1', '2', '3', '4', '5', '6', '7', '8', '9', '*', '0', '#'];

export function P7Coffre({ position }: Props) {
  const { solvePuzzle, mode } = useSimStore();
  const [input, setInput] = useState('');
  const [error, setError] = useState(false);

  const handleKey = (digit: string) => {
    if (mode === 'demo') return;
    if (!/^\d$/.test(digit)) return; // ignore * and #
    const newInput = input + digit;
    setInput(newInput);
    setError(false);
    if (newInput.length === 8) {
      if (newInput === CORRECT_CODE) {
        solvePuzzle('P7_COFFRE');
      } else {
        setError(true);
        setTimeout(() => { setInput(''); setError(false); }, 800);
      }
    }
  };

  return (
    <group position={position}>
      {/* Safe body */}
      <mesh castShadow>
        <boxGeometry args={[1, 1, 0.8]} />
        <meshStandardMaterial color="#2c2c2e" metalness={0.5} />
      </mesh>

      {/* Keypad grid 3x4 */}
      {KEYS.map((k, i) => (
        <mesh
          key={k}
          position={[(i % 3) * 0.22 - 0.22, 0.6 - Math.floor(i / 3) * 0.18, 0.41]}
          onClick={() => handleKey(k)}
        >
          <boxGeometry args={[0.18, 0.14, 0.04]} />
          <meshStandardMaterial color={error ? '#ff3b30' : '#3a3a3a'} />
        </mesh>
      ))}

      {/* Key labels */}
      {KEYS.map((k, i) => (
        <Text
          key={`label-${k}-${i}`}
          position={[(i % 3) * 0.22 - 0.22, 0.6 - Math.floor(i / 3) * 0.18, 0.44]}
          fontSize={0.07}
          color="white"
          anchorX="center"
        >
          {k}
        </Text>
      ))}

      {/* Display */}
      <Text position={[0, 0.95, 0.41]} fontSize={0.07} color={error ? '#ff3b30' : '#00ff00'} anchorX="center">
        {input.padEnd(8, '_')}
      </Text>

      <Text position={[0, 1.2, 0]} fontSize={0.1} color="white" anchorX="center">
        P7 Coffre
      </Text>
    </group>
  );
}
