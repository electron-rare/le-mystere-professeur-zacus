import { Text } from '@react-three/drei';
import type { ReactNode } from 'react';

interface Props {
  position: [number, number, number];
  label: string;
  onClick?: () => void;
  children?: ReactNode;
}

/**
 * Generic puzzle station wrapper: positions content + renders a floating label.
 */
export function PuzzleStation({ position, label, onClick, children }: Props) {
  return (
    <group position={position} onClick={onClick}>
      {children}
      <Text position={[0, 0.6, 0]} fontSize={0.1} color="white" anchorX="center">
        {label}
      </Text>
    </group>
  );
}
