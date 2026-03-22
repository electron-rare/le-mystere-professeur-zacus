/**
 * API client aligned with STORY_RUNTIME_API_JSON_CONTRACT.md
 * Supports Story V2 + Legacy Freenove endpoints.
 */

const DEFAULT_BASE =
  (import.meta.env.VITE_STORY_API_BASE as string | undefined) ??
  'http://localhost:8080';

let baseUrl = DEFAULT_BASE;

export function setApiBase(url: string) {
  baseUrl = url.replace(/\/+$/, '');
}
export function getApiBase() {
  return baseUrl;
}

// ─── Helpers ───

async function jsonFetch<T>(path: string, init?: RequestInit, timeoutMs = 5000): Promise<T> {
  const controller = new AbortController();
  const timeoutId = setTimeout(() => controller.abort(), timeoutMs);
  let res: Response;
  try {
    res = await fetch(`${baseUrl}${path}`, {
      ...init,
      signal: controller.signal,
    });
  } catch (err) {
    clearTimeout(timeoutId);
    if (err instanceof DOMException && err.name === 'AbortError') {
      throw new ApiError(0, `Request timed out after ${timeoutMs}ms`, path);
    }
    throw err;
  }
  clearTimeout(timeoutId);
  const text = await res.text();
  if (!text) throw new ApiError(res.status, 'empty response', path);
  const data = JSON.parse(text);
  if (!res.ok) {
    const msg = data?.error?.message ?? data?.error ?? res.statusText;
    throw new ApiError(res.status, msg, path);
  }
  if (data?.ok === false) {
    throw new ApiError(res.status, data.error ?? 'operation failed', path);
  }
  return data as T;
}

export class ApiError extends Error {
  status: number;
  path: string;

  constructor(
    status: number,
    message: string,
    path: string,
  ) {
    super(message);
    this.name = 'ApiError';
    this.status = status;
    this.path = path;
  }
}

// ─── Story V2 ───

export interface ScenarioListItem {
  id: string;
  version: number;
  estimated_duration_s: number;
}

export interface StoryStatus {
  status: 'idle' | 'running' | 'paused';
  scenario_id: string;
  current_step: string;
  progress_pct: number;
  started_at_ms: number;
  selected: string;
  queue_depth: number;
}

export async function storyList(): Promise<ScenarioListItem[]> {
  const data = await jsonFetch<{ scenarios: ScenarioListItem[] }>(
    '/api/story/list',
  );
  return data.scenarios;
}

export async function storyStatus(): Promise<StoryStatus> {
  return jsonFetch<StoryStatus>('/api/story/status');
}

export async function storySelect(scenarioId: string) {
  return jsonFetch<{ selected: string; status: string }>(
    `/api/story/select/${encodeURIComponent(scenarioId)}`,
    { method: 'POST' },
  );
}

export async function storyStart() {
  return jsonFetch<{ status: string; current_step: string }>(
    '/api/story/start',
    { method: 'POST' },
  );
}

export async function storyPause() {
  return jsonFetch<{ status: string }>('/api/story/pause', { method: 'POST' });
}

export async function storyResume() {
  return jsonFetch<{ status: string }>('/api/story/resume', { method: 'POST' });
}

export async function storySkip() {
  return jsonFetch<{ previous_step: string; current_step: string }>(
    '/api/story/skip',
    { method: 'POST' },
  );
}

export async function storyValidate(yaml: string) {
  return jsonFetch<{ valid: boolean }>('/api/story/validate', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ yaml }),
  });
}

export async function storyDeploy(yaml: string) {
  return jsonFetch<{ deployed: string; status: string }>('/api/story/deploy', {
    method: 'POST',
    headers: { 'Content-Type': 'application/x-yaml' },
    body: yaml,
  });
}

// ─── Legacy / Status ───

export interface LegacyStatus {
  story: {
    scenario: string;
    step: string;
    screen?: string;
    audio_pack?: string;
    runtime_contract?: string;
  };
  runtime3?: Runtime3FirmwareStatus;
  network?: {
    state?: string;
    ip?: string;
  };
  media?: MediaStatus;
}

export interface Runtime3FirmwareStatus {
  discovered: boolean;
  loaded: boolean;
  path: string;
  schema_version: string;
  scenario_id: string;
  scenario_version: number;
  entry_step_id: string;
  source_kind: string;
  generated_by: string;
  migration_mode: string;
  step_count: number;
  transition_count: number;
  size_bytes: number;
  error: string;
}

export interface MediaStatus {
  ready: boolean;
  playing: boolean;
  recording: boolean;
  record_limit_seconds: number;
  record_elapsed_seconds: number;
  record_file: string;
  record_simulated: boolean;
  music_dir: string;
  picture_dir: string;
  record_dir: string;
  last_ok: boolean;
  last_error: string;
}

export async function legacyStatus(): Promise<LegacyStatus> {
  return jsonFetch<LegacyStatus>('/api/status');
}

export async function runtime3Status(): Promise<Runtime3FirmwareStatus> {
  return jsonFetch<Runtime3FirmwareStatus>('/api/runtime3/status');
}

export async function runtime3Document(): Promise<Record<string, unknown>> {
  return jsonFetch<Record<string, unknown>>('/api/runtime3/document');
}

// ─── Media ───

export interface MediaFileList {
  ok: boolean;
  kind: string;
  files: string[];
}

export async function mediaFiles(
  kind: 'music' | 'picture' | 'recorder',
): Promise<MediaFileList> {
  return jsonFetch<MediaFileList>(
    `/api/media/files?kind=${encodeURIComponent(kind)}`,
  );
}

export async function mediaPlay(path: string) {
  return jsonFetch<{ action: string; ok: boolean }>('/api/media/play', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ path }),
  });
}

export async function mediaStop() {
  return jsonFetch<{ action: string; ok: boolean }>('/api/media/stop', {
    method: 'POST',
  });
}

export async function mediaRecordStart(seconds: number, filename: string) {
  return jsonFetch<{ action: string; ok: boolean }>(
    '/api/media/record/start',
    {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ seconds, filename }),
    },
  );
}

export async function mediaRecordStop() {
  return jsonFetch<{ action: string; ok: boolean }>('/api/media/record/stop', {
    method: 'POST',
  });
}

// ─── Network ───

export async function networkReconnect() {
  return jsonFetch<{ action: string; ok: boolean }>(
    '/api/network/wifi/reconnect',
    { method: 'POST' },
  );
}

export async function espnowOn() {
  return jsonFetch<{ action: string; ok: boolean }>('/api/network/espnow/on', {
    method: 'POST',
  });
}

export async function espnowOff() {
  return jsonFetch<{ action: string; ok: boolean }>('/api/network/espnow/off', {
    method: 'POST',
  });
}

// ─── Control (legacy fallback) ───

export async function legacyControl(action: string) {
  return jsonFetch<{ ok: boolean; action: string }>('/api/control', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ action }),
  });
}

// ─── WebSocket Stream ───

export type StreamMessage = {
  type: 'status' | 'step_change' | 'transition' | 'audit_log';
  timestamp: number;
  data: Record<string, unknown>;
};

export function connectStoryStream(
  onMessage: (msg: StreamMessage) => void,
  onError?: (err: Event) => void,
): { close: () => void } {
  const MAX_BACKOFF_MS = 8000;
  let attempt = 0;
  let ws: WebSocket | null = null;
  let closed = false;
  let reconnectTimer: ReturnType<typeof setTimeout> | null = null;

  function connect() {
    if (closed) return;
    const wsUrl = baseUrl.replace(/^http/, 'ws') + '/api/story/stream';
    ws = new WebSocket(wsUrl);

    ws.onopen = () => {
      attempt = 0;
    };

    ws.onmessage = (ev) => {
      try {
        onMessage(JSON.parse(ev.data));
      } catch {
        /* ignore parse errors */
      }
    };

    ws.onerror = (err) => {
      if (onError) onError(err);
    };

    ws.onclose = () => {
      if (closed) return;
      const delay = Math.min(1000 * Math.pow(2, attempt), MAX_BACKOFF_MS);
      attempt++;
      reconnectTimer = setTimeout(connect, delay);
    };
  }

  connect();

  return {
    close() {
      closed = true;
      if (reconnectTimer !== null) clearTimeout(reconnectTimer);
      ws?.close();
    },
  };
}

// ─── SSE Stream (legacy) ───

export function connectLegacyStream(
  onStatus: (data: LegacyStatus) => void,
): EventSource {
  const es = new EventSource(`${baseUrl}/api/stream`);
  es.addEventListener('status', (ev) => {
    try {
      onStatus(JSON.parse((ev as MessageEvent).data));
    } catch {
      /* ignore */
    }
  });
  return es;
}

// ---------------------------------------------------------------------------
// Voice pipeline
// ---------------------------------------------------------------------------

export interface VoiceStatus {
  connected: boolean;
  state: number;
  has_audio: boolean;
  last_response?: string;
}

export async function voiceStatus(): Promise<VoiceStatus> {
  return jsonFetch<VoiceStatus>(`${baseUrl}/api/voice/status`);
}

export async function voiceQuery(text: string): Promise<{ status: string }> {
  return jsonFetch<{ status: string }>(`${baseUrl}/api/voice/query`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ text }),
  });
}

// ─── Analytics ───

export interface GameAnalytics {
  session_id: string;
  duration_ms: number;
  puzzles_solved: number;
  total_hints: number;
  total_attempts: number;
  puzzles: Array<{
    puzzle_id: string;
    solved: boolean;
    attempts: number;
    hints: number;
    duration_ms: number;
  }>;
}

export async function gameAnalytics(): Promise<GameAnalytics> {
  return jsonFetch<GameAnalytics>(`${baseUrl}/api/analytics`);
}

// ─── Hints (via mascarade) ───

export async function askHint(puzzleId: string, question: string, hintLevel: number): Promise<{ hint: string; hint_count: number }> {
  return jsonFetch<{ hint: string; hint_count: number }>(`${baseUrl}/api/voice/query`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ text: `[HINT:${puzzleId}:${hintLevel}] ${question}` }),
  });
}
