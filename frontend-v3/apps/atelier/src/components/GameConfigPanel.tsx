/**
 * GameConfigPanel — operator-facing UI to pick the active group profile
 * (TECH / NON_TECH / MIXED / BOTH) and POST it to the ESP32 master.
 *
 * Also surfaces a read-only adaptive engine status pulled from the hints
 * engine `/healthz` endpoint (enabled flag + advertised profile names).
 *
 * Pure inline-styles per atelier convention (no Tailwind, no atelier-pane*
 * class — the panel is self-contained and floats over the editor pane).
 */
import { useEffect, useState } from 'react';
import { GAME_PROFILE_LABELS, GAME_PROFILES, type GameProfile } from '@zacus/shared';
import { useGameConfig } from '../hooks/useGameConfig.js';

interface ToastState {
  kind: 'success' | 'error';
  message: string;
}

const PROFILE_ORDER: readonly GameProfile[] = GAME_PROFILES;

export function GameConfigPanel() {
  const {
    currentProfile,
    availableProfiles,
    adaptiveEnabled,
    adaptiveProfileConfigs,
    loading,
    esp32Error,
    hintsError,
    setProfile,
    refetch,
    esp32BaseUrl,
    hintsBaseUrl,
  } = useGameConfig();

  const [pending, setPending] = useState<GameProfile>('MIXED');
  const [toast, setToast] = useState<ToastState | null>(null);
  const [busy, setBusy] = useState<boolean>(false);

  // Sync the radio selection with whatever the firmware actually reports
  // once it answers the first GET — but never override an in-flight choice.
  useEffect(() => {
    if (currentProfile && !busy) setPending(currentProfile);
  }, [currentProfile, busy]);

  // Auto-clear toast after 4 s.
  useEffect(() => {
    if (!toast) return;
    const id = setTimeout(() => setToast(null), 4_000);
    return () => clearTimeout(id);
  }, [toast]);

  const apply = async () => {
    setBusy(true);
    setToast(null);
    const ok = await setProfile(pending);
    setBusy(false);
    setToast(
      ok
        ? { kind: 'success', message: `Profil « ${GAME_PROFILE_LABELS[pending]} » appliqué` }
        : { kind: 'error', message: 'Échec de l’envoi vers le firmware ESP32' },
    );
  };

  const esp32Unreachable =
    esp32Error?.kind === 'network' || (esp32Error?.kind === 'http' && esp32Error.status >= 500);

  return (
    <div
      data-testid="game-config-panel"
      style={{
        display: 'flex',
        flexDirection: 'column',
        gap: 10,
        padding: 12,
        background: '#1f1f22',
        border: '1px solid #2c2c2e',
        borderRadius: 6,
        color: '#e8e8e8',
        fontSize: 13,
        minWidth: 280,
        maxWidth: 360,
      }}
    >
      <div
        style={{
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'space-between',
          gap: 8,
        }}
      >
        <strong style={{ fontSize: 13, letterSpacing: 0.2 }}>
          Configuration de groupe
        </strong>
        <span
          title={
            adaptiveEnabled
              ? 'Adaptive engine actif'
              : hintsError
                ? 'Hints engine injoignable'
                : 'Adaptive engine désactivé'
          }
          style={{
            fontSize: 11,
            color: adaptiveEnabled ? '#34c759' : '#ff453a',
          }}
          data-testid="adaptive-indicator"
        >
          {adaptiveEnabled ? '\u{1F7E2} actif' : '\u{1F534} désactivé'}
        </span>
      </div>

      <div style={{ display: 'flex', flexDirection: 'column', gap: 4 }}>
        {PROFILE_ORDER.map((p) => {
          const cfg = adaptiveProfileConfigs?.[p];
          return (
            <label
              key={p}
              style={{
                display: 'flex',
                alignItems: 'center',
                gap: 8,
                padding: '4px 6px',
                borderRadius: 4,
                background: pending === p ? '#3c3c4a' : 'transparent',
                cursor: busy ? 'wait' : 'pointer',
              }}
            >
              <input
                type="radio"
                name="game-profile"
                value={p}
                checked={pending === p}
                disabled={busy}
                onChange={() => setPending(p)}
              />
              <span style={{ flex: 1 }}>
                {GAME_PROFILE_LABELS[p]}
                {currentProfile === p ? (
                  <span style={{ marginLeft: 6, fontSize: 10, color: '#9a9a9d' }}>
                    (actif)
                  </span>
                ) : null}
              </span>
              {cfg ? (
                <span
                  style={{ fontSize: 10, color: '#9a9a9d', fontFamily: 'ui-monospace, Menlo, monospace' }}
                  title={`base_modifier=${cfg.base_modifier}, stuck/bump=${cfg.stuck_minutes_per_bump}min, max_auto_bump=${cfg.max_auto_bump}`}
                >
                  {cfg.stuck_minutes_per_bump}min / max{cfg.max_auto_bump}
                </span>
              ) : null}
            </label>
          );
        })}
      </div>

      <div style={{ display: 'flex', alignItems: 'center', gap: 8 }}>
        <button
          type="button"
          onClick={apply}
          disabled={busy || loading || pending === currentProfile}
          style={{
            padding: '6px 14px',
            background: pending === currentProfile ? '#3c3c4a' : '#0071e3',
            color: '#fff',
            fontSize: 12,
            fontWeight: 600,
            border: 'none',
            borderRadius: 4,
            cursor:
              busy || pending === currentProfile ? 'not-allowed' : 'pointer',
          }}
        >
          {busy ? 'Envoi…' : 'Appliquer'}
        </button>
        <button
          type="button"
          onClick={() => void refetch()}
          disabled={loading}
          style={{
            padding: '6px 10px',
            background: '#232327',
            color: '#9a9a9d',
            fontSize: 11,
            border: '1px solid #2c2c2e',
            borderRadius: 4,
            cursor: loading ? 'wait' : 'pointer',
          }}
        >
          Rafraîchir
        </button>
      </div>

      {esp32Unreachable ? (
        <div
          role="alert"
          style={{
            fontSize: 11,
            color: '#fca5a5',
            background: '#2a1414',
            padding: '4px 6px',
            borderRadius: 4,
          }}
        >
          {esp32BaseUrl} non joignable, vérifier Wi-Fi.
        </div>
      ) : esp32Error ? (
        <div style={{ fontSize: 11, color: '#fca5a5' }}>{esp32Error.message}</div>
      ) : null}

      {hintsError ? (
        <div style={{ fontSize: 11, color: '#fcd34d' }}>
          Hints engine ({hintsBaseUrl}) : {hintsError.message}
        </div>
      ) : availableProfiles.length > 0 ? (
        <div style={{ fontSize: 10, color: '#9a9a9d' }}>
          Profils adaptive disponibles : {availableProfiles.join(', ')}
        </div>
      ) : null}

      {toast ? (
        <div
          role="status"
          style={{
            fontSize: 11,
            padding: '4px 6px',
            borderRadius: 4,
            background: toast.kind === 'success' ? '#0e3a1a' : '#2a1414',
            color: toast.kind === 'success' ? '#86efac' : '#fca5a5',
          }}
        >
          {toast.message}
        </div>
      ) : null}
    </div>
  );
}
