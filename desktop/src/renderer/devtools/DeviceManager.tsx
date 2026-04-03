import React, { useState, useEffect, useCallback } from 'react';

const CONNECTION_ICONS: Record<ZacusDevice['connectionType'], string> = {
  usb:  '🔌',
  ble:  '📶',
  wifi: '📡',
};

export function DeviceManager(): React.JSX.Element {
  const [devices, setDevices] = useState<ZacusDevice[]>([]);
  const [scanning, setScanning] = useState(false);
  const [selectedId, setSelectedId] = useState<string | null>(null);

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

  // BLE discovered — merge into device list
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

  // WiFi mDNS discovered — merge into device list
  useEffect(() => {
    window.zacus.wifi.onServiceFound(service => {
      setDevices(prev => {
        const existing = prev.findIndex(d => d.id === service.id);
        if (existing >= 0) {
          const copy = [...prev];
          copy[existing] = { ...copy[existing], ...service };
          return copy;
        }
        return [...prev, service];
      });
    });
  }, []);

  useEffect(() => { scan(); }, [scan]);

  const handleConnect = useCallback(async (device: ZacusDevice) => {
    setSelectedId(device.id);
    if (device.connectionType === 'wifi' && device.ip) {
      await window.zacus.wifi.wsConnect(`ws://${device.ip}/ws`);
    } else if (device.connectionType === 'ble') {
      await window.zacus.ble.connect(device.id);
    } else if (device.connectionType === 'usb') {
      await window.zacus.serial.connect(device.id, 115200);
    }
  }, []);

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
          <div className="empty-state">
            No devices found. Connect USB or enable WiFi.
          </div>
        )}
        {devices.map(device => (
          <div
            key={device.id}
            className={`device-card ${selectedId === device.id ? 'selected' : ''}`}
            onClick={() => setSelectedId(device.id)}
          >
            <div className="device-header">
              <span className="device-icon">{CONNECTION_ICONS[device.connectionType]}</span>
              <span className="device-name">{device.name}</span>
              <span className="device-type badge">{device.type}</span>
            </div>
            <div className="device-details">
              <span className="text-dim">fw v{device.firmwareVersion}</span>
              {device.batteryPct >= 0 && (
                <span className={device.batteryPct < 20 ? 'warn' : 'text-dim'}>
                  🔋 {device.batteryPct}%
                </span>
              )}
              {device.ip && (
                <span className="text-dim">📡 {device.ip}</span>
              )}
              {device.mac && (
                <span className="text-dim mono">{device.mac}</span>
              )}
            </div>
            <div className="device-meta">
              <span className="text-dim">
                Last seen {new Date(device.lastSeen).toLocaleTimeString()}
              </span>
            </div>
            <div className="device-actions">
              <button
                className="btn small primary"
                onClick={e => { e.stopPropagation(); handleConnect(device); }}
              >
                Connect
              </button>
              {device.connectionType === 'ble' && (
                <button
                  className="btn small"
                  onClick={e => { e.stopPropagation(); window.zacus.ble.disconnect(device.id); }}
                >
                  Disconnect
                </button>
              )}
            </div>
          </div>
        ))}
      </div>
    </div>
  );
}
