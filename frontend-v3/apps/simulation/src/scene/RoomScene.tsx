import { Canvas } from '@react-three/fiber';
import { OrbitControls, Environment, Sparkles } from '@react-three/drei';
import { Suspense } from 'react';
import { Room } from './Room.js';
import { PuzzleStations } from './PuzzleStations.js';
import { RtcPhone } from './RtcPhone.js';
import { NpcBubble } from './NpcBubble.js';
import { useSimStore } from '../store/simStore.js';
import { DemoCameraController } from '../modes/DemoMode.js';

export function RoomScene() {
  const mode = useSimStore((s) => s.mode);

  return (
    <Canvas
      camera={{ position: [0, 3, 6], fov: 60 }}
      shadows
      gl={{ antialias: true }}
    >
      <Suspense fallback={null}>
        {/* Lighting */}
        <ambientLight intensity={0.3} />
        <spotLight
          position={[0, 5, 0]}
          angle={0.6}
          penumbra={0.8}
          intensity={1.2}
          castShadow
          shadow-mapSize={[1024, 1024]}
        />

        {/* Atmospheric particles */}
        <Sparkles count={80} scale={[6, 4, 6]} size={1.5} speed={0.2} opacity={0.3} color="#ffccaa" />

        {/* Room geometry */}
        <Room />

        {/* Central phone / NPC avatar */}
        <RtcPhone position={[0, 0, 0]} />

        {/* 7 puzzle stations */}
        <PuzzleStations />

        {/* NPC speech bubbles */}
        <NpcBubble />

        {/* Camera control */}
        {mode === 'sandbox' && <OrbitControls makeDefault />}
        {mode === 'demo' && <DemoCameraController />}

        <Environment preset="night" />
        <fog attach="fog" args={['#0a0a0f', 8, 20]} />
      </Suspense>
    </Canvas>
  );
}
