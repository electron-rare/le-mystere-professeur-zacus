import type { CSSProperties } from 'react';

const HUD_STYLE: CSSProperties = {
  position: 'absolute',
  bottom: 16,
  right: 16,
  background: 'rgba(0, 0, 0, 0.6)',
  color: '#fff',
  borderRadius: 12,
  padding: 12,
  fontSize: 12,
  backdropFilter: 'blur(8px)',
  WebkitBackdropFilter: 'blur(8px)',
};

/**
 * SandboxMode: free orbit camera + click-to-interact.
 * OrbitControls live in RoomScene when mode === 'sandbox'.
 * This component renders the HUD legend.
 */
export function SandboxMode() {
  return (
    <div style={HUD_STYLE}>
      <div style={{ fontWeight: 600, marginBottom: 4, color: '#0071e3' }}>
        Mode Sandbox
      </div>
      <div>Clic gauche : orbite</div>
      <div>Clic droit : panoramique</div>
      <div>Molette : zoom</div>
      <div style={{ marginTop: 4 }}>Cliquer sur un puzzle pour interagir</div>
    </div>
  );
}
