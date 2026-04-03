import React, { useState, useEffect, useCallback } from 'react';

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

  const buildFromSource = useCallback(async () => {
    setBuilding(true);
    try {
      await window.zacus.build.run();
    } finally {
      setBuilding(false);
    }
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
          <button className="btn" onClick={buildFromSource} disabled={building}>
            {building ? 'Building...' : 'Build from Source'}
          </button>
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
          {devices.length === 0 && (
            <tr>
              <td colSpan={6} className="empty-state">
                No devices connected. Scan for devices in the Dashboard tab.
              </td>
            </tr>
          )}
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
