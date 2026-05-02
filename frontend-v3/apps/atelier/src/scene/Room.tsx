/**
 * Low-poly room geometry: floor, ceiling, 4 walls.
 */
export function Room() {
  const walls = [
    { pos: [0, 2, -4] as [number, number, number], rot: [0, 0, 0] as [number, number, number], size: [12, 4] as [number, number] },
    { pos: [0, 2, 4] as [number, number, number], rot: [0, Math.PI, 0] as [number, number, number], size: [12, 4] as [number, number] },
    { pos: [-6, 2, 0] as [number, number, number], rot: [0, Math.PI / 2, 0] as [number, number, number], size: [8, 4] as [number, number] },
    { pos: [6, 2, 0] as [number, number, number], rot: [0, -Math.PI / 2, 0] as [number, number, number], size: [8, 4] as [number, number] },
  ];

  return (
    <group>
      {/* Floor */}
      <mesh rotation={[-Math.PI / 2, 0, 0]} position={[0, 0, 0]} receiveShadow>
        <planeGeometry args={[12, 8]} />
        <meshStandardMaterial color="#1a1a1a" roughness={0.9} metalness={0.1} />
      </mesh>
      {/* Ceiling */}
      <mesh rotation={[Math.PI / 2, 0, 0]} position={[0, 4, 0]}>
        <planeGeometry args={[12, 8]} />
        <meshStandardMaterial color="#111111" />
      </mesh>
      {/* Walls */}
      {walls.map((wall, i) => (
        <mesh key={i} position={wall.pos} rotation={wall.rot} receiveShadow>
          <planeGeometry args={wall.size} />
          <meshStandardMaterial color="#1e1e2e" roughness={1} />
        </mesh>
      ))}
    </group>
  );
}
