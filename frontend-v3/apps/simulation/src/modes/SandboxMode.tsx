/**
 * SandboxMode: free orbit camera + click-to-interact.
 * The OrbitControls are added directly in RoomScene when mode === 'sandbox'.
 * This component provides the HUD controls for sandbox.
 */
export function SandboxMode() {
  return (
    <div className="absolute bottom-4 right-4 bg-black/60 text-white text-xs rounded-xl p-3 backdrop-blur-md">
      <div className="font-semibold mb-1 text-[#0071e3]">Mode Sandbox</div>
      <div>Clic gauche: orbite</div>
      <div>Clic droit: panoramique</div>
      <div>Molette: zoom</div>
      <div className="mt-1">Cliquer sur un puzzle pour interagir</div>
    </div>
  );
}
