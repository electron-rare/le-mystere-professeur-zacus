import { contextBridge, ipcRenderer } from 'electron';

// Type definitions
export interface SerialPort {
  path: string;
  name: string;
  vendorId?: string;
  productId?: string;
}

export interface ZacusDevice {
  id: string;
  name: string;
  type: 'puzzle' | 'hub';
  firmwareVersion: string;
  batteryPct: number;
  connectionType: 'usb' | 'ble' | 'wifi';
  lastSeen: number;
  ip?: string;
  mac?: string;
}

export interface OTAProgressEvent {
  deviceId: string;
  percent: number;
  stage: 'uploading' | 'verifying' | 'rebooting';
}

// Expose safe API to renderer via contextBridge
contextBridge.exposeInMainWorld('zacus', {
  // === Serial ===
  serial: {
    list: () => ipcRenderer.invoke('serial:list') as Promise<SerialPort[]>,
    connect: (port: string, baud: number) =>
      ipcRenderer.invoke('serial:connect', { port, baud }) as Promise<boolean>,
    disconnect: (port: string) =>
      ipcRenderer.invoke('serial:disconnect', { port }) as Promise<void>,
    write: (port: string, data: string) =>
      ipcRenderer.invoke('serial:write', { port, data }) as Promise<void>,
    flash: (port: string, firmwarePath: string) =>
      ipcRenderer.invoke('serial:flash', { port, firmwarePath }) as Promise<void>,
    onData: (callback: (port: string, data: string) => void) => {
      ipcRenderer.on('serial:data', (_e, { port, data }) => callback(port, data));
    },
    onPlugged: (callback: (port: string) => void) => {
      ipcRenderer.on('serial:plugged', (_e, port) => callback(port));
    },
    onUnplugged: (callback: (port: string) => void) => {
      ipcRenderer.on('serial:unplugged', (_e, port) => callback(port));
    },
  },

  // === BLE ===
  ble: {
    scan: () => ipcRenderer.invoke('ble:scan') as Promise<void>,
    stopScan: () => ipcRenderer.invoke('ble:stop-scan') as Promise<void>,
    connect: (deviceId: string) =>
      ipcRenderer.invoke('ble:connect', deviceId) as Promise<boolean>,
    disconnect: (deviceId: string) =>
      ipcRenderer.invoke('ble:disconnect', deviceId) as Promise<void>,
    write: (deviceId: string, characteristic: string, data: string) =>
      ipcRenderer.invoke('ble:write', { deviceId, characteristic, data }) as Promise<void>,
    dfu: (deviceId: string, firmwarePath: string) =>
      ipcRenderer.invoke('ble:dfu', { deviceId, firmwarePath }) as Promise<void>,
    onDiscovered: (callback: (device: ZacusDevice) => void) => {
      ipcRenderer.on('ble:discovered', (_e, device) => callback(device));
    },
    onData: (callback: (deviceId: string, characteristic: string, data: string) => void) => {
      ipcRenderer.on('ble:data', (_e, { deviceId, characteristic, data }) =>
        callback(deviceId, characteristic, data));
    },
  },

  // === WiFi ===
  wifi: {
    discover: () =>
      ipcRenderer.invoke('wifi:discover') as Promise<ZacusDevice[]>,
    wsConnect: (url: string) =>
      ipcRenderer.invoke('wifi:ws-connect', url) as Promise<boolean>,
    wsSend: (data: string) =>
      ipcRenderer.invoke('wifi:ws-send', data) as Promise<void>,
    wsDisconnect: () =>
      ipcRenderer.invoke('wifi:ws-disconnect') as Promise<void>,
    http: (url: string, method: string, body?: string, headers?: Record<string, string>) =>
      ipcRenderer.invoke('wifi:http', { url, method, body, headers }) as Promise<{
        status: number;
        data: string;
      }>,
    onWsMessage: (callback: (data: string) => void) => {
      ipcRenderer.on('wifi:ws-message', (_e, data) => callback(data));
    },
    onServiceFound: (callback: (service: ZacusDevice) => void) => {
      ipcRenderer.on('wifi:service-found', (_e, service) => callback(service));
    },
  },

  // === OTA ===
  ota: {
    check: (deviceId: string) =>
      ipcRenderer.invoke('ota:check', deviceId) as Promise<{
        current: string;
        available: string;
        needsUpdate: boolean;
      }>,
    update: (deviceId: string, method: 'wifi' | 'ble' | 'usb', firmwarePath: string) =>
      ipcRenderer.invoke('ota:update', { deviceId, method, firmwarePath }) as Promise<void>,
    rollback: (deviceId: string) =>
      ipcRenderer.invoke('ota:rollback', deviceId) as Promise<boolean>,
    onProgress: (callback: (event: OTAProgressEvent) => void) => {
      ipcRenderer.on('ota:progress', (_e, event) => callback(event));
    },
    onComplete: (callback: (deviceId: string, success: boolean, error?: string) => void) => {
      ipcRenderer.on('ota:complete', (_e, { deviceId, success, error }) =>
        callback(deviceId, success, error));
    },
  },

  // === Files ===
  file: {
    open: (filters?: Array<{ name: string; extensions: string[] }>) =>
      ipcRenderer.invoke('file:open', { filters }) as Promise<string | null>,
    save: (data: string, defaultPath?: string) =>
      ipcRenderer.invoke('file:save', { data, defaultPath }) as Promise<string | null>,
    recent: () => ipcRenderer.invoke('file:recent') as Promise<string[]>,
    addRecent: (filePath: string) =>
      ipcRenderer.invoke('file:add-recent', filePath) as Promise<void>,
  },

  // === Menu events ===
  menu: {
    on: (event: string, callback: (...args: unknown[]) => void) => {
      ipcRenderer.on(`menu:${event}`, (_e, ...args) => callback(...args));
    },
  },

  // === Notifications ===
  notify: (title: string, body: string) =>
    ipcRenderer.invoke('notify', { title, body }) as Promise<void>,
});
