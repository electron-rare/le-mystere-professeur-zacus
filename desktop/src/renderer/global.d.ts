// Type declarations for the contextBridge API exposed by the preload script

interface SerialPort {
  path: string;
  name: string;
  vendorId?: string;
  productId?: string;
}

interface ZacusDevice {
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

interface OTAProgressEvent {
  deviceId: string;
  percent: number;
  stage: 'uploading' | 'verifying' | 'rebooting';
}

interface ZacusAPI {
  serial: {
    list: () => Promise<SerialPort[]>;
    connect: (port: string, baud: number) => Promise<boolean>;
    disconnect: (port: string) => Promise<void>;
    write: (port: string, data: string) => Promise<void>;
    flash: (port: string, firmwarePath: string) => Promise<void>;
    onData: (callback: (port: string, data: string) => void) => void;
    onPlugged: (callback: (port: string) => void) => void;
    onUnplugged: (callback: (port: string) => void) => void;
  };
  ble: {
    scan: () => Promise<void>;
    stopScan: () => Promise<void>;
    connect: (deviceId: string) => Promise<boolean>;
    disconnect: (deviceId: string) => Promise<void>;
    write: (deviceId: string, characteristic: string, data: string) => Promise<void>;
    dfu: (deviceId: string, firmwarePath: string) => Promise<void>;
    onDiscovered: (callback: (device: ZacusDevice) => void) => void;
    onData: (callback: (deviceId: string, characteristic: string, data: string) => void) => void;
  };
  wifi: {
    discover: () => Promise<ZacusDevice[]>;
    wsConnect: (url: string) => Promise<boolean>;
    wsSend: (data: string) => Promise<void>;
    wsDisconnect: () => Promise<void>;
    http: (url: string, method: string, body?: string, headers?: Record<string, string>) => Promise<{ status: number; data: string }>;
    onWsMessage: (callback: (data: string) => void) => void;
    onServiceFound: (callback: (service: ZacusDevice) => void) => void;
  };
  ota: {
    check: (deviceId: string) => Promise<{ current: string; available: string; needsUpdate: boolean }>;
    update: (deviceId: string, method: 'wifi' | 'ble' | 'usb', firmwarePath: string) => Promise<void>;
    rollback: (deviceId: string) => Promise<boolean>;
    onProgress: (callback: (event: OTAProgressEvent) => void) => void;
    onComplete: (callback: (deviceId: string, success: boolean, error?: string) => void) => void;
  };
  file: {
    open: (filters?: Array<{ name: string; extensions: string[] }>) => Promise<string | null>;
    save: (data: string, defaultPath?: string) => Promise<string | null>;
    recent: () => Promise<string[]>;
    addRecent: (filePath: string) => Promise<void>;
  };
  menu: {
    on: (event: string, callback: (...args: unknown[]) => void) => void;
  };
  notify: (title: string, body: string) => Promise<void>;
}

declare global {
  interface Window {
    zacus: ZacusAPI;
  }
}

export {};
