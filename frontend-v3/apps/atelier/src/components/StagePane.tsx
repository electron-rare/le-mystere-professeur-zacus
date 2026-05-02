import { lazy, Suspense } from 'react';
import { useRuntimeStore } from '../stores/runtimeStore.js';
import { useSimStore } from '../stores/simStore.js';

const RoomSceneLazy = lazy(async () => {
  const mod = await import('../scene/RoomScene.js');
  return { default: mod.RoomScene };
});

const SandboxModeLazy = lazy(async () => {
  const mod = await import('../modes/SandboxMode.js');
  return { default: mod.SandboxMode };
});

const DemoModeLazy = lazy(async () => {
  const mod = await import('../modes/DemoMode.js');
  return { default: mod.DemoMode };
});

const TestModeLazy = lazy(async () => {
  const mod = await import('../modes/TestMode.js');
  return { default: mod.TestMode };
});

export function StagePane() {
  const isStale = useRuntimeStore((s) => s.isStale);
  const mode = useSimStore((s) => s.mode);

  const ModeOverlay =
    mode === 'demo' ? DemoModeLazy : mode === 'test' ? TestModeLazy : SandboxModeLazy;

  return (
    <div style={{ position: 'relative', height: '100%', background: '#000' }}>
      <Suspense
        fallback={
          <div className="atelier-pane atelier-pane--stage">
            <span>Loading 3D engine…</span>
          </div>
        }
      >
        <RoomSceneLazy />
        <ModeOverlay />
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
            zIndex: 20,
          }}
        >
          stale — click Run
        </div>
      ) : null}
    </div>
  );
}
