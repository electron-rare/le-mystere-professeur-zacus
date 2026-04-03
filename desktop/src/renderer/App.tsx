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
  { id: 'editor',     label: 'Editor',      icon: '\u2B21', shortcut: '\u2318 1' },
  { id: 'dashboard',  label: 'Game Master', icon: '\u25CE', shortcut: '\u2318 2' },
  { id: 'simulation', label: 'Simulation',  icon: '\u25B3', shortcut: '\u2318 3' },
  { id: 'devtools',   label: 'Dev Tools',   icon: '\u2699', shortcut: '\u2318 4' },
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
