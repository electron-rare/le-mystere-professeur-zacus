import { useCallback, useEffect, useRef, useState } from 'react';
import {
  type LegacyStatus,
  type StoryStatus,
  type StreamMessage,
  connectStoryStream,
  legacyStatus,
  storyStatus,
} from './api';

export interface RuntimeState {
  connected: boolean;
  story: StoryStatus | null;
  legacy: LegacyStatus | null;
  lastError: string;
  lastEvent: StreamMessage | null;
}

const POLL_INTERVAL = 3000;

export function useRuntimeStore() {
  const [state, setState] = useState<RuntimeState>({
    connected: false,
    story: null,
    legacy: null,
    lastError: '',
    lastEvent: null,
  });
  const wsRef = useRef<{ close: () => void } | null>(null);
  const timerRef = useRef<ReturnType<typeof setInterval> | null>(null);

  const poll = useCallback(async () => {
    try {
      const [story, legacy] = await Promise.all([
        storyStatus().catch(() => null),
        legacyStatus().catch(() => null),
      ]);
      setState((prev) => ({
        ...prev,
        connected: !!(story || legacy),
        story: story ?? prev.story,
        legacy: legacy ?? prev.legacy,
        lastError: '',
      }));
    } catch (err) {
      setState((prev) => ({
        ...prev,
        connected: false,
        lastError: err instanceof Error ? err.message : String(err),
      }));
    }
  }, []);

  useEffect(() => {
    poll();
    timerRef.current = setInterval(poll, POLL_INTERVAL);

    try {
      const ws = connectStoryStream(
        (msg) => {
          setState((prev) => ({ ...prev, lastEvent: msg }));
          if (msg.type === 'step_change' || msg.type === 'status') {
            poll();
          }
        },
        () => {
          /* ws error — polling will keep us alive */
        },
      );
      wsRef.current = ws;
    } catch {
      /* no ws support */
    }

    return () => {
      if (timerRef.current) clearInterval(timerRef.current);
      if (wsRef.current) wsRef.current.close();
    };
  }, [poll]);

  return { ...state, refresh: poll };
}

/** Detect if we're on the Media Manager screen */
export function isMediaManagerActive(legacy: LegacyStatus | null): boolean {
  if (!legacy) return false;
  const screen = legacy.story.screen ?? '';
  const step = legacy.story.step ?? '';
  return (
    screen === 'SCENE_MEDIA_MANAGER' ||
    step === 'STEP_MEDIA_MANAGER' ||
    screen.includes('MEDIA_MANAGER') ||
    step.includes('MEDIA_MANAGER')
  );
}
