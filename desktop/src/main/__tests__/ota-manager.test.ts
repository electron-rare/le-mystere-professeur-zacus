import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';

// Mock electron BEFORE importing the module under test. OTAManager only
// touches BrowserWindow.webContents.send / executeJavaScript, so a tiny
// stub is enough.
vi.mock('electron', () => ({
  BrowserWindow: vi.fn(),
  IpcMain: vi.fn(),
}));

// Stub homedir so we never write to the real ~/.zacus-studio cache.
vi.mock('os', async () => {
  const actual = await vi.importActual<typeof import('os')>('os');
  return { ...actual, homedir: () => '/tmp/zacus-studio-test-home' };
});

// fs mocks for the file-import test path.
vi.mock('fs/promises', async () => {
  const actual = await vi.importActual<typeof import('fs/promises')>('fs/promises');
  return {
    ...actual,
    mkdir: vi.fn().mockResolvedValue(undefined),
    readFile: vi.fn(),
    writeFile: vi.fn().mockResolvedValue(undefined),
  };
});

vi.mock('fs', async () => {
  const actual = await vi.importActual<typeof import('fs')>('fs');
  return {
    ...actual,
    existsSync: vi.fn().mockReturnValue(false),
    statSync: vi.fn().mockReturnValue({ size: 1024 }),
    createReadStream: vi.fn(() => {
      // Minimal Readable stub: emits a single chunk + end. The OTA WiFi
      // path pipes this into a mocked http.request, which itself never
      // resolves; the rejection we care about comes from the activeOTAs
      // guard, not from upload completion.
      const { Readable } = require('node:stream');
      return Readable.from([Buffer.from([0xe9, 0x00, 0x00])]);
    }),
  };
});

// Networking is exercised separately; for these unit tests we stub http
// to never make real requests.
vi.mock('http', () => ({
  default: {
    get: vi.fn(),
    request: vi.fn(),
  },
  get: vi.fn(),
  request: vi.fn(),
}));

import { OTAManager } from '../ota-manager.js';
import type { BrowserWindow } from 'electron';
import { readFile } from 'fs/promises';

function makeFakeWindow(): BrowserWindow {
  const send = vi.fn();
  const executeJavaScript = vi.fn().mockResolvedValue(undefined);
  return {
    webContents: { send, executeJavaScript },
    // Cast — OTAManager only uses webContents.
  } as unknown as BrowserWindow;
}

describe('OTAManager — construction', () => {
  it('initialises without throwing and triggers cache dir creation', async () => {
    const win = makeFakeWindow();
    const mgr = new OTAManager(win);
    expect(mgr).toBeInstanceOf(OTAManager);
    // Constructor calls ensureCacheDir asynchronously; give it a tick.
    await Promise.resolve();
    const { mkdir } = await import('fs/promises');
    expect(mkdir).toHaveBeenCalled();
  });
});

describe('OTAManager — checkUpdate', () => {
  let mgr: OTAManager;

  beforeEach(() => {
    mgr = new OTAManager(makeFakeWindow());
  });

  afterEach(() => {
    vi.clearAllMocks();
  });

  it('returns "unknown" when both device + manifest are unreachable', async () => {
    // No manifest exists (existsSync = false from fs mock), and the HTTP
    // request will be a no-op stub that never resolves. Make checkUpdate
    // tolerate that path: catch swallows everything.
    const result = await mgr.checkUpdate('192.168.0.99');
    expect(result.current).toBe('unknown');
    expect(result.available).toBe('unknown');
    expect(result.needsUpdate).toBe(false);
  });
});

describe('OTAManager — startUpdate guards', () => {
  let mgr: OTAManager;

  beforeEach(() => {
    mgr = new OTAManager(makeFakeWindow());
  });

  afterEach(() => {
    vi.clearAllMocks();
  });

  it('rejects an unknown OTA method', async () => {
    await expect(
      // @ts-expect-error — exercising the runtime guard
      mgr.startUpdate('192.168.0.42', 'mqtt', '/tmp/firmware.bin'),
    ).rejects.toThrow(/Unknown OTA method|method/i);
  });

  // Note: the "rejects concurrent OTA on same device" guard exists in
  // OTAManager.startUpdate but is unreachable from external callers
  // because the 60 s rate limit triggers first when invoked twice in
  // quick succession. The activeOTAs check survives as a belt-and-
  // braces guard for hypothetical concurrent calls from different IPC
  // contexts and is not exercised here.

  it('rate-limits two updates on the same device within 60s', async () => {
    // First call records lastOTATime even though the WiFi path will fail.
    await mgr.startUpdate('192.168.0.42', 'wifi', '/tmp/firmware.bin').catch(() => undefined);

    // Immediately retry: rate limit must trigger.
    await expect(
      mgr.startUpdate('192.168.0.42', 'wifi', '/tmp/firmware.bin'),
    ).rejects.toThrow(/rate limit/i);
  });
});

describe('OTAManager — importFirmware', () => {
  let mgr: OTAManager;

  beforeEach(() => {
    mgr = new OTAManager(makeFakeWindow());
  });

  afterEach(() => {
    vi.clearAllMocks();
  });

  it('rejects a binary whose first byte is not 0xE9 (ESP32 magic)', async () => {
    vi.mocked(readFile).mockResolvedValueOnce(Buffer.from([0x00, 0x00, 0x00]));
    await expect(mgr.importFirmware('/tmp/not-firmware.bin')).rejects.toThrow(
      /bad magic byte|Invalid ESP32/i,
    );
  });

  it('accepts a binary starting with 0xE9 and copies it to the cache dir', async () => {
    const valid = Buffer.from([0xe9, 0x01, 0x02, 0x03]);
    vi.mocked(readFile).mockResolvedValue(valid);
    const dest = await mgr.importFirmware('/tmp/zacus.bin');
    expect(dest).toContain('zacus.bin');
    const { writeFile } = await import('fs/promises');
    expect(writeFile).toHaveBeenCalledWith(expect.stringContaining('zacus.bin'), valid);
  });
});
