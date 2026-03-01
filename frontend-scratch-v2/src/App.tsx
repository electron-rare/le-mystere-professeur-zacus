import { useState } from 'react';
import Editor from '@monaco-editor/react';
import { BlocklyDesigner } from './components/BlocklyDesigner';
import { RuntimeControls } from './components/RuntimeControls';
import './App.css';

function App() {
  const [yaml, setYaml] = useState('');

  return (
    <div className="app-shell">
      <header className="app-header">
        <h1>Zacus Scratch Frontend V2</h1>
        <p>Nouvelle base from scratch, orientee Blockly + YAML + API Story V2.</p>
      </header>

      <main className="app-grid">
        <section className="panel">
          <h2>Designer blocs</h2>
          <BlocklyDesigner onYamlChange={setYaml} />
        </section>

        <section className="panel">
          <h2>YAML runtime</h2>
          <Editor
            height="40vh"
            defaultLanguage="yaml"
            value={yaml}
            onChange={(value) => setYaml(value ?? '')}
            options={{
              minimap: { enabled: false },
              scrollBeyondLastLine: false,
              fontSize: 13,
            }}
          />
          <RuntimeControls yaml={yaml} />
        </section>
      </main>
    </div>
  );
}

export default App;
