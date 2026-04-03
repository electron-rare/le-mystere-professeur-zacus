import { IpcMain, BrowserWindow } from 'electron';
import { createReadStream, statSync } from 'fs';
import { homedir } from 'os';
import { join, basename } from 'path';
import { mkdir, writeFile, readFile } from 'fs/promises';
import { existsSync } from 'fs';
import * as http from 'http';
import type { IncomingMessage } from 'http';

export interface DeviceVersion {
  firmware: string;
  version: string;
  idf: string;
}

export interface DeviceStatus {
  battery_pct: number;
  uptime_s: number;
  espnow_peers: number;
  heap_free: number;
}

export interface OTAStatus {
  state: 'idle' | 'downloading' | 'verifying' | 'rebooting';
  progress: number;
}

type OTAMethod = 'wifi' | 'ble' | 'usb';

interface ActiveOTA {
  deviceId: string;
  method: OTAMethod;
  startedAt: number;
  abortController?: AbortController;
}

export class OTAManager {
  private win: BrowserWindow;
  private activeOTAs = new Map<string, ActiveOTA>();
  private firmwareCacheDir: string;
  private lastOTATime = new Map<string, number>();
  private readonly OTA_RATE_LIMIT_MS = 60_000;

  constructor(win: BrowserWindow) {
    this.win = win;
    this.firmwareCacheDir = join(homedir(), '.zacus-studio', 'firmwares');
    this.ensureCacheDir();
  }

  private async ensureCacheDir(): Promise<void> {
    await mkdir(this.firmwareCacheDir, { recursive: true });
  }

  setupHandlers(ipcMain: IpcMain): void {
    ipcMain.handle('ota:check', async (_e, deviceId: string) => {
      return this.checkUpdate(deviceId);
    });

    ipcMain.handle('ota:update', async (_e, {
      deviceId,
      method,
      firmwarePath,
    }: { deviceId: string; method: OTAMethod; firmwarePath: string }) => {
      await this.startUpdate(deviceId, method, firmwarePath);
    });

    ipcMain.handle('ota:rollback', async (_e, deviceId: string) => {
      return this.rollback(deviceId);
    });
  }

  // ─── Version Check ────────────────────────────────────────────────────────

  async checkUpdate(deviceId: string): Promise<{
    current: string;
    available: string;
    needsUpdate: boolean;
  }> {
    try {
      const deviceVersion = await this.getDeviceVersion(deviceId);
      const availableVersion = await this.getAvailableVersion(deviceId);

      return {
        current: deviceVersion?.version ?? 'unknown',
        available: availableVersion ?? 'unknown',
        needsUpdate: Boolean(
          deviceVersion && availableVersion &&
          availableVersion !== deviceVersion.version
        ),
      };
    } catch {
      return { current: 'unknown', available: 'unknown', needsUpdate: false };
    }
  }

  private async getDeviceVersion(deviceId: string): Promise<DeviceVersion | null> {
    // deviceId can be an IP or a serial port path
    if (deviceId.includes('/dev/')) {
      // USB: query via serial command
      return this.queryVersionViaSerial(deviceId);
    }

    // WiFi: HTTP GET /version
    const url = `http://${deviceId}/version`;
    try {
      const response = await this.httpGet(url, 5000);
      return JSON.parse(response) as DeviceVersion;
    } catch {
      return null;
    }
  }

  private async getAvailableVersion(deviceId: string): Promise<string | null> {
    // Look in firmware cache for matching firmware
    const manifestPath = join(this.firmwareCacheDir, 'manifest.json');
    if (!existsSync(manifestPath)) return null;

    try {
      const manifest = JSON.parse(await readFile(manifestPath, 'utf-8')) as Record<string, string>;
      // Match by device name prefix (e.g., "p1_son" for puzzle 1)
      const key = Object.keys(manifest).find(k => deviceId.toLowerCase().includes(k.split('_')[0]));
      return key ? manifest[key] : null;
    } catch {
      return null;
    }
  }

  // ─── OTA Update Orchestration ─────────────────────────────────────────────

  async startUpdate(deviceId: string, method: OTAMethod, firmwarePath: string): Promise<void> {
    // Rate limiting: prevent accidental double-flash
    const lastOTA = this.lastOTATime.get(deviceId) ?? 0;
    if (Date.now() - lastOTA < this.OTA_RATE_LIMIT_MS) {
      throw new Error(`OTA rate limit: wait ${Math.ceil((this.OTA_RATE_LIMIT_MS - (Date.now() - lastOTA)) / 1000)}s`);
    }

    if (this.activeOTAs.has(deviceId)) {
      throw new Error(`OTA already in progress for ${deviceId}`);
    }

    const abortController = new AbortController();
    this.activeOTAs.set(deviceId, {
      deviceId,
      method,
      startedAt: Date.now(),
      abortController,
    });

    try {
      this.lastOTATime.set(deviceId, Date.now());

      switch (method) {
        case 'wifi': await this.otaViaWiFi(deviceId, firmwarePath, abortController.signal); break;
        case 'ble':  await this.otaViaBLE(deviceId, firmwarePath); break;
        case 'usb':  await this.otaViaUSB(deviceId, firmwarePath); break;
        default:     throw new Error(`Unknown OTA method: ${method}`);
      }

      // Confirm update by polling /version until device reboots and comes back
      await this.waitForReboot(deviceId);
      const newVersion = await this.getDeviceVersion(deviceId);

      this.sendEvent('ota:complete', {
        deviceId,
        success: true,
        newVersion: newVersion?.version,
      });
    } catch (err) {
      const error = err instanceof Error ? err.message : String(err);
      this.sendEvent('ota:complete', { deviceId, success: false, error });
      throw err;
    } finally {
      this.activeOTAs.delete(deviceId);
    }
  }

  // ─── WiFi OTA ─────────────────────────────────────────────────────────────

  private async otaViaWiFi(
    deviceId: string,
    firmwarePath: string,
    signal: AbortSignal
  ): Promise<void> {
    const fileSize = statSync(firmwarePath).size;
    const url = `http://${deviceId}/ota`;

    this.sendProgress(deviceId, 0, 'uploading');

    // Stream the firmware binary via chunked HTTP POST
    await new Promise<void>((resolve, reject) => {
      if (signal.aborted) { reject(new Error('Aborted')); return; }

      const fileStream = createReadStream(firmwarePath);
      let uploaded = 0;

      const options = new URL(url);
      const req = http.request(
        {
          hostname: options.hostname,
          port: options.port || 80,
          path: options.pathname,
          method: 'POST',
          headers: {
            'Content-Type': 'application/octet-stream',
            'Content-Length': fileSize,
            'X-Firmware-Name': basename(firmwarePath),
          },
        },
        (res: IncomingMessage) => {
          const chunks: Buffer[] = [];
          res.on('data', (chunk: Buffer) => chunks.push(chunk));
          res.on('end', () => {
            if (res.statusCode === 200) resolve();
            else reject(new Error(`OTA server returned ${res.statusCode}: ${Buffer.concat(chunks).toString()}`));
          });
        }
      );

      req.on('error', reject);

      signal.addEventListener('abort', () => {
        req.destroy();
        reject(new Error('OTA cancelled'));
      });

      fileStream.on('data', (chunk: Buffer) => {
        uploaded += chunk.length;
        const pct = Math.round((uploaded / fileSize) * 100);
        this.sendProgress(deviceId, pct, 'uploading');
      });

      fileStream.on('error', reject);
      fileStream.pipe(req);
    });

    // Poll /ota/status for verification phase
    await this.pollOTAStatus(deviceId);
  }

  private async pollOTAStatus(deviceId: string): Promise<void> {
    const maxAttempts = 60;
    let attempts = 0;

    while (attempts < maxAttempts) {
      await sleep(1000);
      attempts++;

      try {
        const statusJson = await this.httpGet(`http://${deviceId}/ota/status`, 3000);
        const status = JSON.parse(statusJson) as OTAStatus;

        if (status.state === 'verifying') {
          this.sendProgress(deviceId, status.progress, 'verifying');
        } else if (status.state === 'rebooting') {
          this.sendProgress(deviceId, 100, 'rebooting');
          return; // Device will reboot, we're done
        } else if (status.state === 'idle' && status.progress === 100) {
          return; // Done
        }
      } catch {
        // Device may have rebooted; treat connection failure as success
        if (attempts > 5) return;
      }
    }

    throw new Error('OTA status polling timed out');
  }

  // ─── BLE OTA ──────────────────────────────────────────────────────────────

  private async otaViaBLE(deviceId: string, firmwarePath: string): Promise<void> {
    // Delegate to ble-handler IPC
    await this.win.webContents.executeJavaScript(
      `window.zacus.ble.dfu(${JSON.stringify(deviceId)}, ${JSON.stringify(firmwarePath)})`
    );
  }

  // ─── USB OTA ──────────────────────────────────────────────────────────────

  private async otaViaUSB(deviceId: string, firmwarePath: string): Promise<void> {
    // Delegate to serial-handler IPC
    await this.win.webContents.executeJavaScript(
      `window.zacus.serial.flash(${JSON.stringify(deviceId)}, ${JSON.stringify(firmwarePath)})`
    );
  }

  // ─── Rollback ─────────────────────────────────────────────────────────────

  async rollback(deviceId: string): Promise<boolean> {
    try {
      const response = await this.httpPost(`http://${deviceId}/ota/rollback`, '', 10000);
      return response.status === 200;
    } catch {
      return false;
    }
  }

  // ─── Reboot Detection ─────────────────────────────────────────────────────

  private async waitForReboot(deviceId: string, timeoutMs = 60_000): Promise<void> {
    if (deviceId.includes('/dev/')) return; // USB: no reboot detection

    const start = Date.now();
    let deviceGoneOnce = false;

    while (Date.now() - start < timeoutMs) {
      await sleep(2000);
      try {
        await this.httpGet(`http://${deviceId}/version`, 2000);
        if (deviceGoneOnce) {
          // Device came back — reboot complete
          return;
        }
      } catch {
        deviceGoneOnce = true;
        this.sendProgress(deviceId, 100, 'rebooting');
      }
    }

    if (!deviceGoneOnce) {
      // Device never rebooted but responded throughout — might be fine
      return;
    }

    throw new Error('Device did not come back online after reboot');
  }

  // ─── Firmware Cache ───────────────────────────────────────────────────────

  async importFirmware(sourcePath: string): Promise<string> {
    const name = basename(sourcePath);

    // Basic ELF header validation for ESP32 binaries
    const { readFile: rf } = await import('fs/promises');
    const header = await rf(sourcePath);

    // ESP32 app images start with 0xE9 (magic byte)
    if (header[0] !== 0xE9) {
      throw new Error('Invalid ESP32 firmware: bad magic byte');
    }

    const destPath = join(this.firmwareCacheDir, name);
    const content = await rf(sourcePath);
    await writeFile(destPath, content);
    return destPath;
  }

  // ─── Serial Version Query ─────────────────────────────────────────────────

  private async queryVersionViaSerial(_port: string): Promise<DeviceVersion | null> {
    // Send version query command and wait for JSON response
    // This assumes the firmware has a CLI that responds to "version\n"
    // Implemented via serial IPC — simplified here
    return null;
  }

  // ─── HTTP Helpers ─────────────────────────────────────────────────────────

  private httpGet(url: string, timeoutMs: number): Promise<string> {
    return new Promise((resolve, reject) => {
      const timer = setTimeout(() => reject(new Error('Timeout')), timeoutMs);
      http.get(url, (res) => {
        clearTimeout(timer);
        const chunks: Buffer[] = [];
        res.on('data', (c: Buffer) => chunks.push(c));
        res.on('end', () => resolve(Buffer.concat(chunks).toString('utf-8')));
        res.on('error', reject);
      }).on('error', (e) => {
        clearTimeout(timer);
        reject(e);
      });
    });
  }

  private async httpPost(url: string, body: string, _timeoutMs: number): Promise<{ status: number; data: string }> {
    const options = new URL(url);
    return new Promise((resolve, reject) => {
      const req = http.request(
        {
          hostname: options.hostname,
          port: options.port || 80,
          path: options.pathname,
          method: 'POST',
          headers: { 'Content-Type': 'application/json', 'Content-Length': Buffer.byteLength(body) },
        },
        (res) => {
          const chunks: Buffer[] = [];
          res.on('data', (c: Buffer) => chunks.push(c));
          res.on('end', () => resolve({
            status: res.statusCode ?? 0,
            data: Buffer.concat(chunks).toString('utf-8'),
          }));
        }
      );
      req.on('error', reject);
      req.write(body);
      req.end();
    });
  }

  // ─── Event Helpers ────────────────────────────────────────────────────────

  private sendProgress(deviceId: string, percent: number, stage: string): void {
    this.win.webContents.send('ota:progress', { deviceId, percent, stage });
  }

  private sendEvent(channel: string, data: unknown): void {
    this.win.webContents.send(channel, data);
  }
}

function sleep(ms: number): Promise<void> {
  return new Promise(resolve => setTimeout(resolve, ms));
}
