import { lazy, Suspense } from 'react';
import { useRuntimeStore } from '../stores/runtimeStore.js';

const ThreeStageLazy = lazy(async () => {
  await import('@react-three/fiber');
  return {
    default: function ThreeStagePlaceholder() {
      return (
        <div className="atelier-pane atelier-pane--stage">
          <span>3D simulation stage (P4)</span>
        </div>
      );
    },
  };
});

export function StagePane() {
  const isStale = useRuntimeStore((s) => s.isStale);
  return (
    <div style={{ position: 'relative', height: '100%' }}>
      <Suspense
        fallback={
          <div className="atelier-pane atelier-pane--stage">
            <span>Loading 3D engine…</span>
          </div>
        }
      >
        <ThreeStageLazy />
      </Suspense>
      {isStale ? (
        <div
          style={{
            position: 'absolute',
            top: 8,
            right: 8,
            padding: '4px 10px',
            background: '#fbbf24',
            color: '#1a1a1d',
            fontSize: 12,
            fontWeight: 600,
            borderRadius: 4,
          }}
        >
          stale — click Run
        </div>
      ) : null}
    </div>
  );
}
