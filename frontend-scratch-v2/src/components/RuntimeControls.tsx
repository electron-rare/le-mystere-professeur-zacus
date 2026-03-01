import { useState } from 'react';

const DEFAULT_BASE_URL =
  (import.meta.env.VITE_STORY_API_BASE as string | undefined) ??
  'http://localhost:8080';

type RuntimeControlsProps = {
  yaml: string;
};

type ResponseView = {
  label: string;
  payload: string;
};

async function readResponsePayload(response: Response): Promise<string> {
  const text = await response.text();
  if (!text) {
    return '<empty response>';
  }

  try {
    return JSON.stringify(JSON.parse(text), null, 2);
  } catch {
    return text;
  }
}

export function RuntimeControls({ yaml }: RuntimeControlsProps) {
  const [baseUrl, setBaseUrl] = useState(DEFAULT_BASE_URL);
  const [result, setResult] = useState<ResponseView>({
    label: 'No request yet',
    payload: '',
  });
  const [busy, setBusy] = useState(false);

  const runRequest = async (
    label: string,
    path: string,
    init?: RequestInit,
  ) => {
    setBusy(true);
    try {
      const response = await fetch(`${baseUrl}${path}`, init);
      const payload = await readResponsePayload(response);
      setResult({
        label: `${label} -> HTTP ${response.status}`,
        payload,
      });
    } catch (error) {
      const message = error instanceof Error ? error.message : String(error);
      setResult({
        label: `${label} -> FAILED`,
        payload: message,
      });
    } finally {
      setBusy(false);
    }
  };

  return (
    <div className="runtime-panel">
      <div>
        <div className="toolbar">
          <input
            type="text"
            value={baseUrl}
            onChange={(event) => setBaseUrl(event.target.value.trim())}
            aria-label="story api base url"
            placeholder="http://esp32:8080"
          />
        </div>
        <div className="actions">
          <button
            type="button"
            disabled={busy}
            onClick={() => runRequest('list', '/api/story/list')}
          >
            List
          </button>
          <button
            type="button"
            disabled={busy}
            onClick={() => runRequest('status', '/api/story/status')}
          >
            Status
          </button>
          <button
            type="button"
            disabled={busy || yaml.trim().length === 0}
            onClick={() =>
              runRequest('validate', '/api/story/validate', {
                method: 'POST',
                headers: {
                  'Content-Type': 'application/json',
                },
                body: JSON.stringify({ yaml }),
              })
            }
          >
            Validate
          </button>
          <button
            type="button"
            disabled={busy || yaml.trim().length === 0}
            onClick={() =>
              runRequest('deploy', '/api/story/deploy', {
                method: 'POST',
                headers: {
                  'Content-Type': 'application/x-yaml',
                },
                body: yaml,
              })
            }
          >
            Deploy
          </button>
        </div>
      </div>
      <pre aria-live="polite">
        {result.label}
        {'\n'}
        {result.payload}
      </pre>
    </div>
  );
}
