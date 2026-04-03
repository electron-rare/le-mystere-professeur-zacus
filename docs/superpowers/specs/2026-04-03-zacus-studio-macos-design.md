# Zacus Studio — App macOS — Design Spec

## Summary

Electron + Swift native macOS app. 4 tabs: Blockly editor, Game Master dashboard, 3D simulation, Dev Tools. Embeds frontend-v3 monorepo. Native Swift modules for USB serial (IOKit), BLE (CoreBluetooth), WiFi (Network.framework). OTA firmware updates via WiFi/BLE/USB. The Mac becomes the complete control hub for Zacus escape room kits.

## Architecture

```
Zacus Studio.app (Electron 33+)
├── Renderer (Chromium)
│   ├── Tab: Éditeur       → frontend-v3/apps/editor
│   ├── Tab: Game Master   → frontend-v3/apps/dashboard
│   ├── Tab: Simulation    → frontend-v3/apps/simulation
│   └── Tab: Dev Tools     → new (built-in)
├── Main Process (Node.js)
│   ├── Window mgmt, menus, file dialogs, auto-updater
│   ├── IPC bridge ↔ Swift native modules
│   ├── PlatformIO CLI wrapper (esptool, pio)
│   └── OTA manager (HTTP push to ESP32s)
└── Native Modules (Swift via node-swift / N-API addon)
    ├── SerialBridge   — IOKit / CoreSerial
    ├── BLEBridge      — CoreBluetooth
    └── WiFiBridge     — Network.framework / mDNS
```

## Tab 1: Éditeur (frontend-v3/apps/editor)

Blockly scenario editor — same as web version with native enhancements:

- **File menu**: ⌘S save, ⌘O open `.zacus` YAML files via native dialog
- **Recent files**: macOS recent documents menu
- **Drag & drop**: drop `.yaml` onto app icon or window to open
- **Compile**: ⌘B compiles to Runtime 3 IR
- **Validate**: ⌘R runs 6 validation rules with native notification on errors
- **Export**: ⌘E exports compiled scenario to SD card (mounted USB)

## Tab 2: Game Master (frontend-v3/apps/dashboard)

Real-time dashboard — same as web with native enhancements:

- **Auto-connect**: discovers BOX-3 via WiFiBridge mDNS (no manual IP)
- **BLE fallback**: if WiFi unavailable, connects to BOX-3 via BLE
- **macOS notifications**: native alerts for puzzle solved, timer warnings, NPC events
- **Touch Bar**: (MacBook Pro) Ring Phone, Force Hint, Skip Puzzle buttons
- **Menu bar indicator**: green/orange/red dot showing game status
- **Audio routing**: ambient tracks via AVFoundation (better than Web Audio)

## Tab 3: Simulation (frontend-v3/apps/simulation)

3D digital twin — same as web with native enhancements:

- **Metal acceleration**: GPU via Electron's Metal backend
- **Fullscreen**: ⌘F for immersive demo mode
- **Audio**: AudioCraft tracks via AVFoundation
- **Screen recording**: ⌘R to record demo for commercial use (AVFoundation)

## Tab 4: Dev Tools (new)

### Device Manager

Discovers and manages all ESP32 devices in the kit.

Discovery methods (parallel):
1. USB Serial: scan `/dev/cu.usbmodem*`, `/dev/cu.usbserial*`
2. BLE: scan for service UUID `ZACUS_SVC_UUID`
3. mDNS: browse `_zacus._tcp.local`
4. ESP-NOW status: BOX-3 reports mesh topology via WebSocket

Device card shows: name, type (puzzle/hub), firmware version, battery %, connection type, last seen.

### Serial Monitor

- Multi-device serial console with color-coded output per device
- Filter by device, log level, keyword
- Auto-scroll with pause
- Log export to file
- Regex highlighting (e.g., `ERROR` in red, `SOLVED` in green)

### Firmware Manager + OTA

3 flash methods per device:

| Method | Transport | Speed | Use case |
|--------|-----------|-------|----------|
| **OTA WiFi** | HTTP POST to ESP32 OTA server | ~30s/MB | Kit deployed, no cables |
| **OTA BLE** | BLE DFU (Nordic-style) | ~2min/MB | No WiFi available |
| **USB Serial** | esptool.py via SerialBridge | ~15s/MB | Development, recovery |

#### ESP32 OTA Server (firmware side)

Each puzzle ESP32 runs a minimal HTTP OTA server:

```c
// Endpoints on each puzzle ESP32 (port 80)
GET  /version    → {"firmware": "p1_son", "version": "1.2.0", "idf": "5.4.0"}
GET  /status     → {"battery_pct": 87, "uptime_s": 3600, "espnow_peers": 5, "heap_free": 45000}
POST /ota        → receives .bin, writes to OTA partition, validates, reboots
GET  /ota/status → {"state": "idle|downloading|verifying|rebooting", "progress": 0-100}
POST /ota/rollback → reverts to previous firmware partition
```

#### OTA Flow (Zacus Studio side)

```
1. Discover devices (mDNS/BLE/ESP-NOW)
2. GET /version on each → compare with local firmware builds
3. Show update matrix (device × current × available)
4. User clicks "Update All" or selects specific devices
5. For each device:
   a. POST /ota with .bin body (chunked transfer)
   b. Poll GET /ota/status for progress
   c. Wait for reboot (device disappears from mesh, reappears)
   d. GET /version to confirm new version
6. If any device fails: show error, offer rollback or USB flash
```

#### Firmware Build Integration

```
[Build from source] button:
1. Runs `pio run -e <puzzle_env>` via CLI
2. Collects .bin from `.pio/build/<env>/firmware.bin`
3. Stores in app's firmware cache (~/.zacus-studio/firmwares/)
4. Available for OTA push

[Import .bin] button:
1. Native file dialog to select pre-built .bin
2. Validates: checks ELF header, partition table, app descriptor
3. Adds to firmware cache
```

### NVS Configurator

Configure per-puzzle settings stored in ESP32 NVS (Non-Volatile Storage):

| Setting | Puzzle | Type | Example |
|---------|--------|------|---------|
| WiFi SSID | All | string | "ZacusNet" |
| WiFi Pass | All | string | "zacus2026" |
| NFC UIDs | P6 | string[] | ["04:A2:B1:...", ...] |
| Morse message | P5 | string | "ZACUS" |
| Target frequency | P4 | float | 107.5 |
| Code digits | All | uint8[] | [1, 4] |
| Game master IP | All | string | "192.168.4.1" |

Write via: Serial command (`nvs set <key> <value>`) or BLE characteristic write.

### Mesh Diagnostic

Visual map of the ESP-NOW network:

```
    ┌─────┐
    │BOX-3│ (master)
    └──┬──┘
   ┌───┼───┬───┬───┬───┐
   │   │   │   │   │   │
  P1  P2  P4  P5  P6  P7
  ✅  ✅  ✅  ✅  ✅  ❌
 -40  -55 -48 -62 -51  --  (RSSI dBm)
 2ms  3ms 2ms 4ms 3ms  --  (latency)
```

Shows: RSSI signal strength, round-trip latency, packet loss %, last message time.

### Battery Dashboard

Real-time battery levels for all wireless devices:

```
BOX-3:  ████████░░ 82% (Anker #1, ~4h remaining)
P1 Son: ██████░░░░ 61% (Pack A, ~3h remaining)
P5 Mor: █████████░ 94% (Pack A, ~5h remaining)
P6 NFC: ████████░░ 78% (Pack B, ~4h remaining)
P7 Cof: ███░░░░░░░ 31% ⚠️ (Pack B, ~1.5h remaining)
```

### Log Recorder

Records all events during a session for post-playtest analysis:

- Start/stop recording
- Captures: serial output, ESP-NOW messages, NPC decisions, puzzle events, timestamps
- Exports to: JSON (machine-readable) or Markdown report (human-readable)
- Feeds into `tools/playtest/collect_metrics.py`

## Native Swift Modules

### SerialBridge

```swift
@objc class SerialBridge: NSObject {
    // Discovery
    func listPorts() -> [[String: Any]]  // [{path, name, vendorId, productId}]
    func startWatching()                  // Notify on plug/unplug via IOKit

    // Connection
    func connect(_ port: String, baud: Int) -> Bool
    func disconnect(_ port: String)

    // I/O
    func write(_ port: String, data: Data)
    func onData(_ port: String, callback: @escaping (Data) -> Void)

    // Flash (wraps esptool.py)
    func flash(_ port: String, firmware: URL, 
               onProgress: @escaping (Int) -> Void,
               onComplete: @escaping (Bool, String?) -> Void)
}
```

### BLEBridge

```swift
@objc class BLEBridge: NSObject, CBCentralManagerDelegate, CBPeripheralDelegate {
    // Discovery
    func startScan(serviceUUIDs: [String])
    func stopScan()
    func onDiscovered(_ callback: @escaping ([String: Any]) -> Void)

    // Connection
    func connect(_ peripheralId: String) -> Bool
    func disconnect(_ peripheralId: String)
    func listConnected() -> [[String: Any]]

    // Data
    func write(_ peripheralId: String, characteristic: String, data: Data)
    func subscribe(_ peripheralId: String, characteristic: String,
                   callback: @escaping (Data) -> Void)

    // OTA via BLE DFU
    func startDFU(_ peripheralId: String, firmware: URL,
                  onProgress: @escaping (Int) -> Void,
                  onComplete: @escaping (Bool, String?) -> Void)
}
```

### WiFiBridge

```swift
@objc class WiFiBridge: NSObject {
    // mDNS Discovery
    func browseMDNS(service: String)  // "_zacus._tcp"
    func onServiceFound(_ callback: @escaping ([String: Any]) -> Void)

    // WebSocket
    func connectWS(_ url: String) -> Bool
    func sendWS(_ data: Data)
    func onWSMessage(_ callback: @escaping (Data) -> Void)
    func disconnectWS()

    // HTTP (for OTA)
    func httpRequest(url: String, method: String, body: Data?,
                     headers: [String: String],
                     onProgress: @escaping (Int) -> Void) async -> (Int, Data?)

    // Network info
    func getLocalIP() -> String?
    func getSSID() -> String?
}
```

## IPC Protocol (Main ↔ Renderer)

```typescript
// === Serial ===
ipcRenderer.invoke('serial:list')           → SerialPort[]
ipcRenderer.invoke('serial:connect', {port, baud})
ipcRenderer.invoke('serial:write', {port, data})
ipcRenderer.invoke('serial:flash', {port, firmwarePath, onProgress})
ipcMain.on('serial:data', (e, {port, data}))
ipcMain.on('serial:plugged', (e, port))
ipcMain.on('serial:unplugged', (e, port))

// === BLE ===
ipcRenderer.invoke('ble:scan')
ipcRenderer.invoke('ble:connect', deviceId)
ipcRenderer.invoke('ble:write', {deviceId, characteristic, data})
ipcRenderer.invoke('ble:dfu', {deviceId, firmwarePath, onProgress})
ipcMain.on('ble:discovered', (e, device))
ipcMain.on('ble:data', (e, {deviceId, characteristic, data}))

// === WiFi ===
ipcRenderer.invoke('wifi:discover')         → ZacusDevice[]
ipcRenderer.invoke('wifi:ws-connect', url)
ipcRenderer.invoke('wifi:ws-send', data)
ipcRenderer.invoke('wifi:http', {url, method, body, headers})
ipcMain.on('wifi:ws-message', (e, data))
ipcMain.on('wifi:service-found', (e, service))

// === OTA ===
ipcRenderer.invoke('ota:check', deviceId)   → {current, available, needsUpdate}
ipcRenderer.invoke('ota:update', {deviceId, method, firmwarePath})
ipcRenderer.invoke('ota:rollback', deviceId)
ipcMain.on('ota:progress', (e, {deviceId, percent}))
ipcMain.on('ota:complete', (e, {deviceId, success, error}))

// === Files ===
ipcRenderer.invoke('file:open', {filters})  → filePath
ipcRenderer.invoke('file:save', {data, defaultPath})
ipcRenderer.invoke('file:recent')           → filePath[]
```

## Build & Distribution

| Item | Value |
|------|-------|
| Framework | Electron 33+ |
| Node | 22 LTS |
| Swift | 6.0+ (for native modules) |
| Target | macOS 14+ (Sonoma) |
| Architectures | Universal (arm64 + x86_64) |
| Signing | Apple Developer ID |
| Notarization | Required for distribution |
| Auto-update | electron-updater via GitHub Releases |
| Bundle ID | `fr.lelectronrare.zacus-studio` |
| App name | Zacus Studio |
| Icon | Professor Zacus character (to design) |
| Size target | < 200MB (.dmg) |

### Build commands

```bash
# Development
cd desktop/
npm run dev              # Electron dev mode with hot reload

# Production
npm run build:mac        # Universal binary .dmg
npm run build:mac-arm64  # Apple Silicon only
npm run notarize         # Apple notarization

# Native modules
cd native/
swift build              # Build Swift modules
npm run rebuild-native   # Rebuild N-API bindings
```

## Repository

New directory in existing Zacus repo:
```
le-mystere-professeur-zacus/
├── desktop/                    ← Zacus Studio
│   ├── package.json
│   ├── electron-builder.yml
│   ├── src/
│   │   ├── main/              ← Electron main process
│   │   │   ├── index.ts
│   │   │   ├── ipc-handlers.ts
│   │   │   ├── ota-manager.ts
│   │   │   ├── menu.ts
│   │   │   └── auto-updater.ts
│   │   ├── preload/
│   │   │   └── index.ts       ← contextBridge API
│   │   ├── renderer/
│   │   │   ├── App.tsx        ← 4-tab layout
│   │   │   ├── tabs/
│   │   │   │   ├── EditorTab.tsx    ← loads editor app
│   │   │   │   ├── DashboardTab.tsx ← loads dashboard app
│   │   │   │   ├── SimulationTab.tsx ← loads simulation app
│   │   │   │   └── DevToolsTab.tsx  ← new
│   │   │   └── devtools/
│   │   │       ├── DeviceManager.tsx
│   │   │       ├── SerialMonitor.tsx
│   │   │       ├── FirmwareManager.tsx
│   │   │       ├── NvsConfigurator.tsx
│   │   │       ├── MeshDiagnostic.tsx
│   │   │       ├── BatteryDashboard.tsx
│   │   │       └── LogRecorder.tsx
│   │   └── native/            ← Swift N-API modules
│   │       ├── Package.swift
│   │       ├── SerialBridge.swift
│   │       ├── BLEBridge.swift
│   │       ├── WiFiBridge.swift
│   │       └── binding.gyp
│   └── resources/
│       ├── icon.icns
│       └── entitlements.plist
├── frontend-v3/               ← existing monorepo (imported as dependency)
└── ESP32_ZACUS/               ← firmware (built locally, pushed via OTA)
```

## Implementation Phases

| Phase | Scope | Est. Hours |
|-------|-------|-----------|
| 1 | Electron scaffold + 4 tabs + menu + file handling | 10h |
| 2 | Swift SerialBridge + IPC + Serial Monitor | 15h |
| 3 | Swift BLEBridge + device discovery + pairing | 12h |
| 4 | Swift WiFiBridge + mDNS + WebSocket | 8h |
| 5 | OTA Manager (WiFi + BLE + USB) + Firmware Manager UI | 20h |
| 6 | Dev Tools UI (NVS config, mesh diagnostic, battery, log) | 15h |
| 7 | Integration with frontend-v3 tabs | 8h |
| 8 | Build pipeline (electron-builder, signing, notarization) | 8h |
| 9 | ESP32 OTA server firmware component | 10h |
| **Total** | | **~106h** |

## ESP32 OTA Server Component

Shared ESP-IDF component added to all puzzle firmwares:

```
ESP32_ZACUS/components/ota_server/
├── include/ota_server.h
├── ota_server.c
└── CMakeLists.txt
```

### API

```c
// Initialize OTA HTTP server on port 80
esp_err_t ota_server_init(void);

// Endpoints registered:
// GET  /version    → firmware name + version + IDF version
// GET  /status     → battery, uptime, heap, ESP-NOW peers
// POST /ota        → receive .bin, write to OTA partition, reboot
// GET  /ota/status → download progress
// POST /ota/rollback → revert to previous partition

// Version info (set at compile time)
#define OTA_FIRMWARE_NAME "p1_son"
#define OTA_FIRMWARE_VERSION "1.0.0"
```

### Safety

- OTA partition scheme: factory + ota_0 + ota_1
- `esp_ota_mark_app_valid_cancel_rollback()` after successful boot
- If new firmware crashes within 30s → auto-rollback to previous
- SHA256 verification of received .bin before writing
- Rate limit: 1 OTA update per 60s (prevent accidental double-flash)
