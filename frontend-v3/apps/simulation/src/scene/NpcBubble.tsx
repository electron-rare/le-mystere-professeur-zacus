import { Html } from '@react-three/drei';
import { useSimStore } from '../store/simStore.js';

export function NpcBubble() {
  const npcLastPhrase = useSimStore((s) => s.npcLastPhrase);
  if (!npcLastPhrase) return null;

  return (
    <Html position={[0, 1.2, 0]} center>
      <div
        style={{
          background: 'rgba(0,0,0,0.85)',
          color: 'white',
          padding: '8px 14px',
          borderRadius: 12,
          fontSize: 13,
          maxWidth: 220,
          textAlign: 'center',
          border: '1px solid rgba(255,255,255,0.15)',
          backdropFilter: 'blur(8px)',
          pointerEvents: 'none',
        }}
      >
        {npcLastPhrase}
      </div>
    </Html>
  );
}
