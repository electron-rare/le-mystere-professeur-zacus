import { useRef } from 'react';
import { useFrame } from '@react-three/fiber';
import * as THREE from 'three';
import { useSimStore } from '../stores/simStore.js';
import { NPC_MOOD_COLORS } from '@zacus/shared';

interface Props {
  position: [number, number, number];
}

export function RtcPhone({ position }: Props) {
  const npcMood = useSimStore((s) => s.npcMood);
  const glowRef = useRef<THREE.PointLight>(null);

  const moodColor = NPC_MOOD_COLORS[npcMood] ?? '#0071e3';

  useFrame(({ clock }) => {
    if (!glowRef.current) return;
    // Pulsing glow effect
    glowRef.current.intensity = 0.5 + 0.3 * Math.sin(clock.elapsedTime * 2);
  });

  return (
    <group position={position}>
      {/* Table */}
      <mesh castShadow position={[0, -0.02, 0]}>
        <cylinderGeometry args={[0.8, 0.8, 0.04, 32]} />
        <meshStandardMaterial color="#3a2a1a" roughness={0.8} />
      </mesh>
      {/* Phone body */}
      <mesh castShadow position={[0, 0.15, 0]}>
        <boxGeometry args={[0.3, 0.25, 0.15]} />
        <meshStandardMaterial color="#1a1a1a" metalness={0.4} />
      </mesh>
      {/* Handset */}
      <mesh castShadow position={[0, 0.32, 0]} rotation={[0, 0, -0.3]}>
        <boxGeometry args={[0.2, 0.06, 0.06]} />
        <meshStandardMaterial color="#111111" />
      </mesh>
      {/* Mood glow */}
      <pointLight ref={glowRef} color={moodColor} distance={2} />
      <mesh position={[0, 0.1, 0]}>
        <sphereGeometry args={[0.08, 16, 16]} />
        <meshStandardMaterial color={moodColor} emissive={moodColor} emissiveIntensity={1} transparent opacity={0.7} />
      </mesh>
    </group>
  );
}
