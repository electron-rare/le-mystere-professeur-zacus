import { useCallback, useRef, useState } from 'react';
import type { RefObject } from 'react';
import { BlocklyWorkspace } from './BlocklyWorkspace.js';
import type { BlocklyWorkspaceHandle } from './BlocklyWorkspace.js';
import { YamlPreview } from './YamlPreview.js';
import { ValidationConsole } from './ValidationConsole.js';
import { exportWorkspaceToYaml } from '../lib/yaml-export.js';
import { importYamlToWorkspace } from '../lib/yaml-import.js';
import { validateScenario } from '../lib/validator.js';
import { parseScenarioYaml } from '@zacus/shared';
import type { ValidationResult } from '../lib/validator.js';
import { registerAllBlocks } from '../blocks/index.js';

// Register all blocks once at module level
registerAllBlocks();

export function EditorLayout() {
  const workspaceRef = useRef<BlocklyWorkspaceHandle>(null) as RefObject<BlocklyWorkspaceHandle>;
  const [yamlPreview, setYamlPreview] = useState('');
  const [validation, setValidation] = useState<ValidationResult | null>(null);

  const handleValidate = useCallback(() => {
    const ws = workspaceRef.current?.getWorkspace();
    if (!ws) return;
    try {
      const yaml = exportWorkspaceToYaml(ws);
      const scenario = parseScenarioYaml(yaml);
      const result = validateScenario(scenario);
      setValidation(result);
      setYamlPreview(yaml);
    } catch (e) {
      setValidation({
        valid: false,
        errors: [String(e)],
        warnings: [],
        summary: 'Erreur de génération YAML',
      });
    }
  }, []);

  const handleExport = useCallback(() => {
    const ws = workspaceRef.current?.getWorkspace();
    if (!ws) return;
    try {
      const yaml = exportWorkspaceToYaml(ws);
      const blob = new Blob([yaml], { type: 'text/yaml' });
      const url = URL.createObjectURL(blob);
      const a = document.createElement('a');
      a.href = url;
      a.download = 'zacus_v3_complete.yaml';
      a.click();
      URL.revokeObjectURL(url);
    } catch (e) {
      console.error('Export failed:', e);
    }
  }, []);

  const handleImport = useCallback(() => {
    const input = document.createElement('input');
    input.type = 'file';
    input.accept = '.yaml,.yml';
    input.onchange = async () => {
      const file = input.files?.[0];
      const ws = workspaceRef.current?.getWorkspace();
      if (!file || !ws) return;
      try {
        const text = await file.text();
        importYamlToWorkspace(ws, text);
        handleValidate();
      } catch (e) {
        setValidation({
          valid: false,
          errors: [`Import échoué: ${String(e)}`],
          warnings: [],
          summary: 'Erreur d\'import YAML',
        });
      }
    };
    input.click();
  }, [handleValidate]);

  return (
    <div style={styles.root}>
      {/* Menu Bar */}
      <header style={styles.header}>
        <span style={styles.title}>Zacus V3 Editor</span>
        <nav style={styles.nav}>
          <button onClick={handleImport} style={{ ...styles.btn, ...styles.btnSecondary }}>
            Importer YAML
          </button>
          <button onClick={handleValidate} style={{ ...styles.btn, ...styles.btnSecondary }}>
            Valider
          </button>
          <button onClick={handleExport} style={{ ...styles.btn, ...styles.btnPrimary }}>
            Exporter YAML
          </button>
        </nav>
      </header>

      {/* Main area */}
      <div style={styles.main}>
        {/* Blockly Workspace */}
        <div style={styles.workspaceContainer}>
          <BlocklyWorkspace ref={workspaceRef} className="flex-1" />
        </div>

        {/* YAML Preview panel */}
        <aside style={styles.sidebar}>
          <div style={styles.sidebarHeader}>Aperçu YAML</div>
          <YamlPreview yaml={yamlPreview} />
        </aside>
      </div>

      {/* Console Bar */}
      <footer style={styles.footer}>
        <ValidationConsole result={validation} />
      </footer>
    </div>
  );
}

const styles = {
  root: {
    display: 'flex',
    flexDirection: 'column' as const,
    height: '100vh',
    width: '100%',
    backgroundColor: '#1c1c1e',
    color: '#fff',
    overflow: 'hidden',
  },
  header: {
    display: 'flex',
    alignItems: 'center',
    gap: 16,
    padding: '0 16px',
    height: 48,
    backgroundColor: '#2c2c2e',
    borderBottom: '1px solid rgba(255,255,255,0.1)',
    flexShrink: 0,
  },
  title: {
    fontWeight: 600,
    fontSize: 14,
    color: '#fff',
  },
  nav: {
    display: 'flex',
    gap: 8,
    marginLeft: 'auto',
  },
  btn: {
    border: 'none',
    borderRadius: 8,
    padding: '6px 12px',
    fontSize: 12,
    fontWeight: 500,
    cursor: 'pointer',
  },
  btnPrimary: {
    backgroundColor: '#0071e3',
    color: '#fff',
  },
  btnSecondary: {
    backgroundColor: 'rgba(255,255,255,0.1)',
    color: '#fff',
  },
  main: {
    display: 'flex',
    flex: 1,
    minHeight: 0,
    overflow: 'hidden',
  },
  workspaceContainer: {
    flex: 1,
    overflow: 'hidden',
    position: 'relative' as const,
  },
  sidebar: {
    width: 320,
    display: 'flex',
    flexDirection: 'column' as const,
    borderLeft: '1px solid rgba(255,255,255,0.1)',
    backgroundColor: '#2c2c2e',
    overflow: 'hidden',
  },
  sidebarHeader: {
    padding: '8px 12px',
    fontSize: 11,
    fontWeight: 500,
    color: 'rgba(255,255,255,0.6)',
    borderBottom: '1px solid rgba(255,255,255,0.1)',
    textTransform: 'uppercase' as const,
    letterSpacing: '0.05em',
    flexShrink: 0,
  },
  footer: {
    height: 32,
    padding: '0 16px',
    backgroundColor: '#2c2c2e',
    borderTop: '1px solid rgba(255,255,255,0.1)',
    flexShrink: 0,
    overflow: 'hidden',
  },
};
