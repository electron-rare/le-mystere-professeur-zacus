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
  { id: 'devices',  label: 'Devices',   icon: 'D' },
  { id: 'serial',   label: 'Serial',    icon: 'S' },
  { id: 'firmware', label: 'Firmware',  icon: 'F' },
  { id: 'nvs',      label: 'NVS',       icon: 'N' },
  { id: 'mesh',     label: 'Mesh',      icon: 'M' },
  { id: 'battery',  label: 'Battery',   icon: 'B' },
  { id: 'logs',     label: 'Logs',      icon: 'L' },
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
