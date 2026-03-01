import React, { useState, useEffect } from 'react';
import './App.css';
import ScenarioSelector from './components/ScenarioSelector';
import LiveOrchestrator from './components/LiveOrchestrator';
import StoryDesigner from './components/StoryDesigner';

const THEME_KEY = 'story_ui_theme';

function App() {
  const [dark, setDark] = useState(() => {
    const saved = localStorage.getItem(THEME_KEY);
    if (saved) return saved === 'dark';
    return window.matchMedia('(prefers-color-scheme: dark)').matches;
  });

  useEffect(() => {
    document.body.classList.toggle('theme-dark', dark);
    localStorage.setItem(THEME_KEY, dark ? 'dark' : 'light');
  }, [dark]);

  return (
    <>
      <header style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', maxWidth: 900, margin: '0 auto', padding: '16px 0' }}>
        <h1 style={{ margin: 0, fontSize: 24 }}>Story V2 WebUI</h1>
        <button onClick={() => setDark((d) => !d)} aria-label="Basculer thème clair/sombre">
          {dark ? '☀️ Thème clair' : '🌙 Thème sombre'}
        </button>
      </header>
      <ScenarioSelector />
      <LiveOrchestrator />
      <StoryDesigner />
    </>
  );
}

export default App;
