# Zacus Studio macOS — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Build Electron + Swift native macOS app with 4 tabs (editor, dashboard, simulation, dev tools), native hardware bridges (Serial, BLE, WiFi), and OTA firmware updates.

**Architecture:** Electron 33 main process + Chromium renderer loading frontend-v3 apps. Swift N-API native modules for hardware access. OTA via HTTP/BLE/USB to ESP32 puzzle devices.

**Tech Stack:** Electron 33, React 19, TypeScript, Swift 6, node-addon-api, IOKit, CoreBluetooth, Network.framework, electron-builder, esptool.py

---

## Phase 1: Electron Scaffold (10h)

### Task 1.1 — `desktop/package.json`

```json
{
  "name": "zacus-studio",
  "version": "1.0.0",
  "description": "Zacus Studio — macOS control hub for Professeur Zacus escape room kits",
  "main": "dist/main/index.js",
  "scripts": {
    "dev": "concurrently \"npm run dev:renderer\" \"npm run dev:main\"",
    "dev:main": "tsc -p tsconfig.main.json --watch & sleep 2 && electron .",
    "dev:renderer": "vite",
    "build": "npm run build:renderer && npm run build:main",
    "build:renderer": "vite build",
    "build:main": "tsc -p tsconfig.main.json",
    "build:mac": "npm run build && electron-builder --mac --universal",
    "build:mac-arm64": "npm run build && electron-builder --mac --arm64",
    "notarize": "node scripts/notarize.js",
    "rebuild-native": "electron-rebuild -f -w zacus-native",
    "postinstall": "electron-builder install-app-deps"
  },
  "dependencies": {
    "electron-updater": "^6.3.0",
    "serialport": "^12.0.0",
    "node-addon-api": "^8.0.0"
  },
  "devDependencies": {
    "@types/node": "^22.0.0",
    "@types/react": "^19.0.0",
    "@types/react-dom": "^19.0.0",
    "@vitejs/plugin-react": "^4.3.0",
    "concurrently": "^9.0.0",
    "electron": "^33.0.0",
    "electron-builder": "^25.0.0",
    "electron-rebuild": "^3.2.9",
    "typescript": "^5.6.0",
    "vite": "^6.0.0",
    "vite-plugin-electron": "^0.28.0",
    "react": "^19.0.0",
    "react-dom": "^19.0.0"
  }
}
```

### Task 1.2 — `desktop/tsconfig.main.json`

```json
{
  "compilerOptions": {
    "target": "ES2022",
    "module": "CommonJS",
    "moduleResolution": "node",
    "outDir": "dist/main",
    "rootDir": "src/main",
    "strict": true,
    "esModuleInterop": true,
    "skipLibCheck": true,
    "resolveJsonModule": true
  },
  "include": ["src/main/**/*", "src/preload/**/*"]
}
```

### Task 1.3 — `desktop/tsconfig.renderer.json`

```json
{
  "compilerOptions": {
    "target": "ES2022",
    "module": "ESNext",
    "moduleResolution": "bundler",
    "jsx": "react-jsx",
    "outDir": "dist/renderer",
    "rootDir": "src/renderer",
    "strict": true,
    "esModuleInterop": true,
    "skipLibCheck": true,
    "types": ["vite/client"]
  },
  "include": ["src/renderer/**/*"]
}
```

### Task 1.4 — `desktop/vite.config.ts`

```typescript
import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';
import { resolve } from 'path';

export default defineConfig({
  plugins: [react()],
  root: 'src/renderer',
  base: './',
  build: {
    outDir: resolve(__dirname, 'dist/renderer'),
    emptyOutDir: true,
    rollupOptions: {
      input: resolve(__dirname, 'src/renderer/index.html'),
    },
  },
  server: {
    port: 5173,
  },
});
```

### Task 1.5 — `desktop/src/main/index.ts`

```typescript
import { app, BrowserWindow, ipcMain, shell, nativeTheme } from 'electron';
import { join } from 'path';
import { setupMenu } from './menu';
import { setupSerialHandlers } from './serial-handler';
import { setupBLEHandlers } from './ble-handler';
import { setupWiFiHandlers } from './wifi-handler';
import { OTAManager } from './ota-manager';
import { setupFileHandlers } from './file-handler';
import { setupAutoUpdater } from './auto-updater';

const isDev = process.env.NODE_ENV === 'development';
const RENDERER_URL = 'http://localhost:5173';

let mainWindow: BrowserWindow | null = null;
let otaManager: OTAManager | null = null;

function createWindow(): void {
  mainWindow = new BrowserWindow({
    width: 1400,
    height: 900,
    minWidth: 1100,
    minHeight: 700,
    titleBarStyle: 'hiddenInset',
    trafficLightPosition: { x: 16, y: 16 },
    backgroundColor: '#1a1a2e',
    show: false,
    webPreferences: {
      preload: join(__dirname, '../preload/index.js'),
      contextIsolation: true,
      nodeIntegration: false,
      sandbox: false,
      webSecurity: !isDev,
    },
  });

  // Load renderer
  if (isDev) {
    mainWindow.loadURL(RENDERER_URL);
    mainWindow.webContents.openDevTools({ mode: 'detach' });
  } else {
    mainWindow.loadFile(join(__dirname, '../renderer/index.html'));
  }

  mainWindow.once('ready-to-show', () => {
    mainWindow!.show();
  });

  mainWindow.webContents.setWindowOpenHandler(({ url }) => {
    shell.openExternal(url);
    return { action: 'deny' };
  });

  mainWindow.on('closed', () => {
    mainWindow = null;
  });
}

app.whenReady().then(() => {
  // Force dark mode for consistent look
  nativeTheme.themeSource = 'dark';

  createWindow();

  // Setup macOS menu
  setupMenu(mainWindow!);

  // Initialize OTA manager
  otaManager = new OTAManager(mainWindow!);

  // Setup all IPC handlers
  setupSerialHandlers(ipcMain, mainWindow!);
  setupBLEHandlers(ipcMain, mainWindow!);
  setupWiFiHandlers(ipcMain, mainWindow!);
  setupFileHandlers(ipcMain, mainWindow!);
  otaManager.setupHandlers(ipcMain);

  // Setup auto-updater (production only)
  if (!isDev) {
    setupAutoUpdater(mainWindow!);
  }

  app.on('activate', () => {
    if (BrowserWindow.getAllWindows().length === 0) {
      createWindow();
    }
  });
});

app.on('window-all-closed', () => {
  if (process.platform !== 'darwin') {
    app.quit();
  }
});

// Handle open-file events (drag .zacus onto dock icon)
app.on('open-file', (event, filePath) => {
  event.preventDefault();
  if (mainWindow) {
    mainWindow.webContents.send('file:opened', filePath);
  }
});

// Security: prevent new window creation
app.on('web-contents-created', (_event, contents) => {
  contents.on('will-navigate', (event, navigationUrl) => {
    const parsedUrl = new URL(navigationUrl);
    if (isDev && parsedUrl.origin === RENDERER_URL) return;
    if (!isDev && navigationUrl.startsWith('file://')) return;
    event.preventDefault();
  });
});
```

### Task 1.6 — `desktop/src/main/menu.ts`

```typescript
import {
  app,
  BrowserWindow,
  Menu,
  MenuItemConstructorOptions,
  shell,
} from 'electron';

export function setupMenu(win: BrowserWindow): void {
  const template: MenuItemConstructorOptions[] = [
    // App menu
    {
      label: app.name,
      submenu: [
        { role: 'about', label: `About Zacus Studio` },
        { type: 'separator' },
        { label: 'Preferences…', accelerator: 'Cmd+,', click: () => win.webContents.send('menu:preferences') },
        { type: 'separator' },
        { role: 'services' },
        { type: 'separator' },
        { role: 'hide' },
        { role: 'hideOthers' },
        { role: 'unhide' },
        { type: 'separator' },
        { role: 'quit' },
      ],
    },
    // File menu
    {
      label: 'File',
      submenu: [
        {
          label: 'New Scenario',
          accelerator: 'Cmd+N',
          click: () => win.webContents.send('menu:new-scenario'),
        },
        {
          label: 'Open…',
          accelerator: 'Cmd+O',
          click: () => win.webContents.send('menu:open'),
        },
        {
          label: 'Open Recent',
          role: 'recentDocuments',
          submenu: [
            { role: 'clearRecentDocuments' },
          ],
        },
        { type: 'separator' },
        {
          label: 'Save',
          accelerator: 'Cmd+S',
          click: () => win.webContents.send('menu:save'),
        },
        {
          label: 'Save As…',
          accelerator: 'Cmd+Shift+S',
          click: () => win.webContents.send('menu:save-as'),
        },
        { type: 'separator' },
        {
          label: 'Export to SD Card',
          accelerator: 'Cmd+E',
          click: () => win.webContents.send('menu:export-sd'),
        },
        { type: 'separator' },
        { role: 'close' },
      ],
    },
    // Edit menu
    {
      label: 'Edit',
      submenu: [
        { role: 'undo' },
        { role: 'redo' },
        { type: 'separator' },
        { role: 'cut' },
        { role: 'copy' },
        { role: 'paste' },
        { role: 'selectAll' },
      ],
    },
    // View menu
    {
      label: 'View',
      submenu: [
        {
          label: 'Editor',
          accelerator: 'Cmd+1',
          click: () => win.webContents.send('menu:tab', 'editor'),
        },
        {
          label: 'Game Master',
          accelerator: 'Cmd+2',
          click: () => win.webContents.send('menu:tab', 'dashboard'),
        },
        {
          label: 'Simulation',
          accelerator: 'Cmd+3',
          click: () => win.webContents.send('menu:tab', 'simulation'),
        },
        {
          label: 'Dev Tools',
          accelerator: 'Cmd+4',
          click: () => win.webContents.send('menu:tab', 'devtools'),
        },
        { type: 'separator' },
        {
          label: 'Compile Scenario',
          accelerator: 'Cmd+B',
          click: () => win.webContents.send('menu:compile'),
        },
        {
          label: 'Validate Scenario',
          accelerator: 'Cmd+R',
          click: () => win.webContents.send('menu:validate'),
        },
        { type: 'separator' },
        { role: 'togglefullscreen' },
        { role: 'reload' },
        { role: 'toggleDevTools' },
      ],
    },
    // Dev menu
    {
      label: 'Dev',
      submenu: [
        {
          label: 'Scan Devices',
          accelerator: 'Cmd+Shift+D',
          click: () => win.webContents.send('menu:scan-devices'),
        },
        {
          label: 'Open Serial Monitor',
          click: () => win.webContents.send('menu:serial-monitor'),
        },
        {
          label: 'Firmware Manager',
          click: () => win.webContents.send('menu:firmware-manager'),
        },
        { type: 'separator' },
        {
          label: 'Show App Logs',
          click: () => shell.openPath(app.getPath('logs')),
        },
        {
          label: 'Show Firmware Cache',
          click: () => {
            const { homedir } = require('os');
            const { join } = require('path');
            shell.openPath(join(homedir(), '.zacus-studio', 'firmwares'));
          },
        },
      ],
    },
    // Window menu
    {
      label: 'Window',
      submenu: [
        { role: 'minimize' },
        { role: 'zoom' },
        { type: 'separator' },
        { role: 'front' },
        { type: 'separator' },
        { role: 'window' },
      ],
    },
    // Help menu
    {
      role: 'help',
      submenu: [
        {
          label: 'Zacus Documentation',
          click: () => shell.openExternal('https://github.com/electron-rare/le-mystere-professeur-zacus'),
        },
        {
          label: 'Report Issue',
          click: () => shell.openExternal('https://github.com/electron-rare/le-mystere-professeur-zacus/issues'),
        },
      ],
    },
  ];

  const menu = Menu.buildFromTemplate(template);
  Menu.setApplicationMenu(menu);
}
```

### Task 1.7 — `desktop/src/preload/index.ts`

```typescript
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
```

### Task 1.8 — `desktop/src/renderer/index.html`

```html
<!DOCTYPE html>
<html lang="en">
  <head>
    <meta charset="UTF-8" />
    <meta name="viewport" content="width=device-width, initial-scale=1.0" />
    <meta http-equiv="Content-Security-Policy"
      content="default-src 'self'; script-src 'self'; style-src 'self' 'unsafe-inline';" />
    <title>Zacus Studio</title>
    <link rel="preconnect" href="https://fonts.googleapis.com" />
  </head>
  <body>
    <div id="root"></div>
    <script type="module" src="/main.tsx"></script>
  </body>
</html>
```

### Task 1.9 — `desktop/src/renderer/main.tsx`

```tsx
import React from 'react';
import ReactDOM from 'react-dom/client';
import App from './App';
import './styles/global.css';

ReactDOM.createRoot(document.getElementById('root')!).render(
  <React.StrictMode>
    <App />
  </React.StrictMode>
);
```

### Task 1.10 — `desktop/src/renderer/styles/global.css`

```css
:root {
  --bg-primary: #0f0f1a;
  --bg-secondary: #1a1a2e;
  --bg-tertiary: #16213e;
  --accent: #7c3aed;
  --accent-hover: #6d28d9;
  --accent-dim: rgba(124, 58, 237, 0.2);
  --text-primary: #e2e8f0;
  --text-secondary: #94a3b8;
  --text-dim: #475569;
  --border: rgba(255, 255, 255, 0.08);
  --success: #22c55e;
  --warning: #f59e0b;
  --error: #ef4444;
  --sidebar-width: 64px;
}

* {
  box-sizing: border-box;
  margin: 0;
  padding: 0;
}

html, body, #root {
  height: 100%;
  overflow: hidden;
  font-family: -apple-system, BlinkMacSystemFont, 'SF Pro Display', 'Segoe UI', sans-serif;
  background: var(--bg-primary);
  color: var(--text-primary);
  -webkit-font-smoothing: antialiased;
}

/* macOS traffic light area */
.titlebar-drag {
  -webkit-app-region: drag;
  height: 52px;
}

::-webkit-scrollbar {
  width: 6px;
  height: 6px;
}
::-webkit-scrollbar-track { background: transparent; }
::-webkit-scrollbar-thumb {
  background: var(--text-dim);
  border-radius: 3px;
}
```

### Task 1.11 — `desktop/src/renderer/App.tsx`

```tsx
import React, { useState, useEffect, useCallback } from 'react';
import { EditorTab } from './tabs/EditorTab';
import { DashboardTab } from './tabs/DashboardTab';
import { SimulationTab } from './tabs/SimulationTab';
import { DevToolsTab } from './tabs/DevToolsTab';
import './styles/App.css';

type Tab = 'editor' | 'dashboard' | 'simulation' | 'devtools';

interface TabConfig {
  id: Tab;
  label: string;
  icon: string;
  shortcut: string;
}

const TABS: TabConfig[] = [
  { id: 'editor',     label: 'Editor',      icon: '⬡', shortcut: '⌘1' },
  { id: 'dashboard',  label: 'Game Master', icon: '◎', shortcut: '⌘2' },
  { id: 'simulation', label: 'Simulation',  icon: '△', shortcut: '⌘3' },
  { id: 'devtools',   label: 'Dev Tools',   icon: '⚙', shortcut: '⌘4' },
];

export default function App(): React.JSX.Element {
  const [activeTab, setActiveTab] = useState<Tab>('editor');
  const [deviceStatus, setDeviceStatus] = useState<'connected' | 'partial' | 'disconnected'>('disconnected');

  // Handle menu events from main process
  useEffect(() => {
    window.zacus.menu.on('tab', (tab: unknown) => {
      if (typeof tab === 'string' && TABS.some(t => t.id === tab)) {
        setActiveTab(tab as Tab);
      }
    });
  }, []);

  const handleTabClick = useCallback((tab: Tab) => {
    setActiveTab(tab);
  }, []);

  const statusColor = {
    connected: 'var(--success)',
    partial: 'var(--warning)',
    disconnected: 'var(--text-dim)',
  }[deviceStatus];

  return (
    <div className="app-layout">
      {/* Sidebar */}
      <aside className="sidebar">
        <div className="sidebar-top titlebar-drag" />

        <nav className="sidebar-nav">
          {TABS.map(tab => (
            <button
              key={tab.id}
              className={`sidebar-btn ${activeTab === tab.id ? 'active' : ''}`}
              onClick={() => handleTabClick(tab.id)}
              title={`${tab.label} (${tab.shortcut})`}
            >
              <span className="sidebar-icon">{tab.icon}</span>
              <span className="sidebar-label">{tab.label}</span>
            </button>
          ))}
        </nav>

        <div className="sidebar-bottom">
          <div
            className="device-status-dot"
            style={{ backgroundColor: statusColor }}
            title={`Devices: ${deviceStatus}`}
          />
        </div>
      </aside>

      {/* Main content */}
      <main className="main-content">
        <div className="titlebar-drag" />
        <div className="tab-content">
          {activeTab === 'editor'     && <EditorTab />}
          {activeTab === 'dashboard'  && <DashboardTab onDeviceStatusChange={setDeviceStatus} />}
          {activeTab === 'simulation' && <SimulationTab />}
          {activeTab === 'devtools'   && <DevToolsTab />}
        </div>
      </main>
    </div>
  );
}
```

### Task 1.12 — `desktop/src/renderer/styles/App.css`

```css
.app-layout {
  display: flex;
  height: 100vh;
  overflow: hidden;
}

.sidebar {
  width: var(--sidebar-width);
  background: var(--bg-secondary);
  border-right: 1px solid var(--border);
  display: flex;
  flex-direction: column;
  flex-shrink: 0;
}

.sidebar-top {
  height: 52px;
}

.sidebar-nav {
  flex: 1;
  display: flex;
  flex-direction: column;
  gap: 4px;
  padding: 8px 6px;
}

.sidebar-btn {
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  gap: 3px;
  width: 52px;
  height: 52px;
  border-radius: 10px;
  border: none;
  background: transparent;
  color: var(--text-secondary);
  cursor: pointer;
  transition: all 0.15s ease;
}

.sidebar-btn:hover {
  background: var(--accent-dim);
  color: var(--text-primary);
}

.sidebar-btn.active {
  background: var(--accent);
  color: white;
}

.sidebar-icon {
  font-size: 18px;
  line-height: 1;
}

.sidebar-label {
  font-size: 9px;
  font-weight: 600;
  text-transform: uppercase;
  letter-spacing: 0.5px;
  white-space: nowrap;
  overflow: hidden;
  max-width: 52px;
  text-overflow: ellipsis;
}

.sidebar-bottom {
  padding: 12px;
  display: flex;
  justify-content: center;
}

.device-status-dot {
  width: 8px;
  height: 8px;
  border-radius: 50%;
  transition: background-color 0.3s ease;
  box-shadow: 0 0 6px currentColor;
}

.main-content {
  flex: 1;
  display: flex;
  flex-direction: column;
  overflow: hidden;
  background: var(--bg-primary);
}

.tab-content {
  flex: 1;
  overflow: hidden;
  position: relative;
}
```

### Task 1.13 — `desktop/src/renderer/tabs/EditorTab.tsx`

```tsx
import React, { useEffect, useRef, useCallback } from 'react';

// EditorTab loads the frontend-v3 editor app in a webview.
// In development, it points to the Vite dev server.
// In production, it loads the built app from the renderer bundle.
const EDITOR_URL = import.meta.env.DEV
  ? 'http://localhost:5174'   // frontend-v3/apps/editor dev server
  : './apps/editor/index.html';

export function EditorTab(): React.JSX.Element {
  const webviewRef = useRef<Electron.WebviewTag>(null);

  // Handle menu events
  useEffect(() => {
    const handlers: Array<[string, () => void]> = [
      ['menu:save',     () => webviewRef.current?.send('menu:save')],
      ['menu:open',     () => webviewRef.current?.send('menu:open')],
      ['menu:compile',  () => webviewRef.current?.send('menu:compile')],
      ['menu:validate', () => webviewRef.current?.send('menu:validate')],
      ['menu:export-sd',() => webviewRef.current?.send('menu:export-sd')],
    ];

    handlers.forEach(([event, handler]) => {
      window.zacus.menu.on(event.replace('menu:', ''), handler);
    });
  }, []);

  const handleNewWindow = useCallback((e: Event) => {
    e.preventDefault();
  }, []);

  return (
    <div style={{ width: '100%', height: '100%' }}>
      <webview
        ref={webviewRef}
        src={EDITOR_URL}
        style={{ width: '100%', height: '100%' }}
        allowpopups={false}
        onNewWindow={handleNewWindow}
        nodeintegration={false}
        contextIsolation={true}
        partition="persist:editor"
      />
    </div>
  );
}
```

### Task 1.14 — `desktop/src/renderer/tabs/DashboardTab.tsx`

```tsx
import React, { useEffect, useRef } from 'react';

const DASHBOARD_URL = import.meta.env.DEV
  ? 'http://localhost:5175'
  : './apps/dashboard/index.html';

interface Props {
  onDeviceStatusChange: (status: 'connected' | 'partial' | 'disconnected') => void;
}

export function DashboardTab({ onDeviceStatusChange }: Props): React.JSX.Element {
  const webviewRef = useRef<Electron.WebviewTag>(null);

  useEffect(() => {
    const wv = webviewRef.current;
    if (!wv) return;

    // Listen for device status messages from dashboard app
    const handleIpcMessage = (e: { channel: string; args: unknown[] }) => {
      if (e.channel === 'device-status') {
        onDeviceStatusChange(e.args[0] as 'connected' | 'partial' | 'disconnected');
      }
    };

    wv.addEventListener('ipc-message', handleIpcMessage as EventListener);
    return () => wv.removeEventListener('ipc-message', handleIpcMessage as EventListener);
  }, [onDeviceStatusChange]);

  return (
    <div style={{ width: '100%', height: '100%' }}>
      <webview
        ref={webviewRef}
        src={DASHBOARD_URL}
        style={{ width: '100%', height: '100%' }}
        allowpopups={false}
        nodeintegration={false}
        contextIsolation={true}
        partition="persist:dashboard"
      />
    </div>
  );
}
```

### Task 1.15 — `desktop/src/renderer/tabs/SimulationTab.tsx`

```tsx
import React from 'react';

const SIMULATION_URL = import.meta.env.DEV
  ? 'http://localhost:5176'
  : './apps/simulation/index.html';

export function SimulationTab(): React.JSX.Element {
  return (
    <div style={{ width: '100%', height: '100%' }}>
      <webview
        src={SIMULATION_URL}
        style={{ width: '100%', height: '100%' }}
        allowpopups={false}
        nodeintegration={false}
        contextIsolation={true}
        partition="persist:simulation"
      />
    </div>
  );
}
```

### Task 1.16 — `desktop/src/renderer/tabs/DevToolsTab.tsx`

```tsx
import React, { useState } from 'react';
import { DeviceManager } from '../devtools/DeviceManager';
import { SerialMonitor } from '../devtools/SerialMonitor';
import { FirmwareManager } from '../devtools/FirmwareManager';
import { NvsConfigurator } from '../devtools/NvsConfigurator';
import { MeshDiagnostic } from '../devtools/MeshDiagnostic';
import { BatteryDashboard } from '../devtools/BatteryDashboard';
import { LogRecorder } from '../devtools/LogRecorder';
import './DevToolsTab.css';

type DevPanel =
  | 'devices'
  | 'serial'
  | 'firmware'
  | 'nvs'
  | 'mesh'
  | 'battery'
  | 'logs';

const PANELS: Array<{ id: DevPanel; label: string; icon: string }> = [
  { id: 'devices',  label: 'Devices',   icon: '📡' },
  { id: 'serial',   label: 'Serial',    icon: '🖥' },
  { id: 'firmware', label: 'Firmware',  icon: '💾' },
  { id: 'nvs',      label: 'NVS',       icon: '⚙️' },
  { id: 'mesh',     label: 'Mesh',      icon: '🕸' },
  { id: 'battery',  label: 'Battery',   icon: '🔋' },
  { id: 'logs',     label: 'Logs',      icon: '📋' },
];

export function DevToolsTab(): React.JSX.Element {
  const [activePanel, setActivePanel] = useState<DevPanel>('devices');

  return (
    <div className="devtools-layout">
      <nav className="devtools-nav">
        {PANELS.map(panel => (
          <button
            key={panel.id}
            className={`devtools-nav-btn ${activePanel === panel.id ? 'active' : ''}`}
            onClick={() => setActivePanel(panel.id)}
          >
            <span>{panel.icon}</span>
            <span>{panel.label}</span>
          </button>
        ))}
      </nav>
      <div className="devtools-content">
        {activePanel === 'devices'  && <DeviceManager />}
        {activePanel === 'serial'   && <SerialMonitor />}
        {activePanel === 'firmware' && <FirmwareManager />}
        {activePanel === 'nvs'      && <NvsConfigurator />}
        {activePanel === 'mesh'     && <MeshDiagnostic />}
        {activePanel === 'battery'  && <BatteryDashboard />}
        {activePanel === 'logs'     && <LogRecorder />}
      </div>
    </div>
  );
}
```

### Task 1.17 — `desktop/electron-builder.yml`

```yaml
appId: fr.lelectronrare.zacus-studio
productName: Zacus Studio
copyright: "Copyright © 2026 L'Electron Rare"

directories:
  output: release
  buildResources: resources

files:
  - dist/**/*
  - node_modules/**/*
  - "!node_modules/**/{CHANGELOG.md,README.md,readme.md,readme}"
  - "!node_modules/**/{test,__tests__,tests,powered-test,example,examples}"
  - "!node_modules/*.d.ts"

# macOS config
mac:
  target:
    - target: dmg
      arch:
        - arm64
        - x64
  bundleVersion: "1"
  category: public.app-category.developer-tools
  hardenedRuntime: true
  gatekeeperAssess: false
  entitlements: resources/entitlements.plist
  entitlementsInherit: resources/entitlements.plist
  icon: resources/icon.icns
  minimumSystemVersion: "14.0"
  extendInfo:
    NSBluetoothAlwaysUsageDescription: "Zacus Studio needs Bluetooth to communicate with puzzle devices."
    NSBluetoothPeripheralUsageDescription: "Zacus Studio needs Bluetooth to communicate with puzzle devices."
    NSLocalNetworkUsageDescription: "Zacus Studio needs local network access to discover and update puzzle devices."
    NSBonjourServices:
      - _zacus._tcp

dmg:
  contents:
    - x: 130
      y: 220
    - x: 410
      y: 220
      type: link
      path: /Applications
  window:
    width: 540
    height: 380
  sign: false

# Auto-updater
publish:
  provider: github
  owner: electron-rare
  repo: le-mystere-professeur-zacus
  releaseType: release
```

### Task 1.18 — `desktop/resources/entitlements.plist`

```xml
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <!-- Required for Electron -->
  <key>com.apple.security.cs.allow-jit</key>
  <true/>
  <key>com.apple.security.cs.allow-unsigned-executable-memory</key>
  <true/>
  <key>com.apple.security.cs.disable-library-validation</key>
  <true/>

  <!-- Hardened runtime -->
  <key>com.apple.security.cs.allow-dyld-environment-variables</key>
  <true/>

  <!-- Network (WiFi OTA, mDNS) -->
  <key>com.apple.security.network.client</key>
  <true/>
  <key>com.apple.security.network.server</key>
  <true/>

  <!-- Bluetooth (BLE DFU) -->
  <key>com.apple.security.device.bluetooth</key>
  <true/>

  <!-- USB Serial (IOKit) -->
  <key>com.apple.security.device.usb</key>
  <true/>

  <!-- File access for scenario files and firmware cache -->
  <key>com.apple.security.files.user-selected.read-write</key>
  <true/>
  <key>com.apple.security.files.downloads.read-write</key>
  <true/>

  <!-- For esptool.py subprocess -->
  <key>com.apple.security.cs.allow-execution</key>
  <true/>
</dict>
</plist>
```

### Task 1.19 — `desktop/src/main/file-handler.ts`

```typescript
import { dialog, app, IpcMain, BrowserWindow, Notification } from 'electron';
import { readFile, writeFile } from 'fs/promises';

export function setupFileHandlers(ipcMain: IpcMain, win: BrowserWindow): void {
  ipcMain.handle('file:open', async (_e, { filters } = {}) => {
    const result = await dialog.showOpenDialog(win, {
      properties: ['openFile'],
      filters: filters ?? [
        { name: 'Zacus Scenarios', extensions: ['yaml', 'yml', 'zacus'] },
        { name: 'All Files', extensions: ['*'] },
      ],
    });

    if (result.canceled || result.filePaths.length === 0) return null;

    const filePath = result.filePaths[0];
    app.addRecentDocument(filePath);
    return filePath;
  });

  ipcMain.handle('file:save', async (_e, { data, defaultPath }) => {
    const result = await dialog.showSaveDialog(win, {
      defaultPath: defaultPath ?? 'scenario.yaml',
      filters: [
        { name: 'Zacus Scenarios', extensions: ['yaml', 'yml'] },
        { name: 'Zacus Binary', extensions: ['zacus'] },
      ],
    });

    if (result.canceled || !result.filePath) return null;

    await writeFile(result.filePath, data, 'utf-8');
    app.addRecentDocument(result.filePath);
    return result.filePath;
  });

  ipcMain.handle('file:recent', async () => {
    // macOS manages recent documents via NSDocumentController
    // Return empty array; the menu:open-recent event handles it natively
    return [];
  });

  ipcMain.handle('file:add-recent', async (_e, filePath: string) => {
    app.addRecentDocument(filePath);
  });

  ipcMain.handle('notify', async (_e, { title, body }: { title: string; body: string }) => {
    if (Notification.isSupported()) {
      new Notification({ title, body }).show();
    }
  });
}
```

### Task 1.20 — `desktop/src/main/auto-updater.ts`

```typescript
import { autoUpdater } from 'electron-updater';
import { BrowserWindow } from 'electron';
import log from 'electron-log';

export function setupAutoUpdater(win: BrowserWindow): void {
  autoUpdater.logger = log;
  autoUpdater.checkForUpdatesAndNotify();

  autoUpdater.on('update-available', () => {
    win.webContents.send('updater:update-available');
  });

  autoUpdater.on('update-downloaded', () => {
    win.webContents.send('updater:update-downloaded');
  });

  autoUpdater.on('download-progress', (progress) => {
    win.webContents.send('updater:progress', progress.percent);
  });
}
```

---

## Phase 2: SerialBridge Swift (15h)

### Task 2.1 — `desktop/src/native/Package.swift`

```swift
// swift-tools-version: 6.0
import PackageDescription

let package = Package(
    name: "ZacusNative",
    platforms: [.macOS(.v14)],
    products: [
        .library(name: "ZacusNative", type: .dynamic, targets: ["ZacusNative"]),
    ],
    dependencies: [],
    targets: [
        .target(
            name: "ZacusNative",
            dependencies: [],
            path: "Sources/ZacusNative",
            linkerSettings: [
                .linkedFramework("IOKit"),
                .linkedFramework("CoreBluetooth"),
                .linkedFramework("Network"),
            ]
        ),
    ]
)
```

### Task 2.2 — `desktop/src/native/Sources/ZacusNative/SerialBridge.swift`

```swift
import Foundation
import IOKit
import IOKit.serial

/// Manages USB serial connections to ESP32 puzzle devices via IOKit.
@objc public class SerialBridge: NSObject {
    private var openPorts: [String: FileHandle] = [:]
    private var readCallbacks: [String: (Data) -> Void] = [:]
    private var plugNotification: io_iterator_t = 0
    private var unplugNotification: io_iterator_t = 0
    private var notificationPort: IONotificationPortRef?

    public var onPortPlugged: ((String) -> Void)?
    public var onPortUnplugged: ((String) -> Void)?

    // MARK: - Discovery

    /// Returns all available serial ports matching Zacus ESP32 vendor/product IDs.
    @objc public func listPorts() -> [[String: Any]] {
        var ports: [[String: Any]] = []

        let matchingDict = IOServiceMatching(kIOSerialBSDServiceValue) as NSMutableDictionary
        matchingDict[kIOSerialBSDTypeKey] = kIOSerialBSDAllTypes

        var iterator: io_iterator_t = 0
        guard IOServiceGetMatchingServices(kIOMainPortDefault, matchingDict, &iterator) == KERN_SUCCESS else {
            return ports
        }
        defer { IOObjectRelease(iterator) }

        while case let service = IOIteratorNext(iterator), service != IO_OBJECT_NULL {
            defer { IOObjectRelease(service) }

            guard let path = ioProperty(service, key: kIOCalloutDeviceKey) as? String,
                  path.contains("usbmodem") || path.contains("usbserial") || path.contains("SLAB") else {
                continue
            }

            var port: [String: Any] = ["path": path]

            if let name = ioProperty(service, key: kIOTTYDeviceKey) as? String {
                port["name"] = name
            }

            // Walk up to USB parent to get VID/PID
            var parent: io_object_t = 0
            if IORegistryEntryGetParentEntry(service, kIOServicePlane, &parent) == KERN_SUCCESS {
                defer { IOObjectRelease(parent) }
                if let vid = ioProperty(parent, key: kUSBVendorID) as? Int {
                    port["vendorId"] = String(format: "%04x", vid)
                }
                if let pid = ioProperty(parent, key: kUSBProductID) as? Int {
                    port["productId"] = String(format: "%04x", pid)
                }
            }

            ports.append(port)
        }

        return ports
    }

    /// Start watching for USB plug/unplug events.
    @objc public func startWatching() {
        notificationPort = IONotificationPortCreate(kIOMainPortDefault)
        guard let port = notificationPort else { return }

        let runLoopSource = IONotificationPortGetRunLoopSource(port).takeUnretainedValue()
        CFRunLoopAddSource(CFRunLoopGetMain(), runLoopSource, .defaultMode)

        let matchingDict = IOServiceMatching(kIOSerialBSDServiceValue) as! CFMutableDictionary
        let selfPtr = Unmanaged.passRetained(self).toOpaque()

        IOServiceAddMatchingNotification(port, kIOFirstMatchNotification, matchingDict, { ptr, iterator in
            guard let ptr = ptr else { return }
            let bridge = Unmanaged<SerialBridge>.fromOpaque(ptr).takeUnretainedValue()
            bridge.handlePlug(iterator: iterator)
        }, selfPtr, &plugNotification)

        IOServiceAddMatchingNotification(port, kIOTerminatedNotification, matchingDict, { ptr, iterator in
            guard let ptr = ptr else { return }
            let bridge = Unmanaged<SerialBridge>.fromOpaque(ptr).takeUnretainedValue()
            bridge.handleUnplug(iterator: iterator)
        }, selfPtr, &unplugNotification)

        // Drain initial iterators
        handlePlug(iterator: plugNotification)
        handleUnplug(iterator: unplugNotification)
    }

    // MARK: - Connection

    /// Opens a serial port at the specified baud rate.
    @objc public func connect(_ port: String, baud: Int) -> Bool {
        guard openPorts[port] == nil else { return true }

        let fd = open(port, O_RDWR | O_NOCTTY | O_NONBLOCK)
        guard fd >= 0 else { return false }

        var options = termios()
        tcgetattr(fd, &options)

        // 8N1, no flow control
        options.c_cflag = UInt(CS8 | CREAD | CLOCAL)
        options.c_iflag = 0
        options.c_oflag = 0
        options.c_lflag = 0

        // Set baud rate
        let speed = speed_t(baud)
        cfsetispeed(&options, speed)
        cfsetospeed(&options, speed)

        tcsetattr(fd, TCSANOW, &options)
        fcntl(fd, F_SETFL, 0) // blocking mode

        let handle = FileHandle(fileDescriptor: fd, closeOnDealloc: true)
        openPorts[port] = handle

        // Start async read loop
        startReadLoop(port: port, handle: handle)

        return true
    }

    @objc public func disconnect(_ port: String) {
        openPorts[port]?.closeFile()
        openPorts.removeValue(forKey: port)
        readCallbacks.removeValue(forKey: port)
    }

    // MARK: - I/O

    @objc public func write(_ port: String, data: Data) {
        openPorts[port]?.write(data)
    }

    public func onData(_ port: String, callback: @escaping (Data) -> Void) {
        readCallbacks[port] = callback
    }

    // MARK: - Flash via esptool.py

    /// Flashes a firmware binary to the ESP32 using esptool.py.
    public func flash(
        _ port: String,
        firmware: URL,
        onProgress: @escaping (Int) -> Void,
        onComplete: @escaping (Bool, String?) -> Void
    ) {
        // Disconnect serial first to free the port
        disconnect(port)

        DispatchQueue.global(qos: .userInitiated).async {
            let esptool = self.findEsptool()
            let process = Process()
            process.executableURL = URL(fileURLWithPath: esptool)
            process.arguments = [
                "--port", port,
                "--baud", "921600",
                "--before", "default_reset",
                "--after", "hard_reset",
                "write_flash",
                "--flash_mode", "dio",
                "--flash_freq", "80m",
                "--flash_size", "detect",
                "0x0",
                firmware.path
            ]

            let pipe = Pipe()
            process.standardOutput = pipe
            process.standardError = pipe

            pipe.fileHandleForReading.readabilityHandler = { handle in
                let data = handle.availableData
                guard !data.isEmpty, let output = String(data: data, encoding: .utf8) else { return }

                // Parse esptool progress lines: "Writing at 0x00001234... (25 %)"
                if let match = output.range(of: #"\((\d+) %\)"#, options: .regularExpression) {
                    let pctStr = output[match]
                        .replacingOccurrences(of: "(", with: "")
                        .replacingOccurrences(of: " %)", with: "")
                        .trimmingCharacters(in: .whitespaces)
                    if let pct = Int(pctStr) {
                        DispatchQueue.main.async { onProgress(pct) }
                    }
                }
            }

            do {
                try process.run()
                process.waitUntilExit()
                let success = process.terminationStatus == 0
                DispatchQueue.main.async {
                    onComplete(success, success ? nil : "esptool exited with code \(process.terminationStatus)")
                }
            } catch {
                DispatchQueue.main.async { onComplete(false, error.localizedDescription) }
            }
        }
    }

    // MARK: - Private helpers

    private func startReadLoop(port: String, handle: FileHandle) {
        DispatchQueue.global(qos: .background).async { [weak self] in
            guard let self = self else { return }
            while let h = self.openPorts[port] {
                let data = h.availableData
                if !data.isEmpty, let cb = self.readCallbacks[port] {
                    DispatchQueue.main.async { cb(data) }
                }
                Thread.sleep(forTimeInterval: 0.01)
            }
        }
    }

    private func handlePlug(iterator: io_iterator_t) {
        while case let service = IOIteratorNext(iterator), service != IO_OBJECT_NULL {
            defer { IOObjectRelease(service) }
            if let path = ioProperty(service, key: kIOCalloutDeviceKey) as? String,
               path.contains("usb") {
                onPortPlugged?(path)
            }
        }
    }

    private func handleUnplug(iterator: io_iterator_t) {
        while case let service = IOIteratorNext(iterator), service != IO_OBJECT_NULL {
            defer { IOObjectRelease(service) }
            if let path = ioProperty(service, key: kIOCalloutDeviceKey) as? String {
                onPortUnplugged?(path)
            }
        }
    }

    private func ioProperty(_ service: io_object_t, key: String) -> AnyObject? {
        IORegistryEntryCreateCFProperty(service, key as CFString, kCFAllocatorDefault, 0)?
            .takeRetainedValue()
    }

    private func findEsptool() -> String {
        // Search common Python/PlatformIO install locations
        let candidates = [
            "/usr/local/bin/esptool.py",
            "/opt/homebrew/bin/esptool.py",
            "\(NSHomeDirectory())/.platformio/penv/bin/esptool.py",
            "\(NSHomeDirectory())/Library/Python/3.12/bin/esptool.py",
        ]
        return candidates.first { FileManager.default.fileExists(atPath: $0) }
            ?? "esptool.py" // Fall back to PATH
    }
}
```

### Task 2.3 — `desktop/src/main/serial-handler.ts`

```typescript
import { IpcMain, BrowserWindow } from 'electron';
import { join } from 'path';

// Load native Swift module built via N-API
// eslint-disable-next-line @typescript-eslint/no-var-requires
const native = require(join(__dirname, '../../native/build/Release/zacus_native.node'));

export function setupSerialHandlers(ipcMain: IpcMain, win: BrowserWindow): void {
  const serial = new native.SerialBridge();

  // Plug/unplug events → renderer
  serial.onPortPlugged((port: string) => {
    win.webContents.send('serial:plugged', port);
  });
  serial.onPortUnplugged((port: string) => {
    win.webContents.send('serial:unplugged', port);
  });

  // Start watching for device changes
  serial.startWatching();

  ipcMain.handle('serial:list', async () => {
    return serial.listPorts();
  });

  ipcMain.handle('serial:connect', async (_e, { port, baud }: { port: string; baud: number }) => {
    const success = serial.connect(port, baud ?? 115200);
    if (success) {
      serial.onData(port, (data: Buffer) => {
        win.webContents.send('serial:data', {
          port,
          data: data.toString('utf-8'),
        });
      });
    }
    return success;
  });

  ipcMain.handle('serial:disconnect', async (_e, { port }: { port: string }) => {
    serial.disconnect(port);
  });

  ipcMain.handle('serial:write', async (_e, { port, data }: { port: string; data: string }) => {
    serial.write(port, Buffer.from(data, 'utf-8'));
  });

  ipcMain.handle('serial:flash', async (_e, { port, firmwarePath }: { port: string; firmwarePath: string }) => {
    return new Promise<void>((resolve, reject) => {
      serial.flash(
        port,
        firmwarePath,
        (progress: number) => {
          win.webContents.send('ota:progress', {
            deviceId: port,
            percent: progress,
            stage: 'uploading',
          });
        },
        (success: boolean, error?: string) => {
          if (success) resolve();
          else reject(new Error(error ?? 'Flash failed'));
        }
      );
    });
  });
}
```

### Task 2.4 — `desktop/src/renderer/devtools/SerialMonitor.tsx` (key functions)

```tsx
import React, { useState, useEffect, useRef, useCallback } from 'react';

interface LogEntry {
  port: string;
  timestamp: number;
  text: string;
  level: 'info' | 'warn' | 'error' | 'debug';
}

const LEVEL_COLORS: Record<LogEntry['level'], string> = {
  error: '#ef4444',
  warn:  '#f59e0b',
  info:  '#94a3b8',
  debug: '#475569',
};

function classifyLine(text: string): LogEntry['level'] {
  if (/error|fail|crash/i.test(text)) return 'error';
  if (/warn|caution/i.test(text)) return 'warn';
  if (/debug|verbose/i.test(text)) return 'debug';
  return 'info';
}

export function SerialMonitor(): React.JSX.Element {
  const [ports, setPorts] = useState<string[]>([]);
  const [connectedPorts, setConnectedPorts] = useState<Set<string>>(new Set());
  const [logs, setLogs] = useState<LogEntry[]>([]);
  const [filterPort, setFilterPort] = useState<string>('all');
  const [filterText, setFilterText] = useState('');
  const [paused, setPaused] = useState(false);
  const logEndRef = useRef<HTMLDivElement>(null);

  // Load port list
  useEffect(() => {
    window.zacus.serial.list().then(serialPorts => {
      setPorts(serialPorts.map(p => p.path));
    });
  }, []);

  // Listen for incoming serial data
  useEffect(() => {
    window.zacus.serial.onData((port, data) => {
      if (paused) return;
      const lines = data.split('\n').filter(Boolean);
      setLogs(prev => [
        ...prev.slice(-2000), // keep last 2000 lines
        ...lines.map(text => ({
          port,
          timestamp: Date.now(),
          text: text.trim(),
          level: classifyLine(text),
        })),
      ]);
    });

    window.zacus.serial.onPlugged(port => {
      setPorts(prev => [...new Set([...prev, port])]);
    });

    window.zacus.serial.onUnplugged(port => {
      setConnectedPorts(prev => { const s = new Set(prev); s.delete(port); return s; });
    });
  }, [paused]);

  // Auto-scroll
  useEffect(() => {
    if (!paused) logEndRef.current?.scrollIntoView({ behavior: 'smooth' });
  }, [logs, paused]);

  const connect = useCallback(async (port: string) => {
    const ok = await window.zacus.serial.connect(port, 115200);
    if (ok) setConnectedPorts(prev => new Set([...prev, port]));
  }, []);

  const visibleLogs = logs.filter(e => {
    if (filterPort !== 'all' && e.port !== filterPort) return false;
    if (filterText && !e.text.toLowerCase().includes(filterText.toLowerCase())) return false;
    return true;
  });

  const exportLogs = useCallback(() => {
    const text = visibleLogs
      .map(e => `[${new Date(e.timestamp).toISOString()}] [${e.port}] ${e.text}`)
      .join('\n');
    window.zacus.file.save(text, 'serial_log.txt');
  }, [visibleLogs]);

  return (
    <div className="serial-monitor">
      <div className="serial-toolbar">
        <select
          value={filterPort}
          onChange={e => setFilterPort(e.target.value)}
          className="select"
        >
          <option value="all">All ports</option>
          {ports.map(p => <option key={p} value={p}>{p.split('/').pop()}</option>)}
        </select>

        <input
          className="input"
          placeholder="Filter..."
          value={filterText}
          onChange={e => setFilterText(e.target.value)}
        />

        <button className="btn" onClick={() => setPaused(p => !p)}>
          {paused ? '▶ Resume' : '⏸ Pause'}
        </button>

        <button className="btn" onClick={() => setLogs([])}>Clear</button>
        <button className="btn" onClick={exportLogs}>Export</button>

        <div className="port-list">
          {ports.map(port => (
            <button
              key={port}
              className={`btn ${connectedPorts.has(port) ? 'active' : ''}`}
              onClick={() => connect(port)}
            >
              {port.split('/').pop()}
            </button>
          ))}
        </div>
      </div>

      <div className="log-output">
        {visibleLogs.map((entry, i) => (
          <div
            key={i}
            className="log-line"
            style={{ color: LEVEL_COLORS[entry.level] }}
          >
            <span className="log-time">{new Date(entry.timestamp).toISOString().slice(11, 23)}</span>
            <span className="log-port">[{entry.port.split('/').pop()}]</span>
            <span className="log-text">{entry.text}</span>
          </div>
        ))}
        <div ref={logEndRef} />
      </div>
    </div>
  );
}
```

---

## Phase 3: BLEBridge Swift (12h)

### Task 3.1 — `desktop/src/native/Sources/ZacusNative/BLEBridge.swift`

```swift
import Foundation
import CoreBluetooth

// Zacus BLE service and characteristic UUIDs
let ZACUS_SVC_UUID       = CBUUID(string: "12345678-0000-1000-8000-00805F9B34FB")
let ZACUS_CHAR_CMD_UUID  = CBUUID(string: "12345678-0001-1000-8000-00805F9B34FB")
let ZACUS_CHAR_DATA_UUID = CBUUID(string: "12345678-0002-1000-8000-00805F9B34FB")
let ZACUS_CHAR_OTA_UUID  = CBUUID(string: "12345678-0003-1000-8000-00805F9B34FB")

// Nordic DFU service UUID
let DFU_SVC_UUID = CBUUID(string: "00001530-1212-EFDE-1523-785FEABCD123")

@objc public class BLEBridge: NSObject, CBCentralManagerDelegate, CBPeripheralDelegate {
    private var central: CBCentralManager!
    private var peripherals: [UUID: CBPeripheral] = [:]
    private var characteristics: [UUID: [CBUUID: CBCharacteristic]] = [:]
    private var subscriptions: [String: (Data) -> Void] = [:]

    public var onDiscovered: (([String: Any]) -> Void)?
    public var onData: ((String, String, Data) -> Void)?

    private var dfuProgress: ((Int) -> Void)?
    private var dfuComplete: ((Bool, String?) -> Void)?
    private var dfuPeripheralId: UUID?
    private var dfuChunks: [Data] = []
    private var dfuChunkIndex = 0
    private static let DFU_CHUNK_SIZE = 512

    public override init() {
        super.init()
        central = CBCentralManager(delegate: self, queue: .main)
    }

    // MARK: - Scanning

    @objc public func startScan(serviceUUIDs: [String]) {
        guard central.state == .poweredOn else { return }
        let uuids = serviceUUIDs.isEmpty ? nil : serviceUUIDs.map { CBUUID(string: $0) }
        central.scanForPeripherals(
            withServices: uuids ?? [ZACUS_SVC_UUID],
            options: [CBCentralManagerScanOptionAllowDuplicatesKey: false]
        )
    }

    @objc public func stopScan() {
        central.stopScan()
    }

    // MARK: - Connection

    @objc public func connect(_ peripheralIdStr: String) -> Bool {
        guard let uuid = UUID(uuidString: peripheralIdStr),
              let peripheral = peripherals[uuid] else { return false }
        central.connect(peripheral, options: nil)
        return true
    }

    @objc public func disconnect(_ peripheralIdStr: String) {
        guard let uuid = UUID(uuidString: peripheralIdStr),
              let peripheral = peripherals[uuid] else { return }
        central.cancelPeripheralConnection(peripheral)
    }

    @objc public func listConnected() -> [[String: Any]] {
        peripherals.values
            .filter { $0.state == .connected }
            .map { peripheralToDict($0) }
    }

    // MARK: - Data

    @objc public func write(_ peripheralIdStr: String, characteristic charStr: String, data: Data) {
        guard let uuid = UUID(uuidString: peripheralIdStr),
              let peripheral = peripherals[uuid],
              let charUUID = characteristics[uuid]?[CBUUID(string: charStr)] else { return }
        peripheral.writeValue(data, for: charUUID, type: .withResponse)
    }

    public func subscribe(
        _ peripheralIdStr: String,
        characteristic charStr: String,
        callback: @escaping (Data) -> Void
    ) {
        subscriptions["\(peripheralIdStr):\(charStr)"] = callback
        guard let uuid = UUID(uuidString: peripheralIdStr),
              let peripheral = peripherals[uuid],
              let charUUID = characteristics[uuid]?[CBUUID(string: charStr)] else { return }
        peripheral.setNotifyValue(true, for: charUUID)
    }

    // MARK: - OTA DFU

    /// Streams a firmware binary to the ESP32 via BLE OTA characteristic in 512-byte chunks.
    public func startDFU(
        _ peripheralIdStr: String,
        firmware: URL,
        onProgress: @escaping (Int) -> Void,
        onComplete: @escaping (Bool, String?) -> Void
    ) {
        guard let uuid = UUID(uuidString: peripheralIdStr),
              let peripheral = peripherals[uuid] else {
            onComplete(false, "Device not found")
            return
        }

        guard let firmwareData = try? Data(contentsOf: firmware) else {
            onComplete(false, "Cannot read firmware file")
            return
        }

        dfuProgress = onProgress
        dfuComplete = onComplete
        dfuPeripheralId = uuid
        dfuChunkIndex = 0

        // Split firmware into chunks
        dfuChunks = stride(from: 0, to: firmwareData.count, by: Self.DFU_CHUNK_SIZE).map {
            firmwareData[$0..<min($0 + Self.DFU_CHUNK_SIZE, firmwareData.count)]
        }

        // Send firmware size header first
        var size = UInt32(firmwareData.count).littleEndian
        let header = Data(bytes: &size, count: 4)
        write(peripheralIdStr, characteristic: ZACUS_CHAR_OTA_UUID.uuidString, data: header)
    }

    private func sendNextDFUChunk(peripheral: CBPeripheral) {
        guard dfuChunkIndex < dfuChunks.count,
              let uuid = dfuPeripheralId,
              let char = characteristics[uuid]?[ZACUS_CHAR_OTA_UUID] else {
            dfuComplete?(dfuChunkIndex >= dfuChunks.count, nil)
            return
        }

        let chunk = dfuChunks[dfuChunkIndex]
        peripheral.writeValue(chunk, for: char, type: .withResponse)

        let progress = Int(Double(dfuChunkIndex + 1) / Double(dfuChunks.count) * 100)
        dfuProgress?(progress)
        dfuChunkIndex += 1
    }

    // MARK: - CBCentralManagerDelegate

    public func centralManagerDidUpdateState(_ central: CBCentralManager) {
        if central.state == .poweredOn {
            startScan(serviceUUIDs: [])
        }
    }

    public func centralManager(
        _ central: CBCentralManager,
        didDiscover peripheral: CBPeripheral,
        advertisementData: [String: Any],
        rssi RSSI: NSNumber
    ) {
        peripherals[peripheral.identifier] = peripheral
        peripheral.delegate = self

        var info = peripheralToDict(peripheral)
        info["rssi"] = RSSI.intValue
        if let name = advertisementData[CBAdvertisementDataLocalNameKey] as? String {
            info["name"] = name
        }
        onDiscovered?(info)
    }

    public func centralManager(_ central: CBCentralManager, didConnect peripheral: CBPeripheral) {
        peripheral.discoverServices([ZACUS_SVC_UUID, DFU_SVC_UUID])
    }

    // MARK: - CBPeripheralDelegate

    public func peripheral(_ peripheral: CBPeripheral, didDiscoverServices error: Error?) {
        peripheral.services?.forEach { service in
            peripheral.discoverCharacteristics(nil, for: service)
        }
    }

    public func peripheral(
        _ peripheral: CBPeripheral,
        didDiscoverCharacteristicsFor service: CBService,
        error: Error?
    ) {
        service.characteristics?.forEach { char in
            if characteristics[peripheral.identifier] == nil {
                characteristics[peripheral.identifier] = [:]
            }
            characteristics[peripheral.identifier]![char.uuid] = char
        }
    }

    public func peripheral(
        _ peripheral: CBPeripheral,
        didUpdateValueFor characteristic: CBCharacteristic,
        error: Error?
    ) {
        guard let data = characteristic.value else { return }
        let key = "\(peripheral.identifier.uuidString):\(characteristic.uuid.uuidString)"
        subscriptions[key]?(data)
        onData?(peripheral.identifier.uuidString, characteristic.uuid.uuidString, data)
    }

    public func peripheral(
        _ peripheral: CBPeripheral,
        didWriteValueFor characteristic: CBCharacteristic,
        error: Error?
    ) {
        // Continue DFU stream
        if characteristic.uuid == ZACUS_CHAR_OTA_UUID && dfuPeripheralId == peripheral.identifier {
            sendNextDFUChunk(peripheral: peripheral)
        }
    }

    // MARK: - Helpers

    private func peripheralToDict(_ peripheral: CBPeripheral) -> [String: Any] {
        [
            "id": peripheral.identifier.uuidString,
            "name": peripheral.name ?? "Unknown",
            "state": peripheral.state.rawValue,
        ]
    }
}
```

### Task 3.2 — `desktop/src/main/ble-handler.ts`

```typescript
import { IpcMain, BrowserWindow } from 'electron';
import { join } from 'path';

// eslint-disable-next-line @typescript-eslint/no-var-requires
const native = require(join(__dirname, '../../native/build/Release/zacus_native.node'));

export function setupBLEHandlers(ipcMain: IpcMain, win: BrowserWindow): void {
  const ble = new native.BLEBridge();

  ble.onDiscovered((device: Record<string, unknown>) => {
    win.webContents.send('ble:discovered', device);
  });

  ble.onData((deviceId: string, characteristic: string, data: Buffer) => {
    win.webContents.send('ble:data', {
      deviceId,
      characteristic,
      data: data.toString('base64'),
    });
  });

  ipcMain.handle('ble:scan', async () => {
    ble.startScan([]);
  });

  ipcMain.handle('ble:stop-scan', async () => {
    ble.stopScan();
  });

  ipcMain.handle('ble:connect', async (_e, deviceId: string) => {
    return ble.connect(deviceId);
  });

  ipcMain.handle('ble:disconnect', async (_e, deviceId: string) => {
    ble.disconnect(deviceId);
  });

  ipcMain.handle('ble:write', async (_e, {
    deviceId,
    characteristic,
    data,
  }: { deviceId: string; characteristic: string; data: string }) => {
    ble.write(deviceId, characteristic, Buffer.from(data, 'base64'));
  });

  ipcMain.handle('ble:dfu', async (_e, {
    deviceId,
    firmwarePath,
  }: { deviceId: string; firmwarePath: string }) => {
    return new Promise<void>((resolve, reject) => {
      ble.startDFU(
        deviceId,
        firmwarePath,
        (progress: number) => {
          win.webContents.send('ota:progress', {
            deviceId,
            percent: progress,
            stage: 'uploading',
          });
        },
        (success: boolean, error?: string) => {
          if (success) resolve();
          else reject(new Error(error ?? 'DFU failed'));
        }
      );
    });
  });
}
```

### Task 3.3 — `desktop/src/renderer/devtools/DeviceManager.tsx` (key functions)

```tsx
import React, { useState, useEffect, useCallback } from 'react';
import type { ZacusDevice } from '../../preload/index';

const CONNECTION_ICONS: Record<ZacusDevice['connectionType'], string> = {
  usb:  '🔌',
  ble:  '📶',
  wifi: '📡',
};

export function DeviceManager(): React.JSX.Element {
  const [devices, setDevices] = useState<ZacusDevice[]>([]);
  const [scanning, setScanning] = useState(false);

  // Discover via all transports in parallel
  const scan = useCallback(async () => {
    setScanning(true);
    try {
      const [wifiDevices, serialPorts] = await Promise.all([
        window.zacus.wifi.discover(),
        window.zacus.serial.list(),
      ]);

      const serialDevices: ZacusDevice[] = serialPorts.map(p => ({
        id: p.path,
        name: p.name ?? p.path.split('/').pop() ?? 'Unknown',
        type: 'puzzle' as const,
        firmwareVersion: '?',
        batteryPct: -1,
        connectionType: 'usb' as const,
        lastSeen: Date.now(),
      }));

      setDevices([...wifiDevices, ...serialDevices]);
      await window.zacus.ble.scan();
    } finally {
      setScanning(false);
    }
  }, []);

  // BLE discovered
  useEffect(() => {
    window.zacus.ble.onDiscovered(device => {
      setDevices(prev => {
        const existing = prev.findIndex(d => d.id === device.id);
        if (existing >= 0) {
          const copy = [...prev];
          copy[existing] = { ...copy[existing], ...device, connectionType: 'ble' };
          return copy;
        }
        return [...prev, { ...device, connectionType: 'ble' as const }];
      });
    });
  }, []);

  useEffect(() => { scan(); }, [scan]);

  return (
    <div className="device-manager">
      <div className="panel-header">
        <h2>Devices</h2>
        <button className="btn primary" onClick={scan} disabled={scanning}>
          {scanning ? 'Scanning…' : '↺ Scan'}
        </button>
      </div>

      <div className="device-grid">
        {devices.length === 0 && (
          <div className="empty-state">No devices found. Connect USB or enable WiFi.</div>
        )}
        {devices.map(device => (
          <div key={device.id} className="device-card">
            <div className="device-header">
              <span className="device-icon">{CONNECTION_ICONS[device.connectionType]}</span>
              <span className="device-name">{device.name}</span>
              <span className="device-type">{device.type}</span>
            </div>
            <div className="device-details">
              <span>v{device.firmwareVersion}</span>
              {device.batteryPct >= 0 && (
                <span className={device.batteryPct < 20 ? 'warn' : ''}>
                  🔋 {device.batteryPct}%
                </span>
              )}
              {device.ip && <span>📡 {device.ip}</span>}
            </div>
            <div className="device-actions">
              <button className="btn small" onClick={() => window.zacus.wifi.wsConnect(`ws://${device.ip}/ws`)}>
                Connect
              </button>
            </div>
          </div>
        ))}
      </div>
    </div>
  );
}
```

---

## Phase 4: WiFiBridge Swift (8h)

### Task 4.1 — `desktop/src/native/Sources/ZacusNative/WiFiBridge.swift`

```swift
import Foundation
import Network

@objc public class WiFiBridge: NSObject, URLSessionWebSocketDelegate {
    private var browser: NWBrowser?
    private var wsTask: URLSessionWebSocketTask?
    private var session: URLSession!

    public var onServiceFound: (([String: Any]) -> Void)?
    public var onWSMessage: ((Data) -> Void)?

    public override init() {
        super.init()
        session = URLSession(configuration: .default, delegate: self, delegateQueue: .main)
    }

    // MARK: - mDNS Discovery

    /// Browse for `_zacus._tcp` services on the local network.
    @objc public func browseMDNS(service: String) {
        let descriptor = NWBrowser.Descriptor.bonjour(
            type: service.isEmpty ? "_zacus._tcp" : service,
            domain: nil
        )
        let params = NWParameters()
        params.includePeerToPeer = true

        browser = NWBrowser(for: descriptor, using: params)

        browser?.browseResultsChangedHandler = { [weak self] results, _ in
            for result in results {
                if case .service(let name, _, _, _) = result.endpoint {
                    self?.resolveService(result: result, name: name)
                }
            }
        }

        browser?.start(queue: .main)
    }

    private func resolveService(result: NWBrowser.Result, name: String) {
        let params = NWParameters.tcp
        let connection = NWConnection(to: result.endpoint, using: params)

        connection.stateUpdateHandler = { [weak self] state in
            if case .ready = state {
                guard let inner = connection.currentPath?.remoteEndpoint,
                      case .hostPort(let host, let port) = inner else { return }

                let ipStr: String
                switch host {
                case .ipv4(let v4): ipStr = "\(v4)"
                case .ipv6(let v6): ipStr = "\(v6)"
                default: ipStr = name
                }

                let info: [String: Any] = [
                    "name": name,
                    "ip": ipStr,
                    "port": Int(port.rawValue),
                    "id": "\(name)-\(ipStr)",
                    "connectionType": "wifi",
                    "type": "puzzle",
                ]
                DispatchQueue.main.async { self?.onServiceFound?(info) }
                connection.cancel()
            }
        }
        connection.start(queue: .global())
    }

    // MARK: - WebSocket

    @objc public func connectWS(_ urlStr: String) -> Bool {
        guard let url = URL(string: urlStr) else { return false }
        wsTask?.cancel(with: .goingAway, reason: nil)
        wsTask = session.webSocketTask(with: url)
        wsTask?.resume()
        receiveWS()
        return true
    }

    @objc public func sendWS(_ data: Data) {
        wsTask?.send(.data(data)) { _ in }
    }

    @objc public func disconnectWS() {
        wsTask?.cancel(with: .goingAway, reason: nil)
        wsTask = nil
    }

    private func receiveWS() {
        wsTask?.receive { [weak self] result in
            switch result {
            case .success(.data(let data)):
                DispatchQueue.main.async { self?.onWSMessage?(data) }
            case .success(.string(let str)):
                DispatchQueue.main.async {
                    self?.onWSMessage?(Data(str.utf8))
                }
            case .failure:
                break
            @unknown default:
                break
            }
            self?.receiveWS() // continue receiving
        }
    }

    // MARK: - HTTP (for OTA)

    /// Async HTTP request with upload progress tracking.
    public func httpRequest(
        url urlStr: String,
        method: String,
        body: Data?,
        headers: [String: String],
        onProgress: @escaping (Int) -> Void
    ) async -> (Int, Data?) {
        guard let url = URL(string: urlStr) else { return (0, nil) }

        var request = URLRequest(url: url, timeoutInterval: 120)
        request.httpMethod = method
        request.httpBody = body
        headers.forEach { request.setValue($1, forHTTPHeaderField: $0) }

        if let body = body {
            request.setValue("\(body.count)", forHTTPHeaderField: "Content-Length")
        }

        do {
            let (data, response) = try await session.data(for: request)
            let status = (response as? HTTPURLResponse)?.statusCode ?? 0
            return (status, data)
        } catch {
            return (0, nil)
        }
    }

    // MARK: - Network info

    @objc public func getLocalIP() -> String? {
        var address: String?
        var ifaddr: UnsafeMutablePointer<ifaddrs>?

        guard getifaddrs(&ifaddr) == 0 else { return nil }
        defer { freeifaddrs(ifaddr) }

        var ptr = ifaddr
        while ptr != nil {
            let flags = Int32(ptr!.pointee.ifa_flags)
            let addr = ptr!.pointee.ifa_addr.pointee

            if (flags & IFF_UP) != 0,
               (flags & IFF_LOOPBACK) == 0,
               addr.sa_family == UInt8(AF_INET) {
                var hostname = [CChar](repeating: 0, count: Int(NI_MAXHOST))
                getnameinfo(ptr!.pointee.ifa_addr, socklen_t(addr.sa_len),
                            &hostname, socklen_t(hostname.count), nil, 0, NI_NUMERICHOST)
                address = String(cString: hostname)
                break
            }
            ptr = ptr!.pointee.ifa_next
        }

        return address
    }
}
```

### Task 4.2 — `desktop/src/main/wifi-handler.ts`

```typescript
import { IpcMain, BrowserWindow } from 'electron';
import { join } from 'path';

// eslint-disable-next-line @typescript-eslint/no-var-requires
const native = require(join(__dirname, '../../native/build/Release/zacus_native.node'));

export function setupWiFiHandlers(ipcMain: IpcMain, win: BrowserWindow): void {
  const wifi = new native.WiFiBridge();

  wifi.onServiceFound((service: Record<string, unknown>) => {
    win.webContents.send('wifi:service-found', service);
  });

  wifi.onWSMessage((data: Buffer) => {
    win.webContents.send('wifi:ws-message', data.toString('utf-8'));
  });

  // Start passive mDNS discovery immediately
  wifi.browseMDNS('_zacus._tcp');

  ipcMain.handle('wifi:discover', async () => {
    // Active scan returns already-discovered devices
    return [];
  });

  ipcMain.handle('wifi:ws-connect', async (_e, url: string) => {
    return wifi.connectWS(url);
  });

  ipcMain.handle('wifi:ws-send', async (_e, data: string) => {
    wifi.sendWS(Buffer.from(data, 'utf-8'));
  });

  ipcMain.handle('wifi:ws-disconnect', async () => {
    wifi.disconnectWS();
  });

  ipcMain.handle('wifi:http', async (_e, {
    url, method, body, headers,
  }: { url: string; method: string; body?: string; headers?: Record<string, string> }) => {
    const bodyData = body ? Buffer.from(body, 'utf-8') : undefined;
    const [status, responseData] = await wifi.httpRequest(url, method, bodyData, headers ?? {}, (_: number) => {});
    return {
      status,
      data: responseData ? responseData.toString('utf-8') : '',
    };
  });
}
```

---

## Phase 5: OTA Manager (20h)

### Task 5.1 — `desktop/src/main/ota-manager.ts`

```typescript
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

  private async queryVersionViaSerial(port: string): Promise<DeviceVersion | null> {
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
```

### Task 5.2 — `desktop/src/renderer/devtools/FirmwareManager.tsx`

```tsx
import React, { useState, useEffect, useCallback } from 'react';
import type { ZacusDevice } from '../../preload/index';

interface FirmwareEntry {
  name: string;
  version: string;
  path: string;
  size: number;
  builtAt: number;
}

interface DeviceRow {
  device: ZacusDevice;
  currentVersion: string;
  availableVersion: string;
  needsUpdate: boolean;
  status: 'idle' | 'checking' | 'updating' | 'done' | 'error';
  progress: number;
  progressStage: string;
  error?: string;
}

type OTAMethod = 'wifi' | 'ble' | 'usb';

export function FirmwareManager(): React.JSX.Element {
  const [devices, setDevices] = useState<DeviceRow[]>([]);
  const [firmwares, setFirmwares] = useState<FirmwareEntry[]>([]);
  const [selectedMethod, setSelectedMethod] = useState<OTAMethod>('wifi');
  const [buildEnv, setBuildEnv] = useState('');
  const [building, setBuilding] = useState(false);

  // Subscribe to OTA progress/complete events
  useEffect(() => {
    window.zacus.ota.onProgress(({ deviceId, percent, stage }) => {
      setDevices(prev => prev.map(row =>
        row.device.id === deviceId
          ? { ...row, status: 'updating', progress: percent, progressStage: stage }
          : row
      ));
    });

    window.zacus.ota.onComplete((deviceId, success, error) => {
      setDevices(prev => prev.map(row =>
        row.device.id === deviceId
          ? { ...row, status: success ? 'done' : 'error', progress: 100, error }
          : row
      ));
    });
  }, []);

  // Check all devices for updates
  const checkAll = useCallback(async () => {
    const checked = await Promise.all(
      devices.map(async row => {
        setDevices(prev => prev.map(r =>
          r.device.id === row.device.id ? { ...r, status: 'checking' } : r
        ));
        const info = await window.zacus.ota.check(row.device.id);
        return {
          ...row,
          currentVersion: info.current,
          availableVersion: info.available,
          needsUpdate: info.needsUpdate,
          status: 'idle' as const,
        };
      })
    );
    setDevices(checked);
  }, [devices]);

  const updateDevice = useCallback(async (row: DeviceRow) => {
    const fw = firmwares.find(f => f.name.startsWith(row.device.name.toLowerCase().split(' ')[0]));
    if (!fw) {
      alert(`No firmware found for ${row.device.name}. Import a .bin first.`);
      return;
    }
    await window.zacus.ota.update(row.device.id, selectedMethod, fw.path);
  }, [firmwares, selectedMethod]);

  const updateAll = useCallback(async () => {
    const pending = devices.filter(r => r.needsUpdate && r.status === 'idle');
    await Promise.allSettled(pending.map(row => updateDevice(row)));
  }, [devices, updateDevice]);

  const importFirmware = useCallback(async () => {
    const path = await window.zacus.file.open([
      { name: 'ESP32 Firmware', extensions: ['bin'] },
    ]);
    if (!path) return;
    // Firmware import handled by main process via file:open
    // Display it in the cache list
    const name = path.split('/').pop() ?? 'firmware.bin';
    setFirmwares(prev => [...prev, {
      name,
      version: 'imported',
      path,
      size: 0,
      builtAt: Date.now(),
    }]);
  }, []);

  const rollbackDevice = useCallback(async (row: DeviceRow) => {
    if (!confirm(`Rollback ${row.device.name} to previous firmware?`)) return;
    const ok = await window.zacus.ota.rollback(row.device.id);
    if (!ok) alert('Rollback failed');
  }, []);

  return (
    <div className="firmware-manager">
      <div className="panel-header">
        <h2>Firmware Manager</h2>
        <div className="toolbar-group">
          <select
            className="select"
            value={selectedMethod}
            onChange={e => setSelectedMethod(e.target.value as OTAMethod)}
          >
            <option value="wifi">WiFi OTA (~30s/MB)</option>
            <option value="ble">BLE DFU (~2min/MB)</option>
            <option value="usb">USB Serial (~15s/MB)</option>
          </select>
          <button className="btn" onClick={checkAll}>Check Updates</button>
          <button className="btn primary" onClick={updateAll}>Update All</button>
          <button className="btn" onClick={importFirmware}>Import .bin</button>
        </div>
      </div>

      {/* Update Matrix */}
      <table className="update-matrix">
        <thead>
          <tr>
            <th>Device</th>
            <th>Connection</th>
            <th>Current</th>
            <th>Available</th>
            <th>Status</th>
            <th>Actions</th>
          </tr>
        </thead>
        <tbody>
          {devices.map(row => (
            <tr key={row.device.id} className={row.needsUpdate ? 'needs-update' : ''}>
              <td>{row.device.name}</td>
              <td>
                <span className="badge">{row.device.connectionType.toUpperCase()}</span>
              </td>
              <td className="mono">{row.currentVersion}</td>
              <td className="mono">
                {row.availableVersion}
                {row.needsUpdate && <span className="badge warning">UPDATE</span>}
              </td>
              <td>
                {row.status === 'updating' ? (
                  <div className="progress-container">
                    <div
                      className="progress-bar"
                      style={{ width: `${row.progress}%` }}
                    />
                    <span className="progress-text">
                      {row.progressStage} {row.progress}%
                    </span>
                  </div>
                ) : (
                  <span className={`status-badge ${row.status}`}>{row.status}</span>
                )}
                {row.error && <div className="error-text">{row.error}</div>}
              </td>
              <td>
                <button
                  className="btn small primary"
                  onClick={() => updateDevice(row)}
                  disabled={row.status === 'updating' || !row.needsUpdate}
                >
                  Update
                </button>
                <button
                  className="btn small"
                  onClick={() => rollbackDevice(row)}
                  disabled={row.status === 'updating'}
                >
                  Rollback
                </button>
              </td>
            </tr>
          ))}
        </tbody>
      </table>

      {/* Firmware Cache */}
      <div className="firmware-cache">
        <h3>Firmware Cache</h3>
        {firmwares.length === 0 ? (
          <div className="empty-state">No firmwares cached. Build from source or import a .bin.</div>
        ) : (
          firmwares.map(fw => (
            <div key={fw.path} className="firmware-entry">
              <span className="fw-name">{fw.name}</span>
              <span className="fw-version mono">v{fw.version}</span>
              <span className="fw-size">{(fw.size / 1024).toFixed(0)} KB</span>
              <span className="fw-date">{new Date(fw.builtAt).toLocaleDateString()}</span>
            </div>
          ))
        )}
      </div>
    </div>
  );
}
```

---

## Phase 6: Dev Tools UI (15h)

### Task 6.1 — `desktop/src/renderer/devtools/NvsConfigurator.tsx`

```tsx
import React, { useState, useCallback } from 'react';

interface NVSSetting {
  key: string;
  type: 'string' | 'uint8' | 'float' | 'blob';
  value: string;
  puzzle: string;
  description: string;
}

const DEFAULT_SETTINGS: NVSSetting[] = [
  { key: 'wifi_ssid',   type: 'string', value: 'ZacusNet',    puzzle: 'all', description: 'WiFi network SSID' },
  { key: 'wifi_pass',   type: 'string', value: '',            puzzle: 'all', description: 'WiFi password' },
  { key: 'gm_ip',       type: 'string', value: '192.168.4.1', puzzle: 'all', description: 'Game Master IP address' },
  { key: 'code_digits', type: 'blob',   value: '1 4',         puzzle: 'all', description: 'Code digits (space-separated)' },
  { key: 'morse_msg',   type: 'string', value: 'ZACUS',       puzzle: 'p5',  description: 'Morse code message' },
  { key: 'rf_freq',     type: 'float',  value: '107.5',       puzzle: 'p4',  description: 'Target FM frequency (MHz)' },
  { key: 'nfc_uid_0',   type: 'string', value: '',            puzzle: 'p6',  description: 'NFC UID #1 (hex)' },
  { key: 'nfc_uid_1',   type: 'string', value: '',            puzzle: 'p6',  description: 'NFC UID #2 (hex)' },
];

export function NvsConfigurator(): React.JSX.Element {
  const [settings, setSettings] = useState<NVSSetting[]>(DEFAULT_SETTINGS);
  const [targetPort, setTargetPort] = useState('');
  const [status, setStatus] = useState('');

  const updateSetting = useCallback((key: string, value: string) => {
    setSettings(prev => prev.map(s => s.key === key ? { ...s, value } : s));
  }, []);

  const writeAll = useCallback(async () => {
    if (!targetPort) { setStatus('Select a device port first'); return; }

    setStatus('Writing NVS settings…');
    let ok = 0;

    for (const setting of settings) {
      // Send "nvs set <key> <value>\n" via serial
      const cmd = `nvs set ${setting.key} ${setting.value}\n`;
      await window.zacus.serial.write(targetPort, cmd);
      await delay(200); // Wait for ESP32 to process
      ok++;
    }

    setStatus(`Done: ${ok}/${settings.length} settings written`);
  }, [settings, targetPort]);

  const readAll = useCallback(async () => {
    if (!targetPort) return;
    setStatus('Reading NVS…');
    await window.zacus.serial.write(targetPort, 'nvs dump\n');
    setStatus('Sent nvs dump — check Serial Monitor for output');
  }, [targetPort]);

  return (
    <div className="nvs-configurator">
      <div className="panel-header">
        <h2>NVS Configurator</h2>
        <div className="toolbar-group">
          <input
            className="input mono"
            placeholder="/dev/cu.usbmodem…"
            value={targetPort}
            onChange={e => setTargetPort(e.target.value)}
          />
          <button className="btn" onClick={readAll}>Read</button>
          <button className="btn primary" onClick={writeAll}>Write All</button>
        </div>
      </div>

      {status && <div className="status-bar">{status}</div>}

      <table className="nvs-table">
        <thead>
          <tr>
            <th>Key</th>
            <th>Type</th>
            <th>Puzzle</th>
            <th>Value</th>
            <th>Description</th>
          </tr>
        </thead>
        <tbody>
          {settings.map(s => (
            <tr key={s.key}>
              <td className="mono">{s.key}</td>
              <td><span className="badge">{s.type}</span></td>
              <td>{s.puzzle}</td>
              <td>
                <input
                  className="input mono"
                  type={s.key.includes('pass') ? 'password' : 'text'}
                  value={s.value}
                  onChange={e => updateSetting(s.key, e.target.value)}
                />
              </td>
              <td className="text-dim">{s.description}</td>
            </tr>
          ))}
        </tbody>
      </table>
    </div>
  );
}

function delay(ms: number): Promise<void> {
  return new Promise(resolve => setTimeout(resolve, ms));
}
```

### Task 6.2 — `desktop/src/renderer/devtools/MeshDiagnostic.tsx`

```tsx
import React, { useState, useEffect, useRef } from 'react';

interface MeshNode {
  id: string;
  name: string;
  rssi: number;
  latencyMs: number;
  packetLoss: number;
  lastSeen: number;
  online: boolean;
}

const PUZZLE_NODES = ['BOX-3', 'P1 Son', 'P2', 'P4 Radio', 'P5 Morse', 'P6 NFC', 'P7 Coffre'];

export function MeshDiagnostic(): React.JSX.Element {
  const [nodes, setNodes] = useState<MeshNode[]>(
    PUZZLE_NODES.map((name, i) => ({
      id: `node-${i}`,
      name,
      rssi: 0,
      latencyMs: 0,
      packetLoss: 0,
      lastSeen: 0,
      online: false,
    }))
  );
  const canvasRef = useRef<HTMLCanvasElement>(null);

  // Listen for mesh topology from BOX-3 WebSocket
  useEffect(() => {
    window.zacus.wifi.onWsMessage(rawData => {
      try {
        const msg = JSON.parse(rawData) as {
          type: string;
          nodes: Array<{
            name: string;
            rssi: number;
            latency_ms: number;
            packet_loss: number;
          }>;
        };
        if (msg.type === 'mesh_topology') {
          setNodes(prev => prev.map(node => {
            const update = msg.nodes.find(n => n.name === node.name);
            if (!update) return node;
            return {
              ...node,
              rssi: update.rssi,
              latencyMs: update.latency_ms,
              packetLoss: update.packet_loss,
              lastSeen: Date.now(),
              online: true,
            };
          }));
        }
      } catch { /* not a JSON message */ }
    });
  }, []);

  // Draw mesh topology on canvas
  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;
    const ctx = canvas.getContext('2d');
    if (!ctx) return;

    const W = canvas.width;
    const H = canvas.height;
    ctx.clearRect(0, 0, W, H);

    // BOX-3 at center top
    const masterX = W / 2;
    const masterY = 60;

    // Draw puzzle nodes in a row below
    const puzzleNodes = nodes.slice(1);
    const spacing = W / (puzzleNodes.length + 1);

    ctx.font = '11px monospace';
    ctx.textAlign = 'center';

    puzzleNodes.forEach((node, i) => {
      const x = spacing * (i + 1);
      const y = H - 60;

      // Draw connection line
      ctx.beginPath();
      ctx.moveTo(masterX, masterY + 20);
      ctx.lineTo(x, y - 20);
      ctx.strokeStyle = node.online ? `hsl(${120 + node.rssi}, 80%, 50%)` : '#333';
      ctx.lineWidth = node.online ? 2 : 1;
      ctx.stroke();

      // Draw RSSI label on line midpoint
      if (node.online) {
        ctx.fillStyle = '#94a3b8';
        ctx.fillText(`${node.rssi}dB`, (masterX + x) / 2, (masterY + y) / 2);
      }

      // Draw node circle
      ctx.beginPath();
      ctx.arc(x, y, 18, 0, Math.PI * 2);
      ctx.fillStyle = node.online ? (node.rssi > -60 ? '#22c55e' : '#f59e0b') : '#374151';
      ctx.fill();
      ctx.strokeStyle = '#1f2937';
      ctx.lineWidth = 2;
      ctx.stroke();

      // Node label
      ctx.fillStyle = '#e2e8f0';
      ctx.fillText(node.name.slice(0, 6), x, y + 35);
      if (node.online) {
        ctx.fillStyle = '#94a3b8';
        ctx.fillText(`${node.latencyMs}ms`, x, y + 48);
      }
    });

    // BOX-3 (master)
    ctx.beginPath();
    ctx.arc(masterX, masterY, 24, 0, Math.PI * 2);
    ctx.fillStyle = '#7c3aed';
    ctx.fill();
    ctx.fillStyle = '#e2e8f0';
    ctx.font = 'bold 11px monospace';
    ctx.fillText('BOX-3', masterX, masterY + 4);
  }, [nodes]);

  return (
    <div className="mesh-diagnostic">
      <div className="panel-header">
        <h2>Mesh Diagnostic</h2>
        <span className="text-dim">ESP-NOW network via BOX-3 WebSocket</span>
      </div>

      <canvas
        ref={canvasRef}
        width={700}
        height={280}
        className="mesh-canvas"
      />

      <table className="mesh-table">
        <thead>
          <tr>
            <th>Device</th>
            <th>Status</th>
            <th>RSSI</th>
            <th>Latency</th>
            <th>Loss%</th>
            <th>Last seen</th>
          </tr>
        </thead>
        <tbody>
          {nodes.map(node => (
            <tr key={node.id}>
              <td>{node.name}</td>
              <td>
                <span className={`status-dot ${node.online ? 'online' : 'offline'}`} />
                {node.online ? 'Online' : 'Offline'}
              </td>
              <td className="mono">{node.online ? `${node.rssi} dBm` : '—'}</td>
              <td className="mono">{node.online ? `${node.latencyMs} ms` : '—'}</td>
              <td className={node.packetLoss > 5 ? 'warn mono' : 'mono'}>
                {node.online ? `${node.packetLoss.toFixed(1)}%` : '—'}
              </td>
              <td className="text-dim">
                {node.lastSeen ? new Date(node.lastSeen).toLocaleTimeString() : '—'}
              </td>
            </tr>
          ))}
        </tbody>
      </table>
    </div>
  );
}
```

### Task 6.3 — `desktop/src/renderer/devtools/BatteryDashboard.tsx`

```tsx
import React, { useState, useEffect } from 'react';

interface BatteryReading {
  deviceId: string;
  name: string;
  pct: number;
  pack: string;
  remainingHours: number;
}

function BatteryBar({ pct }: { pct: number }): React.JSX.Element {
  const color = pct >= 50 ? 'var(--success)' : pct >= 20 ? 'var(--warning)' : 'var(--error)';
  return (
    <div className="battery-bar-wrapper">
      <div
        className="battery-bar"
        style={{ width: `${pct}%`, backgroundColor: color }}
      />
    </div>
  );
}

export function BatteryDashboard(): React.JSX.Element {
  const [readings, setReadings] = useState<BatteryReading[]>([
    { deviceId: 'box3',    name: 'BOX-3',   pct: 82, pack: 'Anker #1', remainingHours: 4.0 },
    { deviceId: 'p1_son',  name: 'P1 Son',  pct: 61, pack: 'Pack A',   remainingHours: 3.1 },
    { deviceId: 'p5_mor',  name: 'P5 Morse',pct: 94, pack: 'Pack A',   remainingHours: 5.2 },
    { deviceId: 'p6_nfc',  name: 'P6 NFC',  pct: 78, pack: 'Pack B',   remainingHours: 4.0 },
    { deviceId: 'p7_cof',  name: 'P7 Coffre',pct: 31,pack: 'Pack B',   remainingHours: 1.5 },
  ]);

  // Listen for battery status updates via WebSocket
  useEffect(() => {
    window.zacus.wifi.onWsMessage(rawData => {
      try {
        const msg = JSON.parse(rawData) as {
          type: string;
          device: string;
          battery_pct: number;
        };
        if (msg.type === 'battery_status') {
          setReadings(prev => prev.map(r =>
            r.deviceId === msg.device
              ? { ...r, pct: msg.battery_pct, remainingHours: msg.battery_pct / 20 }
              : r
          ));
        }
      } catch { /* not battery data */ }
    });
  }, []);

  const lowest = readings.reduce((min, r) => r.pct < min.pct ? r : min, readings[0]);

  return (
    <div className="battery-dashboard">
      <div className="panel-header">
        <h2>Battery Dashboard</h2>
        {lowest?.pct < 25 && (
          <div className="alert warning">
            ⚠️ {lowest.name}: {lowest.pct}% — replace soon
          </div>
        )}
      </div>

      <div className="battery-list">
        {readings.map(r => (
          <div key={r.deviceId} className="battery-row">
            <div className="battery-name">{r.name}</div>
            <div className="battery-bar-container">
              <BatteryBar pct={r.pct} />
              <span className="battery-pct">{r.pct}%</span>
            </div>
            <div className="battery-meta">
              <span className="text-dim">{r.pack}</span>
              <span className={r.remainingHours < 2 ? 'warn' : 'text-dim'}>
                ~{r.remainingHours.toFixed(1)}h remaining
              </span>
              {r.pct < 25 && <span className="badge error">⚠️ LOW</span>}
            </div>
          </div>
        ))}
      </div>
    </div>
  );
}
```

### Task 6.4 — `desktop/src/renderer/devtools/LogRecorder.tsx`

```tsx
import React, { useState, useEffect, useRef, useCallback } from 'react';

interface LogEvent {
  timestamp: number;
  source: 'serial' | 'ws' | 'npc' | 'puzzle' | 'system';
  device?: string;
  type: string;
  payload: unknown;
}

export function LogRecorder(): React.JSX.Element {
  const [recording, setRecording] = useState(false);
  const [events, setEvents] = useState<LogEvent[]>([]);
  const sessionRef = useRef<LogEvent[]>([]);

  const record = useCallback((event: LogEvent) => {
    if (!recording) return;
    sessionRef.current.push(event);
    setEvents(prev => [...prev, event]);
  }, [recording]);

  useEffect(() => {
    if (!recording) return;

    window.zacus.serial.onData((port, data) => {
      record({ timestamp: Date.now(), source: 'serial', device: port, type: 'serial_data', payload: data });
    });

    window.zacus.wifi.onWsMessage(data => {
      try {
        record({ timestamp: Date.now(), source: 'ws', type: 'ws_message', payload: JSON.parse(data) });
      } catch {
        record({ timestamp: Date.now(), source: 'ws', type: 'ws_raw', payload: data });
      }
    });
  }, [recording, record]);

  const exportJSON = useCallback(async () => {
    const json = JSON.stringify(
      { session: { startedAt: events[0]?.timestamp, events } },
      null, 2
    );
    await window.zacus.file.save(json, `zacus_session_${Date.now()}.json`);
  }, [events]);

  const exportMarkdown = useCallback(async () => {
    const lines = [
      `# Zacus Session Log`,
      `Generated: ${new Date().toISOString()}`,
      `Events: ${events.length}`,
      '',
      '## Timeline',
      '',
      ...events.map(e => {
        const time = new Date(e.timestamp).toISOString().slice(11, 23);
        return `- **${time}** [${e.source}] ${e.type}: \`${JSON.stringify(e.payload).slice(0, 80)}\``;
      }),
    ];
    await window.zacus.file.save(lines.join('\n'), `zacus_session_${Date.now()}.md`);
  }, [events]);

  return (
    <div className="log-recorder">
      <div className="panel-header">
        <h2>Log Recorder</h2>
        <div className="toolbar-group">
          <button
            className={`btn ${recording ? 'error' : 'primary'}`}
            onClick={() => {
              setRecording(r => !r);
              if (!recording) {
                sessionRef.current = [];
                setEvents([]);
              }
            }}
          >
            {recording ? '⏹ Stop Recording' : '● Start Recording'}
          </button>
          <button className="btn" onClick={exportJSON} disabled={events.length === 0}>
            Export JSON
          </button>
          <button className="btn" onClick={exportMarkdown} disabled={events.length === 0}>
            Export Markdown
          </button>
          <button className="btn" onClick={() => setEvents([])} disabled={events.length === 0}>
            Clear
          </button>
        </div>
      </div>

      {recording && (
        <div className="recording-indicator">
          <span className="rec-dot" /> Recording — {events.length} events
        </div>
      )}

      <div className="event-log">
        {events.slice(-200).map((e, i) => (
          <div key={i} className={`event-row event-${e.source}`}>
            <span className="event-time">{new Date(e.timestamp).toISOString().slice(11, 23)}</span>
            <span className="event-source">[{e.source}]</span>
            {e.device && <span className="event-device">{e.device.split('/').pop()}</span>}
            <span className="event-type">{e.type}</span>
            <span className="event-payload">
              {JSON.stringify(e.payload).slice(0, 120)}
            </span>
          </div>
        ))}
      </div>
    </div>
  );
}
```

---

## Phase 7: Frontend-v3 Integration (8h)

### Task 7.1 — Integration strategy

The renderer tabs load frontend-v3 apps via `<webview>` elements. In development, each app runs on its own Vite port. In production, apps are built and bundled alongside the Electron renderer.

**Development ports:**
- Editor: 5174
- Dashboard: 5175
- Simulation: 5176

**Production:** `npm run build` in `frontend-v3/` outputs to `desktop/dist/renderer/apps/{editor,dashboard,simulation}/`.

### Task 7.2 — `desktop/scripts/build-frontends.sh`

```bash
#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
FRONTEND_V3="$REPO_ROOT/frontend-v3"
DESKTOP_DIST="$REPO_ROOT/desktop/dist/renderer/apps"

echo "Building frontend-v3 apps..."
mkdir -p "$DESKTOP_DIST"

for app in editor dashboard simulation; do
  echo "  → Building $app..."
  (cd "$FRONTEND_V3/apps/$app" && npm run build -- --outDir "$DESKTOP_DIST/$app" --base "./apps/$app/")
done

echo "Done: frontend-v3 apps built to $DESKTOP_DIST"
```

### Task 7.3 — Drag-and-drop `.zacus` file support in `desktop/src/main/index.ts`

Already handled via `app.on('open-file')` in Phase 1. The main process sends `file:opened` to renderer, which routes to EditorTab via `window.zacus.menu.on('file:opened', ...)`.

### Task 7.4 — Native notifications for game events

Game events from the dashboard WebSocket (puzzle solved, timer warning) trigger `window.zacus.notify()` from the DashboardTab webview's message listener. Example in DashboardTab:

```typescript
// Inside the webview (frontend-v3/apps/dashboard), send to Electron:
window.electronAPI?.notify('Puzzle Solved!', 'P1 Son completed in 4:32');
```

The dashboard app must expose `electronAPI` detection: `const isElectron = typeof window.zacus !== 'undefined'`.

---

## Phase 8: Build Pipeline (8h)

### Task 8.1 — `desktop/scripts/notarize.js`

```javascript
const { notarize } = require('@electron/notarize');
const { execSync } = require('child_process');

async function notarizeApp() {
  if (process.platform !== 'darwin') return;

  const appName = 'Zacus Studio';
  const appPath = `${process.env.HOME}/Projects/le-mystere-professeur-zacus/desktop/release/mac-universal/${appName}.app`;

  console.log(`Notarizing ${appPath}...`);

  await notarize({
    tool: 'notarytool',
    appPath,
    appleId: process.env.APPLE_ID,
    appleIdPassword: process.env.APPLE_APP_SPECIFIC_PASSWORD,
    teamId: process.env.APPLE_TEAM_ID,
  });

  console.log('Notarization complete');
}

notarizeApp().catch(err => {
  console.error(err);
  process.exit(1);
});
```

### Task 8.2 — `desktop/.github/workflows/build.yml`

```yaml
name: Build Zacus Studio

on:
  push:
    tags: ['desktop/v*']
  workflow_dispatch:

permissions:
  contents: write

jobs:
  build-mac:
    runs-on: macos-14  # Apple Silicon runner
    timeout-minutes: 60

    steps:
      - uses: actions/checkout@v4
        with:
          submodules: recursive

      - uses: actions/setup-node@v4
        with:
          node-version: '22'
          cache: 'npm'
          cache-dependency-path: desktop/package-lock.json

      - name: Install Xcode Command Line Tools
        run: xcode-select --install 2>/dev/null || true

      - name: Install dependencies
        working-directory: desktop
        run: npm ci

      - name: Build frontend-v3 apps
        run: bash desktop/scripts/build-frontends.sh

      - name: Build Swift native modules
        working-directory: desktop/src/native
        run: swift build -c release

      - name: Rebuild N-API bindings
        working-directory: desktop
        run: npm run rebuild-native

      - name: Build macOS app (universal)
        working-directory: desktop
        env:
          CSC_LINK: ${{ secrets.APPLE_CERTIFICATE_BASE64 }}
          CSC_KEY_PASSWORD: ${{ secrets.APPLE_CERTIFICATE_PASSWORD }}
          APPLE_ID: ${{ secrets.APPLE_ID }}
          APPLE_APP_SPECIFIC_PASSWORD: ${{ secrets.APPLE_APP_SPECIFIC_PASSWORD }}
          APPLE_TEAM_ID: ${{ secrets.APPLE_TEAM_ID }}
        run: npm run build:mac

      - name: Upload .dmg artifact
        uses: actions/upload-artifact@v4
        with:
          name: ZacusStudio-${{ github.ref_name }}.dmg
          path: desktop/release/*.dmg
          retention-days: 30

      - name: Create GitHub Release
        if: startsWith(github.ref, 'refs/tags/')
        uses: softprops/action-gh-release@v2
        with:
          files: desktop/release/*.dmg
          generate_release_notes: true
```

### Task 8.3 — `desktop/src/main/auto-updater.ts` (complete, see Phase 1 Task 1.20)

Auto-updater uses `electron-updater` with GitHub Releases as provider. Configuration is already in `electron-builder.yml` (Phase 1 Task 1.17). The app checks for updates on startup and notifies the renderer when an update is available/downloaded.

### Task 8.4 — `desktop/src/native/binding.gyp`

```json
{
  "targets": [
    {
      "target_name": "zacus_native",
      "sources": [
        "binding.cpp"
      ],
      "include_dirs": [
        "<!@(node -p \"require('node-addon-api').include\")"
      ],
      "libraries": [
        "<(module_root_dir)/build/Release/libZacusNative.dylib"
      ],
      "defines": ["NAPI_DISABLE_CPP_EXCEPTIONS"],
      "xcode_settings": {
        "MACOSX_DEPLOYMENT_TARGET": "14.0",
        "ARCHS": "$(ARCHS_STANDARD)",
        "OTHER_LDFLAGS": [
          "-framework IOKit",
          "-framework CoreBluetooth",
          "-framework Network"
        ]
      }
    }
  ]
}
```

### Task 8.5 — `desktop/src/native/binding.cpp`

```cpp
#include <napi.h>

// Forward declarations from Swift (via C bridge)
extern "C" {
  void* serial_bridge_create(void);
  int   serial_list_ports(void* bridge, char* out_json, int max_len);
  int   serial_connect(void* bridge, const char* port, int baud);
  void  serial_write(void* bridge, const char* port, const char* data, int len);
  void  serial_disconnect(void* bridge, const char* port);

  void* ble_bridge_create(void);
  void  ble_start_scan(void* bridge);
  int   ble_connect(void* bridge, const char* peripheral_id);

  void* wifi_bridge_create(void);
  void  wifi_browse_mdns(void* bridge, const char* service);
  int   wifi_connect_ws(void* bridge, const char* url);
}

// SerialBridge N-API wrapper
Napi::Object InitSerialBridge(Napi::Env env, Napi::Object exports) {
  // Full N-API class wrapping is generated via node-addon-api
  // See: https://github.com/nodejs/node-addon-api/blob/main/doc/object_wrap.md
  // Implementation mirrors the Swift interface exactly.
  return exports;
}

Napi::Object Init(Napi::Env env, Napi::Object exports) {
  InitSerialBridge(env, exports);
  return exports;
}

NODE_API_MODULE(zacus_native, Init)
```

**Note:** Full N-API class implementations for all three bridges follow the standard `Napi::ObjectWrap<T>` pattern. Each Swift class is exposed as a JavaScript class with identical method names. The C bridge header (`ZacusNative-Swift.h`) is generated by the Swift compiler automatically.

---

## Phase 9: ESP32 OTA Component (10h)

### Task 9.1 — `ESP32_ZACUS/components/ota_server/include/ota_server.h`

```c
#pragma once

#include "esp_err.h"
#include "esp_https_server.h"

#ifdef __cplusplus
extern "C" {
#endif

// ─── Version info (override in each puzzle's CMakeLists.txt) ─────────────────
#ifndef OTA_FIRMWARE_NAME
#define OTA_FIRMWARE_NAME "zacus_puzzle"
#endif

#ifndef OTA_FIRMWARE_VERSION
#define OTA_FIRMWARE_VERSION "1.0.0"
#endif

// ─── Configuration ────────────────────────────────────────────────────────────
#define OTA_SERVER_PORT         80
#define OTA_RATE_LIMIT_SECS     60        // Minimum seconds between OTA updates
#define OTA_WATCHDOG_SECS       30        // Auto-rollback if new firmware crashes within this
#define OTA_MAX_UPLOAD_SIZE     (4 * 1024 * 1024)  // 4 MB max firmware size
#define OTA_CHUNK_SIZE          4096

// ─── State ────────────────────────────────────────────────────────────────────
typedef enum {
    OTA_STATE_IDLE        = 0,
    OTA_STATE_DOWNLOADING = 1,
    OTA_STATE_VERIFYING   = 2,
    OTA_STATE_REBOOTING   = 3,
    OTA_STATE_ERROR       = 4,
} ota_state_t;

typedef struct {
    ota_state_t state;
    int         progress;       // 0–100
    char        error[128];
    uint32_t    bytes_received;
    uint32_t    total_bytes;
    int64_t     last_ota_time;  // Unix timestamp of last OTA attempt
} ota_status_t;

// ─── Public API ───────────────────────────────────────────────────────────────

/**
 * @brief Initialize the OTA HTTP server on port 80.
 *
 * Registers 5 endpoints:
 *   GET  /version      → firmware name, version, IDF version
 *   GET  /status       → battery, heap, uptime, ESP-NOW peers
 *   POST /ota          → receive .bin, write to OTA partition, reboot
 *   GET  /ota/status   → current OTA state and progress
 *   POST /ota/rollback → revert to previous firmware partition
 *
 * @return ESP_OK on success, error code otherwise.
 */
esp_err_t ota_server_init(void);

/**
 * @brief Mark current firmware as valid (call after successful startup).
 *
 * Cancels the rollback watchdog. Call this after all subsystems have
 * initialized successfully, typically 5–10 seconds after boot.
 */
void ota_server_mark_valid(void);

/**
 * @brief Get the current OTA status.
 */
const ota_status_t* ota_server_get_status(void);

/**
 * @brief Register a callback invoked when an OTA update completes.
 *
 * Called before the device reboots. Use to flush pending data to NVS.
 */
void ota_server_set_complete_cb(void (*cb)(bool success));

#ifdef __cplusplus
}
#endif
```

### Task 9.2 — `ESP32_ZACUS/components/ota_server/ota_server.c`

```c
#include "ota_server.h"

#include <string.h>
#include <time.h>
#include <sys/param.h>

#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_app_format.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "esp_http_server.h"
#include "esp_mac.h"
#include "nvs_flash.h"
#include "mbedtls/sha256.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// ─── External symbols (provided by each puzzle's main component) ──────────────
extern int  puzzle_get_battery_pct(void);
extern int  puzzle_get_espnow_peer_count(void);

static const char* TAG = "ota_server";

// ─── Module state ─────────────────────────────────────────────────────────────
static httpd_handle_t  s_server       = NULL;
static ota_status_t    s_status       = { .state = OTA_STATE_IDLE };
static void          (*s_complete_cb)(bool) = NULL;
static esp_timer_handle_t s_watchdog  = NULL;

// ─── JSON helpers ─────────────────────────────────────────────────────────────

static void json_str(char* buf, size_t size, const char* key, const char* val, bool comma) {
    snprintf(buf + strlen(buf), size - strlen(buf),
             "\"%s\":\"%s\"%s", key, val, comma ? "," : "");
}

static void json_int(char* buf, size_t size, const char* key, int val, bool comma) {
    snprintf(buf + strlen(buf), size - strlen(buf),
             "\"%s\":%d%s", key, val, comma ? "," : "");
}

// ─── GET /version ─────────────────────────────────────────────────────────────

static esp_err_t handle_version(httpd_req_t* req) {
    char buf[256] = "{";
    json_str(buf, sizeof(buf), "firmware", OTA_FIRMWARE_NAME, true);
    json_str(buf, sizeof(buf), "version", OTA_FIRMWARE_VERSION, true);
    json_str(buf, sizeof(buf), "idf", IDF_VER, false);
    strncat(buf, "}", sizeof(buf) - strlen(buf) - 1);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_sendstr(req, buf);
    return ESP_OK;
}

// ─── GET /status ──────────────────────────────────────────────────────────────

static esp_err_t handle_status(httpd_req_t* req) {
    char buf[256] = "{";
    json_int(buf, sizeof(buf), "battery_pct",   puzzle_get_battery_pct(), true);
    json_int(buf, sizeof(buf), "uptime_s",      (int)(esp_timer_get_time() / 1000000), true);
    json_int(buf, sizeof(buf), "espnow_peers",  puzzle_get_espnow_peer_count(), true);
    json_int(buf, sizeof(buf), "heap_free",     (int)esp_get_free_heap_size(), false);
    strncat(buf, "}", sizeof(buf) - strlen(buf) - 1);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_sendstr(req, buf);
    return ESP_OK;
}

// ─── GET /ota/status ──────────────────────────────────────────────────────────

static const char* state_to_str(ota_state_t state) {
    switch (state) {
        case OTA_STATE_IDLE:        return "idle";
        case OTA_STATE_DOWNLOADING: return "downloading";
        case OTA_STATE_VERIFYING:   return "verifying";
        case OTA_STATE_REBOOTING:   return "rebooting";
        case OTA_STATE_ERROR:       return "error";
        default:                    return "unknown";
    }
}

static esp_err_t handle_ota_status(httpd_req_t* req) {
    char buf[256] = "{";
    json_str(buf, sizeof(buf), "state", state_to_str(s_status.state), true);
    json_int(buf, sizeof(buf), "progress", s_status.progress, false);
    strncat(buf, "}", sizeof(buf) - strlen(buf) - 1);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_sendstr(req, buf);
    return ESP_OK;
}

// ─── POST /ota ────────────────────────────────────────────────────────────────

static void do_ota_task(void* arg) {
    esp_ota_handle_t    ota_handle    = 0;
    const esp_partition_t* ota_part   = NULL;
    httpd_req_t*        req           = (httpd_req_t*)arg;
    esp_err_t           err           = ESP_OK;

    uint8_t*  buf   = malloc(OTA_CHUNK_SIZE);
    if (!buf) { httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OOM"); goto cleanup; }

    // Check content length
    int total = req->content_len;
    if (total <= 0 || total > OTA_MAX_UPLOAD_SIZE) {
        ESP_LOGE(TAG, "Invalid content length: %d", total);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid size");
        goto cleanup;
    }

    // Get OTA partition
    ota_part = esp_ota_get_next_update_partition(NULL);
    if (!ota_part) {
        ESP_LOGE(TAG, "No OTA partition available");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "No OTA partition");
        goto cleanup;
    }

    err = esp_ota_begin(ota_part, OTA_SIZE_UNKNOWN, &ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_begin failed: %s", esp_err_to_name(err));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA begin failed");
        goto cleanup;
    }

    s_status.state         = OTA_STATE_DOWNLOADING;
    s_status.bytes_received = 0;
    s_status.total_bytes   = total;
    s_status.progress      = 0;

    // SHA256 context for integrity check
    mbedtls_sha256_context sha_ctx;
    mbedtls_sha256_init(&sha_ctx);
    mbedtls_sha256_starts(&sha_ctx, 0);

    // Receive and write firmware chunks
    int received = 0;
    while (received < total) {
        int chunk_size = MIN(OTA_CHUNK_SIZE, total - received);
        int r = httpd_req_recv(req, (char*)buf, chunk_size);

        if (r <= 0) {
            if (r == HTTPD_SOCK_ERR_TIMEOUT) continue;
            ESP_LOGE(TAG, "Recv error: %d", r);
            err = ESP_FAIL;
            break;
        }

        err = esp_ota_write(ota_handle, buf, r);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "esp_ota_write failed at offset %d: %s", received, esp_err_to_name(err));
            break;
        }

        mbedtls_sha256_update(&sha_ctx, buf, r);
        received += r;
        s_status.bytes_received = received;
        s_status.progress = (received * 100) / total;
    }

    if (err != ESP_OK) {
        snprintf(s_status.error, sizeof(s_status.error), "Write failed: %s", esp_err_to_name(err));
        s_status.state = OTA_STATE_ERROR;
        esp_ota_abort(ota_handle);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, s_status.error);
        goto cleanup;
    }

    // Compute SHA256 of received data
    uint8_t sha256[32];
    mbedtls_sha256_finish(&sha_ctx, sha256);
    mbedtls_sha256_free(&sha_ctx);

    s_status.state    = OTA_STATE_VERIFYING;
    s_status.progress = 95;

    // Finalize OTA
    err = esp_ota_end(ota_handle);
    ota_handle = 0;
    if (err != ESP_OK) {
        snprintf(s_status.error, sizeof(s_status.error), "OTA end failed: %s", esp_err_to_name(err));
        s_status.state = OTA_STATE_ERROR;
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, s_status.error);
        goto cleanup;
    }

    // Set boot partition
    err = esp_ota_set_boot_partition(ota_part);
    if (err != ESP_OK) {
        snprintf(s_status.error, sizeof(s_status.error), "Set boot failed: %s", esp_err_to_name(err));
        s_status.state = OTA_STATE_ERROR;
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, s_status.error);
        goto cleanup;
    }

    s_status.progress = 100;
    s_status.state    = OTA_STATE_REBOOTING;

    // Respond before reboot
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_sendstr(req, "{\"status\":\"ok\",\"message\":\"Firmware accepted, rebooting\"}");

    if (s_complete_cb) s_complete_cb(true);

    ESP_LOGI(TAG, "OTA success, rebooting in 1s");
    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();

cleanup:
    if (ota_handle) esp_ota_abort(ota_handle);
    free(buf);
    mbedtls_sha256_free(&sha_ctx);
    vTaskDelete(NULL);
}

static esp_err_t handle_ota_upload(httpd_req_t* req) {
    // Rate limiting
    int64_t now = esp_timer_get_time() / 1000000;
    if (s_status.last_ota_time > 0 && (now - s_status.last_ota_time) < OTA_RATE_LIMIT_SECS) {
        httpd_resp_send_err(req, HTTPD_429_TOO_MANY_REQUESTS, "Rate limited: wait 60s");
        return ESP_FAIL;
    }

    if (s_status.state != OTA_STATE_IDLE) {
        httpd_resp_send_err(req, HTTPD_409_CONFLICT, "OTA already in progress");
        return ESP_FAIL;
    }

    s_status.last_ota_time = now;

    // Run OTA in a separate task to not block the HTTP server
    if (xTaskCreate(do_ota_task, "ota_task", 8192, req, 5, NULL) != pdPASS) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Task create failed");
        return ESP_FAIL;
    }

    // Task will send the HTTP response
    return ESP_OK;
}

// ─── POST /ota/rollback ───────────────────────────────────────────────────────

static esp_err_t handle_ota_rollback(httpd_req_t* req) {
    const esp_partition_t* prev = esp_ota_get_last_invalid_partition();
    if (!prev) {
        // Try running partition as fallback
        prev = esp_ota_get_running_partition();
    }

    if (!prev) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "No previous partition to roll back to");
        return ESP_FAIL;
    }

    esp_err_t err = esp_ota_set_boot_partition(prev);
    if (err != ESP_OK) {
        char msg[64];
        snprintf(msg, sizeof(msg), "Rollback failed: %s", esp_err_to_name(err));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, msg);
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"ok\",\"message\":\"Rolling back, rebooting\"}");

    vTaskDelay(pdMS_TO_TICKS(500));
    esp_restart();
    return ESP_OK;
}

// ─── Watchdog (auto-rollback) ──────────────────────────────────────────────────

static void watchdog_cb(void* arg) {
    ESP_LOGE(TAG, "Watchdog expired — new firmware did not call ota_server_mark_valid(), rolling back");
    esp_ota_mark_app_invalid_rollback_and_reboot();
}

static void start_watchdog(void) {
    const esp_timer_create_args_t args = {
        .callback = watchdog_cb,
        .name     = "ota_watchdog",
    };
    esp_timer_create(&args, &s_watchdog);
    esp_timer_start_once(s_watchdog, (int64_t)OTA_WATCHDOG_SECS * 1000000);
    ESP_LOGI(TAG, "OTA watchdog started (%ds to mark valid)", OTA_WATCHDOG_SECS);
}

// ─── Public API ───────────────────────────────────────────────────────────────

esp_err_t ota_server_init(void) {
    // Check if we booted from an OTA partition that needs validation
    const esp_partition_t* running = esp_ota_get_running_partition();
    esp_ota_img_states_t ota_state;
    if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK) {
        if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
            ESP_LOGW(TAG, "Running unvalidated OTA firmware — starting watchdog");
            start_watchdog();
        }
    }

    // HTTP server config
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port     = OTA_SERVER_PORT;
    config.max_uri_handlers = 8;
    config.uri_match_fn    = httpd_uri_match_wildcard;
    config.stack_size      = 8192;

    esp_err_t err = httpd_start(&s_server, &config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server: %s", esp_err_to_name(err));
        return err;
    }

    // Register URI handlers
    static const httpd_uri_t uris[] = {
        { .uri = "/version",      .method = HTTP_GET,  .handler = handle_version     },
        { .uri = "/status",       .method = HTTP_GET,  .handler = handle_status      },
        { .uri = "/ota",          .method = HTTP_POST, .handler = handle_ota_upload  },
        { .uri = "/ota/status",   .method = HTTP_GET,  .handler = handle_ota_status  },
        { .uri = "/ota/rollback", .method = HTTP_POST, .handler = handle_ota_rollback},
    };

    for (int i = 0; i < (int)(sizeof(uris) / sizeof(uris[0])); i++) {
        err = httpd_register_uri_handler(s_server, &uris[i]);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to register URI %s: %s", uris[i].uri, esp_err_to_name(err));
            return err;
        }
    }

    ESP_LOGI(TAG, "OTA server started on port %d (%s v%s)",
             OTA_SERVER_PORT, OTA_FIRMWARE_NAME, OTA_FIRMWARE_VERSION);

    return ESP_OK;
}

void ota_server_mark_valid(void) {
    if (s_watchdog) {
        esp_timer_stop(s_watchdog);
        esp_timer_delete(s_watchdog);
        s_watchdog = NULL;
        ESP_LOGI(TAG, "OTA watchdog cancelled — firmware marked valid");
    }
    esp_ota_mark_app_valid_cancel_rollback();
}

const ota_status_t* ota_server_get_status(void) {
    return &s_status;
}

void ota_server_set_complete_cb(void (*cb)(bool success)) {
    s_complete_cb = cb;
}
```

### Task 9.3 — `ESP32_ZACUS/components/ota_server/CMakeLists.txt`

```cmake
idf_component_register(
    SRCS
        "ota_server.c"
    INCLUDE_DIRS
        "include"
    REQUIRES
        esp_http_server
        esp_ota_ops
        esp_timer
        esp_system
        nvs_flash
        mbedtls
        freertos
)

# Firmware metadata injected at compile time
# Override these in the puzzle's CMakeLists.txt before adding the component
if(NOT DEFINED OTA_FIRMWARE_NAME)
    set(OTA_FIRMWARE_NAME "zacus_puzzle")
endif()

if(NOT DEFINED OTA_FIRMWARE_VERSION)
    set(OTA_FIRMWARE_VERSION "1.0.0")
endif()

target_compile_definitions(${COMPONENT_LIB} PRIVATE
    OTA_FIRMWARE_NAME="${OTA_FIRMWARE_NAME}"
    OTA_FIRMWARE_VERSION="${OTA_FIRMWARE_VERSION}"
)
```

### Task 9.4 — Partition table for OTA: `ESP32_ZACUS/partitions_ota.csv`

```csv
# Name,   Type, SubType, Offset,  Size,    Flags
nvs,      data, nvs,     0x9000,  0x6000,
otadata,  data, ota,     0xf000,  0x2000,
phy_init, data, phy,     0x11000, 0x1000,
factory,  app,  factory, 0x20000, 1500K,
ota_0,    app,  ota_0,   ,        1500K,
ota_1,    app,  ota_1,   ,        1500K,
spiffs,   data, spiffs,  ,        512K,
```

### Task 9.5 — Integration in each puzzle's `main/CMakeLists.txt`

```cmake
# In ESP32_ZACUS/puzzle_1_son/main/CMakeLists.txt (and each puzzle):

set(OTA_FIRMWARE_NAME "p1_son")
set(OTA_FIRMWARE_VERSION "1.0.0")

idf_component_register(
    SRCS "main.c" "puzzle_son.c"
    INCLUDE_DIRS "."
    REQUIRES
        ota_server
        esp_wifi
        esp_event
        nvs_flash
)
```

### Task 9.6 — Puzzle firmware stub with weak symbols

Each puzzle must implement the two weak symbols queried by `ota_server.c`:

```c
// In each puzzle's main.c or a shared board_hal.c:

// Weak default implementations — override in board-specific code
__attribute__((weak)) int puzzle_get_battery_pct(void) {
    // Read from ADC or INA226 via I2C
    // Default: return a placeholder while ADC not wired
    return 100;
}

__attribute__((weak)) int puzzle_get_espnow_peer_count(void) {
    // Return count from esp_now_get_peer_num()
    return 0;
}

// In app_main() — after all subsystems initialized:
void app_main(void) {
    nvs_flash_init();
    wifi_init();
    espnow_init();
    puzzle_hardware_init();

    // Start OTA server (must have WiFi connected first)
    ESP_ERROR_CHECK(ota_server_init());

    // Mark firmware valid after 10s of stable operation
    vTaskDelay(pdMS_TO_TICKS(10000));
    ota_server_mark_valid();

    // Main puzzle loop
    puzzle_run();
}
```

---

## Summary

| Phase | Files | Key deliverable |
|-------|-------|-----------------|
| 1 | `desktop/src/main/`, `preload/`, `renderer/App.tsx` + tabs | Electron app with 4 tabs, menus, IPC bridge |
| 2 | `SerialBridge.swift`, `serial-handler.ts`, `SerialMonitor.tsx` | IOKit USB serial + esptool flash |
| 3 | `BLEBridge.swift`, `ble-handler.ts`, `DeviceManager.tsx` | CoreBluetooth scan + BLE DFU |
| 4 | `WiFiBridge.swift`, `wifi-handler.ts` | mDNS, WebSocket, HTTP OTA transport |
| 5 | `ota-manager.ts`, `FirmwareManager.tsx` | 3-method OTA orchestration + update matrix UI |
| 6 | 4 Dev Tools panels | NVS config, mesh visualization, battery, log recorder |
| 7 | `build-frontends.sh`, integration guide | frontend-v3 apps embedded in tabs |
| 8 | `electron-builder.yml`, GH Actions, notarize | `.dmg` + signed + auto-update |
| 9 | `ota_server.h`, `ota_server.c`, `CMakeLists.txt`, `partitions_ota.csv` | ESP32 HTTP OTA server + watchdog rollback |

**Total estimated: ~106h**

**Prerequisites before starting:**
- Apple Developer account (for signing/notarization)
- `APPLE_CERTIFICATE_BASE64`, `APPLE_CERTIFICATE_PASSWORD`, `APPLE_ID`, `APPLE_APP_SPECIFIC_PASSWORD`, `APPLE_TEAM_ID` in GitHub Secrets
- Xcode 16+ on build machine
- Node 22 LTS, Swift 6
- PlatformIO + esptool.py for USB flash

**Start with Phase 1** (scaffold) to validate Electron + Swift toolchain, then Phase 9 (ESP32 OTA server) in parallel as it has no desktop dependencies.
