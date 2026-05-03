import { lazy, Suspense, useCallback } from 'react';
import { useRuntimeStore } from '../stores/runtimeStore.js';
import { useSimStore } from '../stores/simStore.js';
import { useValidationStore } from '../stores/validationStore.js';

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
  const pendingIr = useRuntimeStore((s) => s.pendingIr);
  const commitPendingIr = useRuntimeStore((s) => s.commitPendingIr);
  const mode = useSimStore((s) => s.mode);
  const loadScenario = useSimStore((s) => s.loadScenario);
  const hasErrors = useValidationStore((s) =>
    s.entries.some((e) => e.severity === 'error'),
  );

  const handleRun = useCallback(() => {
    if (!pendingIr || hasErrors) return;
    loadScenario(pendingIr);
    commitPendingIr();
  }, [pendingIr, hasErrors, loadScenario, commitPendingIr]);

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
        <button
          type="button"
          onClick={handleRun}
          disabled={hasErrors}
          title={
            hasErrors
              ? 'Corrige les erreurs de validation pour relancer'
              : 'Recompiler et rejouer la scène avec le scenario édité'
          }
          style={{
            position: 'absolute',
            top: 8,
            right: 8,
            padding: '6px 14px',
            background: hasErrors ? '#6b6b70' : '#fbbf24',
            color: '#1a1a1d',
            fontSize: 12,
            fontWeight: 700,
            border: 'none',
            borderRadius: 4,
            cursor: hasErrors ? 'not-allowed' : 'pointer',
            zIndex: 20,
            boxShadow: hasErrors ? 'none' : '0 2px 6px rgba(0,0,0,0.4)',
          }}
        >
          {hasErrors ? '⚠ stale (errors)' : '▶ Run'}
        </button>
      ) : null}
    </div>
  );
}
