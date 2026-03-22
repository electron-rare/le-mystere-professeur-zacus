import { useCallback, useEffect, useState } from 'react';
import {
  type MediaFileList,
  type MediaStatus,
  mediaFiles,
  mediaPlay,
  mediaStop,
  mediaRecordStart,
  mediaRecordStop,
  legacyControl,
} from '../lib/api';
import type { RuntimeState } from '../lib/useRuntimeStore';
import { isMediaManagerActive } from '../lib/useRuntimeStore';

type Props = { runtime: RuntimeState };

type FileCategory = 'music' | 'picture' | 'recorder';

const CATEGORIES: FileCategory[] = ['music', 'picture', 'recorder'];

export function MediaManager({ runtime }: Props) {
  const [listings, setListings] = useState<
    Record<FileCategory, MediaFileList | null>
  >({
    music: null,
    picture: null,
    recorder: null,
  });
  const [busy, setBusy] = useState(false);
  const [feedback, setFeedback] = useState('');
  const [recSeconds, setRecSeconds] = useState(20);
  const [recFilename, setRecFilename] = useState('take_1.wav');

  const media: MediaStatus | undefined = runtime.legacy?.media;
  const active = isMediaManagerActive(runtime.legacy);

  const loadFiles = useCallback(async () => {
    const results = await Promise.allSettled(
      CATEGORIES.map((kind) => mediaFiles(kind)),
    );
    const next = { ...listings };
    CATEGORIES.forEach((kind, i) => {
      const r = results[i];
      next[kind] = r.status === 'fulfilled' ? r.value : null;
    });
    setListings(next);
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  useEffect(() => {
    if (active) loadFiles();
  }, [active, loadFiles]);

  const run = async (label: string, fn: () => Promise<unknown>) => {
    setBusy(true);
    setFeedback('');
    try {
      await fn();
      setFeedback(`${label} OK`);
    } catch (err) {
      setFeedback(
        `${label} failed: ${err instanceof Error ? err.message : err}`,
      );
    } finally {
      setBusy(false);
    }
  };

  const handlePlay = (path: string) =>
    run('Play', async () => {
      if (media?.playing) await mediaStop();
      await mediaPlay(path);
    });

  const handleStop = () => run('Stop', mediaStop);

  const handleRecordStart = () =>
    run('Record', () => mediaRecordStart(recSeconds, recFilename));

  const handleRecordStop = () => run('RecStop', mediaRecordStop);

  return (
    <div className="media-manager">
      <h2>Media Manager</h2>

      {!active && (
        <div className="notice warn">
          Media Manager not active on device. Current step:{' '}
          <span className="mono">
            {runtime.legacy?.story.screen ?? runtime.legacy?.story.step ?? '?'}
          </span>
        </div>
      )}

      {/* Media status */}
      {media && (
        <section className="card">
          <h3>Status</h3>
          <div className="kv">
            <span>Ready</span>
            <span className={media.ready ? 'badge ok' : 'badge err'}>
              {String(media.ready)}
            </span>
          </div>
          <div className="kv">
            <span>Playing</span>
            <span className={media.playing ? 'badge active' : 'badge'}>
              {String(media.playing)}
            </span>
          </div>
          <div className="kv">
            <span>Recording</span>
            <span className={media.recording ? 'badge active' : 'badge'}>
              {String(media.recording)}
            </span>
          </div>
          {media.recording && (
            <div className="kv">
              <span>Elapsed</span>
              <span>
                {media.record_elapsed_seconds}s / {media.record_limit_seconds}s
              </span>
            </div>
          )}
          {media.record_simulated && (
            <div className="notice info">
              Recording is simulated (placeholder WAV).
            </div>
          )}
          {media.last_error && (
            <div className="notice err">
              Last error: {media.last_error}
            </div>
          )}
        </section>
      )}

      {/* File listings */}
      {CATEGORIES.map((kind) => {
        const list = listings[kind];
        return (
          <section className="card" key={kind}>
            <h3>{kind.charAt(0).toUpperCase() + kind.slice(1)}</h3>
            {!list ? (
              <p className="muted">
                Not loaded.{' '}
                <button
                  type="button"
                  onClick={() =>
                    mediaFiles(kind).then((r) =>
                      setListings((prev) => ({ ...prev, [kind]: r })),
                    )
                  }
                >
                  Load
                </button>
              </p>
            ) : list.files.length === 0 ? (
              <p className="muted">No files.</p>
            ) : (
              <ul className="file-list">
                {list.files.map((f) => (
                  <li key={f}>
                    <span className="mono">{f}</span>
                    {kind === 'music' && (
                      <button
                        type="button"
                        disabled={busy}
                        onClick={() => handlePlay(f)}
                      >
                        Play
                      </button>
                    )}
                  </li>
                ))}
              </ul>
            )}
          </section>
        );
      })}

      {/* Playback controls */}
      <section className="card">
        <h3>Playback</h3>
        <div className="actions">
          <button
            type="button"
            disabled={busy || !media?.playing}
            onClick={handleStop}
          >
            Stop
          </button>
          <button type="button" disabled={busy} onClick={loadFiles}>
            Refresh files
          </button>
        </div>
      </section>

      {/* Recording */}
      <section className="card">
        <h3>Recording</h3>
        <div className="rec-form">
          <label>
            Duration (s)
            <input
              type="number"
              min={1}
              max={120}
              value={recSeconds}
              onChange={(e) => setRecSeconds(Number(e.target.value))}
            />
          </label>
          <label>
            Filename
            <input
              type="text"
              value={recFilename}
              onChange={(e) => setRecFilename(e.target.value)}
            />
          </label>
        </div>
        <div className="actions">
          <button
            type="button"
            disabled={busy || !!media?.recording}
            onClick={handleRecordStart}
          >
            Record
          </button>
          <button
            type="button"
            disabled={busy || !media?.recording}
            onClick={handleRecordStop}
          >
            Stop rec
          </button>
        </div>
      </section>

      {/* Boot mode */}
      <section className="card">
        <h3>Boot mode</h3>
        <div className="actions">
          <button
            type="button"
            disabled={busy}
            onClick={() => run('BootStatus', () => legacyControl('BOOT_MODE_STATUS'))}
          >
            Status
          </button>
          <button
            type="button"
            disabled={busy}
            onClick={() =>
              run('SetMedia', () =>
                legacyControl('BOOT_MODE_SET MEDIA_MANAGER'),
              )
            }
          >
            Set Media
          </button>
          <button
            type="button"
            disabled={busy}
            onClick={() =>
              run('SetStory', () => legacyControl('BOOT_MODE_SET STORY'))
            }
          >
            Set Story
          </button>
          <button
            type="button"
            disabled={busy}
            onClick={() => run('Clear', () => legacyControl('BOOT_MODE_CLEAR'))}
          >
            Clear
          </button>
        </div>
      </section>

      {feedback && <p className="feedback">{feedback}</p>}
    </div>
  );
}
