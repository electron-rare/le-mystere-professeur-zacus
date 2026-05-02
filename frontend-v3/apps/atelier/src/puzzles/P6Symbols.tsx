import { useState } from 'react';
import { Text } from '@react-three/drei';
import { useSimStore } from '../stores/simStore.js';

interface Props {
  position: [number, number, number];
}

const SYMBOLS = ['α', 'β', 'γ', 'δ', 'σ', 'ω', 'φ', 'ψ', 'χ', 'ρ', 'μ', 'ν'];
const CORRECT_ORDER = [0, 3, 6, 1]; // indices of correct symbols in order
const SLOT_COUNT = 4;

export function P6Symbols({ position }: Props) {
  const { solvePuzzle, mode } = useSimStore();
  const [slots, setSlots] = useState<(number | null)[]>(Array(SLOT_COUNT).fill(null));
  const [selected, setSelected] = useState<number | null>(null);

  const handleTileClick = (idx: number) => {
    if (mode === 'demo') return;
    setSelected(idx);
  };

  const handleSlotClick = (slotIdx: number) => {
    if (mode === 'demo' || selected === null) return;
    const newSlots = [...slots];
    newSlots[slotIdx] = selected;
    setSlots(newSlots);
    setSelected(null);
    // Check if complete and correct
    if (newSlots.every((s) => s !== null)) {
      const correct = newSlots.every((s, i) => s === CORRECT_ORDER[i]);
      if (correct) solvePuzzle('P6_SYMBOLES');
    }
  };

  return (
    <group position={position}>
      {/* Board */}
      <mesh castShadow>
        <boxGeometry args={[0.8, 0.05, 0.6]} />
        <meshStandardMaterial color="#5a3a1a" roughness={0.9} />
      </mesh>

      {/* Symbol tiles */}
      {SYMBOLS.slice(0, 8).map((sym, i) => (
        <mesh
          key={i}
          position={[(i % 4) * 0.18 - 0.27, 0.08, Math.floor(i / 4) * 0.18 - 0.09]}
          onClick={() => handleTileClick(i)}
        >
          <boxGeometry args={[0.14, 0.04, 0.14]} />
          <meshStandardMaterial
            color={selected === i ? '#0071e3' : '#c8a060'}
            emissive={selected === i ? '#0071e3' : '#000000'}
            emissiveIntensity={selected === i ? 0.5 : 0}
          />
        </mesh>
      ))}

      {/* Symbol text labels */}
      {SYMBOLS.slice(0, 8).map((sym, i) => (
        <Text
          key={`sym-${i}`}
          position={[(i % 4) * 0.18 - 0.27, 0.12, Math.floor(i / 4) * 0.18 - 0.09]}
          fontSize={0.08}
          color="#1a1a1a"
          anchorX="center"
        >
          {sym}
        </Text>
      ))}

      {/* Slots */}
      {Array.from({ length: SLOT_COUNT }, (_, i) => (
        <mesh
          key={`slot-${i}`}
          position={[i * 0.18 - 0.27, 0.08, 0.22]}
          onClick={() => handleSlotClick(i)}
        >
          <boxGeometry args={[0.14, 0.04, 0.14]} />
          <meshStandardMaterial
            color={slots[i] !== null ? '#34c759' : '#3a3a3c'}
            emissive={slots[i] !== null ? '#34c759' : '#000000'}
            emissiveIntensity={slots[i] !== null ? 0.3 : 0}
          />
        </mesh>
      ))}

      <Text position={[0, 0.4, 0]} fontSize={0.07} color="white" anchorX="center">
        P6 Symboles
      </Text>
    </group>
  );
}
