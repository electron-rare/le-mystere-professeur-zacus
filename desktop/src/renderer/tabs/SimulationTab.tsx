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
        allowpopups={'false' as unknown as boolean}
        nodeintegration={'false' as unknown as boolean}
        partition="persist:simulation"
      />
    </div>
  );
}
